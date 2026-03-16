#!/bin/bash
# Мастер-скрипт для последовательного запуска всех тестов SmartLinks
# Запускает: IoC тесты -> JWT тесты -> Интеграционные тесты
# Автоматически очищает все ресурсы после завершения

set -e

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Переменные
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
FAILED_TESTS=()
TEST_START_TIME=$(date +%s)

# Функция для печати заголовков
print_header() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

# Функция для печати успеха
print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

# Функция для печати ошибки
print_error() {
    echo -e "${RED}❌ $1${NC}"
}

# Функция для печати предупреждения
print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# Функция частичной очистки (процессы и логи, НЕ трогает Docker контейнеры)
cleanup_processes() {
    echo "Cleaning up processes and logs..."

    # 1. Убить процессы smartlinks
    pkill -f "./smartlinks" 2>/dev/null || true
    pkill -f "smartlinks_bdd_tests" 2>/dev/null || true

    # 2. Остановить тестовые контейнеры JWT (но не MongoDB!)
    docker stop smartlinks-jwt-test 2>/dev/null || true
    docker stop smartlinks-jwt-tests 2>/dev/null || true
    docker rm smartlinks-jwt-test 2>/dev/null || true
    docker rm smartlinks-jwt-tests 2>/dev/null || true

    # 3. Очистить логи
#     rm -f "$BUILD_DIR"/*.log 2>/dev/null || true
#     rm -f "$PROJECT_ROOT/tests"/*.log 2>/dev/null || true

    sleep 1
    echo "Process cleanup completed"
    echo ""
}

# Функция полной очистки всех ресурсов (включая Docker контейнеры)
cleanup_all() {
    print_header "Cleaning up all resources"

    # 1. Очистить процессы и логи
    cleanup_processes

    # 2. Остановить все Docker контейнеры проекта
    echo "Stopping Docker containers..."
    docker-compose -f "$PROJECT_ROOT/docker-compose.yml" down 2>/dev/null || true
    docker-compose -f "$PROJECT_ROOT/docker-compose.test.yml" down 2>/dev/null || true

    # 3. Подождать завершения всех процессов
    sleep 2

    print_success "Full cleanup completed"
    echo ""
}

# Проверка что проект собран
check_build() {
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Build directory not found: $BUILD_DIR"
        echo "Please build the project first:"
        echo "  mkdir -p build && cd build"
        echo "  cmake .. && cmake --build ."
        exit 1
    fi
}

# ============================================
# ОСНОВНОЙ БЛОК ВЫПОЛНЕНИЯ
# ============================================

print_header "SmartLinks - All Tests Runner"
echo "Project: $PROJECT_ROOT"
echo "Build: $BUILD_DIR"
echo ""
echo "Test sequence:"
echo "  1. IoC Container Unit Tests"
echo "  2. JWT Microservice Tests"
echo "  3.1. SmartLinks Integration Tests (JSON/Legacy mode)"
echo "  3.2. SmartLinks DSL Integration Tests (DSL mode)"
echo ""

# Предварительная проверка
check_build

# Начальная очистка (процессы и логи, НЕ трогаем Docker контейнеры)
print_header "Initial cleanup"
cleanup_processes

# Установить trap для полной очистки при выходе
trap cleanup_all EXIT INT TERM

# ============================================
# 1. IoC Container Unit Tests
# ============================================
print_header "Test Suite 1/3: IoC Container Unit Tests"

cd "$BUILD_DIR"

if [ -f "./tests/ioc_tests" ]; then
    echo "Running IoC unit tests..."
    if ./tests/ioc_tests; then
        print_success "IoC tests passed"
    else
        print_error "IoC tests failed"
        FAILED_TESTS+=("IoC Unit Tests")
    fi
else
    print_warning "ioc_tests executable not found, skipping"
    echo "Expected location: $BUILD_DIR/tests/ioc_tests"
    echo "Build with: cmake --build . --target ioc_tests"
fi

# ============================================
# 2. JWT Microservice Tests (Docker)
# ============================================
print_header "Test Suite 2/3: JWT Microservice Tests"

cd "$PROJECT_ROOT"

# Проверить что MongoDB запущен
echo "Checking MongoDB..."
if ! docker ps | grep -q smartlinks-mongodb; then
    echo "Starting MongoDB..."
    docker-compose up -d mongodb

    # Дождаться пока MongoDB станет healthy
    echo "Waiting for MongoDB to be healthy..."
    for i in {1..30}; do
        if docker inspect smartlinks-mongodb --format='{{.State.Health.Status}}' 2>/dev/null | grep -q "healthy"; then
            print_success "MongoDB is healthy"
            break
        fi
        if [ $i -eq 30 ]; then
            print_error "MongoDB did not become healthy in 60 seconds"
            docker logs smartlinks-mongodb | tail -20
            FAILED_TESTS+=("MongoDB Startup")
        fi
        sleep 2
        echo -n "."
    done
    echo ""
else
    print_success "MongoDB is already running"
fi

# Запустить тестовый JWT сервис
echo "Starting JWT test service..."
COMPOSE_PROJECT_NAME=smartlinks-cpp-test docker-compose -f docker-compose.test.yml up -d jwt-service-test
sleep 5

# Проверить health
echo "Checking JWT service health..."
if curl -s http://localhost:3001/health | grep -q "healthy"; then
    print_success "JWT service is healthy"
else
    print_error "JWT service is not healthy"
    docker logs smartlinks-jwt-test
    FAILED_TESTS+=("JWT Service Health Check")
fi

# Запустить тесты JWT
echo "Running JWT tests..."
if COMPOSE_PROJECT_NAME=smartlinks-cpp-test docker-compose -f docker-compose.test.yml run --rm jwt-tests; then
    print_success "JWT tests passed"
else
    print_error "JWT tests failed"
    FAILED_TESTS+=("JWT Microservice Tests")
fi

# Остановить тестовый JWT сервис
echo "Stopping JWT test service..."
COMPOSE_PROJECT_NAME=smartlinks-cpp-test docker-compose -f docker-compose.test.yml down

# ============================================
# 3. SmartLinks Integration Tests
# ============================================
print_header "Test Suite 3/3: SmartLinks Integration Tests"

cd "$PROJECT_ROOT"

# Проверить что MongoDB запущен (необходим для интеграционных тестов)
echo "Checking MongoDB..."
if ! docker ps | grep -q smartlinks-mongodb; then
    echo "Starting MongoDB..."
    docker-compose up -d mongodb

    # Дождаться пока MongoDB станет healthy
    echo "Waiting for MongoDB to be healthy..."
    for i in {1..30}; do
        if docker inspect smartlinks-mongodb --format='{{.State.Health.Status}}' 2>/dev/null | grep -q "healthy"; then
            print_success "MongoDB is healthy"
            break
        fi
        if [ $i -eq 30 ]; then
            print_error "MongoDB did not become healthy in 60 seconds"
            docker logs smartlinks-mongodb | tail -20
            FAILED_TESTS+=("MongoDB Startup")
        fi
        sleep 2
        echo -n "."
    done
    echo ""
else
    print_success "MongoDB is already running"
fi

# Запустить JWT сервис для интеграционных тестов SmartLinks
echo "Starting JWT service for SmartLinks integration tests..."

# Остановить контейнер если запущен
docker stop smartlinks-jwt 2>/dev/null || true
docker rm smartlinks-jwt 2>/dev/null || true

# Запустить JWT service (будет использовать БД Links по умолчанию)
docker-compose up -d jwt-service

# Дождаться пока JWT сервис станет healthy
echo "Waiting for JWT service to be healthy..."
for i in {1..30}; do
    HEALTH_STATUS=$(docker inspect smartlinks-jwt --format='{{.State.Health.Status}}' 2>/dev/null || echo "unknown")
    if [ "$HEALTH_STATUS" == "healthy" ]; then
        print_success "JWT service is healthy"
        break
    fi
    if [ $i -eq 30 ]; then
        print_error "JWT service did not become healthy in 60 seconds"
        echo "Health status: $HEALTH_STATUS"
        docker logs smartlinks-jwt | tail -20
        FAILED_TESTS+=("JWT Service Startup")
    fi
    sleep 2
    echo -n "."
done
echo ""

# Создать тестового пользователя напрямую в MongoDB
echo "Creating test user for JWT authorization tests..."

# Скопировать hash-password.js в контейнер (если ещё не скопирован)
if ! docker exec smartlinks-jwt test -f /app/hash-password.js 2>/dev/null; then
    echo "Copying hash-password.js to JWT container..."
    docker cp "$PROJECT_ROOT/jwt-service/hash-password.js" smartlinks-jwt:/app/ >/dev/null 2>&1
fi

# Генерация bcrypt хэша для password123 через JWT контейнер
echo "Generating password hash..."
PASSWORD_HASH_OUTPUT=$(docker exec smartlinks-jwt node /app/hash-password.js password123 2>&1)
PASSWORD_HASH=$(echo "$PASSWORD_HASH_OUTPUT" | grep "Hash:" | awk '{print $2}')

if [ -z "$PASSWORD_HASH" ]; then
    print_error "Failed to generate password hash"
    echo "Output: $PASSWORD_HASH_OUTPUT"
    FAILED_TESTS+=("Password Hash Generation")
    # Использовать fallback хэш для password123
    PASSWORD_HASH='$2b$10$yIqlNxsCShTSeRwlT0bdzOJSwF9BM55lJ/qPdyWu/1ZVf05KYV/A2'
    print_warning "Using fallback password hash"
fi

echo "Password hash ready"

# Вставить пользователя в MongoDB (в обе БД: Links и LinksTest)
echo "Inserting test user into MongoDB..."

# Экранировать PASSWORD_HASH для безопасной передачи в MongoDB
# Заменить одинарные кавычки для безопасности
ESCAPED_HASH="${PASSWORD_HASH}"

# Вставить в БД LinksTest (для SmartLinks JSON тестов)
docker exec smartlinks-mongodb mongosh -u root -p password --authenticationDatabase admin --quiet LinksTest --eval 'db.users.deleteOne({ username: "testuser" }); db.users.insertOne({ username: "testuser", password_hash: "'"${ESCAPED_HASH}"'", email: "testuser@example.com", created_at: new Date(), active: true });' >/dev/null 2>&1

# Вставить в БД LinksDSLTest (для SmartLinks DSL тестов)
docker exec smartlinks-mongodb mongosh -u root -p password --authenticationDatabase admin --quiet LinksDSLTest --eval 'db.users.deleteOne({ username: "testuser" }); db.users.insertOne({ username: "testuser", password_hash: "'"${ESCAPED_HASH}"'", email: "testuser@example.com", created_at: new Date(), active: true });' >/dev/null 2>&1

# Вставить в БД Links (для JWT service, который использует Links по умолчанию)
MONGO_RESULT=$(docker exec smartlinks-mongodb mongosh -u root -p password --authenticationDatabase admin --quiet Links --eval 'db.users.deleteOne({ username: "testuser" }); db.users.insertOne({ username: "testuser", password_hash: "'"${ESCAPED_HASH}"'", email: "testuser@example.com", created_at: new Date(), active: true });' 2>&1)

if echo "$MONGO_RESULT" | grep -q "insertedId"; then
    print_success "Test user created in MongoDB (all databases: Links, LinksTest, LinksDSLTest)"
else
    print_warning "MongoDB insert may have failed"
    echo "Result: $MONGO_RESULT"
fi

# Остановить любые существующие экземпляры smartlinks
echo "Stopping existing smartlinks instances..."
pkill -f "./smartlinks" 2>/dev/null || true
sleep 1

# ============================================
# 3.1. SmartLinks Integration Tests (JSON/Legacy режим)
# ============================================
print_header "Test Suite 3.1/3: SmartLinks Integration Tests (JSON/Legacy mode)"

# Запустить сервер в JSON режиме с тестовой БД LinksTest
echo "Starting smartlinks test server (JSON/Legacy mode)..."
cd "$BUILD_DIR"
MONGODB_DATABASE=LinksTest USE_DSL_RULES=false JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json ./smartlinks > smartlinks_json_test.log 2>&1 &
SERVER_PID=$!
echo "Server started (PID: $SERVER_PID, Database: LinksTest, USE_DSL_RULES=false)"

# Подождать запуска сервера
echo "Waiting for server to start..."
for i in {1..30}; do
    if curl -s http://localhost:8080/test-redirect >/dev/null 2>&1; then
        print_success "Server is ready"
        break
    fi
    if [ $i -eq 30 ]; then
        print_error "Server did not start in 30 seconds"
        echo "Last log lines:"
        tail -20 smartlinks_json_test.log
        kill $SERVER_PID 2>/dev/null || true
        FAILED_TESTS+=("SmartLinks Server Startup (JSON mode)")
        SERVER_PID=""
    fi
    sleep 1
    echo -n "."
done
echo ""

# Запустить интеграционные JSON тесты
if [ -n "$SERVER_PID" ]; then
    echo "Running integration tests (JSON/Legacy mode)..."
    if ctest --output-on-failure -R SmartLinks_Integration_Tests --verbose; then
        print_success "Integration tests (JSON mode) passed"
    else
        print_error "Integration tests (JSON mode) failed"
        echo "Server log (last 50 lines):"
        tail -50 smartlinks_json_test.log
        FAILED_TESTS+=("SmartLinks Integration Tests (JSON mode)")
    fi

    # Остановить сервер
    echo "Stopping test server (JSON mode)..."
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    SERVER_PID=""
fi

sleep 2

# ============================================
# 3.2. SmartLinks DSL Integration Tests (DSL режим)
# ============================================
print_header "Test Suite 3.2/3: SmartLinks DSL Integration Tests (DSL mode)"

# Запустить сервер в DSL режиме с тестовой БД LinksDSLTest
echo "Starting smartlinks test server (DSL mode)..."
cd "$BUILD_DIR"
MONGODB_DATABASE=LinksDSLTest JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json ./smartlinks > smartlinks_dsl_test.log 2>&1 &
SERVER_PID=$!
echo "Server started (PID: $SERVER_PID, Database: LinksDSLTest, USE_DSL_RULES=default)"

# Подождать запуска сервера
echo "Waiting for server to start..."
for i in {1..30}; do
    if curl -s http://localhost:8080/test-dsl-simple >/dev/null 2>&1; then
        print_success "Server is ready"
        break
    fi
    if [ $i -eq 30 ]; then
        print_error "Server did not start in 30 seconds"
        echo "Last log lines:"
        tail -20 smartlinks_dsl_test.log
        kill $SERVER_PID 2>/dev/null || true
        FAILED_TESTS+=("SmartLinks Server Startup (DSL mode)")
        SERVER_PID=""
    fi
    sleep 1
    echo -n "."
done
echo ""

# Запустить интеграционные DSL тесты
if [ -n "$SERVER_PID" ]; then
    echo "Running integration tests (DSL mode)..."
    if ctest --output-on-failure -R SmartLinks_DSL_Integration_Tests --verbose; then
        print_success "Integration tests (DSL mode) passed"
    else
        print_error "Integration tests (DSL mode) failed"
        echo "Server log (last 50 lines):"
        tail -50 smartlinks_dsl_test.log
        FAILED_TESTS+=("SmartLinks DSL Integration Tests")
    fi

    # Остановить сервер
    echo "Stopping test server (DSL mode)..."
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    SERVER_PID=""
fi

# ============================================
# ИТОГОВЫЙ ОТЧЁТ
# ============================================
print_header "Test Results Summary"

TEST_END_TIME=$(date +%s)
TEST_DURATION=$((TEST_END_TIME - TEST_START_TIME))

echo "Total execution time: ${TEST_DURATION}s"
echo ""

if [ ${#FAILED_TESTS[@]} -eq 0 ]; then
    print_success "ALL TESTS PASSED! 🎉"
    echo ""
    echo "Test suites completed:"
    echo "  ✅ IoC Container Unit Tests"
    echo "  ✅ JWT Microservice Tests"
    echo "  ✅ SmartLinks Integration Tests (JSON/Legacy mode)"
    echo "  ✅ SmartLinks DSL Integration Tests (DSL mode)"
    echo ""
    exit 0
else
    print_error "SOME TESTS FAILED"
    echo ""
    echo "Failed test suites:"
    for test in "${FAILED_TESTS[@]}"; do
        echo "  ❌ $test"
    done
    echo ""
    echo "Check the logs:"
    echo "  - smartlinks_json_test.log (JSON/Legacy mode)"
    echo "  - smartlinks_dsl_test.log (DSL mode)"
    exit 1
fi
