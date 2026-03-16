#!/bin/bash
# Скрипт для удобного запуска тестов JWT сервиса

set -e

echo "🧪 JWT Service Tests Runner"
echo "=========================="
echo ""

# Проверить что MongoDB запущен
echo "📦 Checking MongoDB..."
if ! docker ps | grep -q smartlinks-mongodb; then
    echo "❌ MongoDB is not running. Starting..."
    docker-compose up -d mongodb
    sleep 5
fi
echo "✅ MongoDB is running"
echo ""

# Запустить тестовый JWT сервис
echo "🚀 Starting test JWT service..."
docker-compose -f docker-compose.test.yml up -d jwt-service-test
sleep 5

# Проверить health
echo "🏥 Checking JWT service health..."
if curl -s http://localhost:3001/health | grep -q "healthy"; then
    echo "✅ JWT service is healthy"
else
    echo "❌ JWT service is not healthy"
    docker logs smartlinks-jwt-test
    exit 1
fi
echo ""

# Запустить тесты
echo "🧪 Running tests..."
echo ""
docker-compose -f docker-compose.test.yml run --rm jwt-tests "$@"

# Сохранить exit code
TEST_EXIT_CODE=$?

echo ""
if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo "✅ All tests passed!"
else
    echo "❌ Tests failed with exit code $TEST_EXIT_CODE"
fi

exit $TEST_EXIT_CODE
