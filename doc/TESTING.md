# SmartLinks - Testing Guide

Документация по тестированию проекта SmartLinks.

## 📋 Оглавление

- [Общая информация](#общая-информация)
- [Быстрый старт](#быстрый-старт)
- [Тестовые наборы](#тестовые-наборы)
- [Запуск отдельных тестов](#запуск-отдельных-тестов)
- [Тестовое окружение](#тестовое-окружение)
- [Troubleshooting](#troubleshooting)

---

## Общая информация

Проект SmartLinks включает четыре основных тестовых набора:

1. **IoC Container Unit Tests** - модульные тесты IoC-контейнера (C++, GoogleTest) - 20 тестов
2. **JWT Microservice Tests** - интеграционные тесты JWT сервиса (Node.js, Jest) - 40 тестов
3. **SmartLinks Integration Tests (JSON/Legacy Mode)** - интеграционные тесты основного приложения в режиме JSON (C++, GoogleTest) - 17 тестов
4. **SmartLinks DSL Integration Tests (DSL Mode)** - интеграционные тесты основного приложения в режиме DSL (C++, GoogleTest) - 18 тестов

### Технологии

| Компонент | Фреймворк | Язык | Окружение |
|-----------|-----------|------|-----------|
| IoC Tests | GoogleTest | C++ | Native |
| JWT Tests | Jest | JavaScript | Docker |
| Integration Tests | GoogleTest | C++ | Native + MongoDB |

---

## Быстрый старт

### Запуск ВСЕХ тестов

Используйте мастер-скрипт для запуска всех тестов последовательно:

```bash
./run_all_tests.sh
```

**Что делает скрипт:**
1. ✅ Запускает IoC unit тесты (20 тестов)
2. ✅ Запускает JWT микросервис тесты (40 тестов, в Docker)
3. ✅ Запускает SmartLinks интеграционные тесты - JSON/Legacy mode (17 тестов)
4. ✅ Запускает SmartLinks DSL интеграционные тесты - DSL mode (18 тестов)
5. ✅ Автоматически очищает все ресурсы (контейнеры, процессы, логи)

**Пример вывода:**

```
========================================
SmartLinks - All Tests Runner
========================================

Test sequence:
  1. IoC Container Unit Tests
  2. JWT Microservice Tests
  3.1. SmartLinks Integration Tests (JSON/Legacy mode)
  3.2. SmartLinks DSL Integration Tests (DSL mode)

========================================
Test Suite 1/4: IoC Container Unit Tests
========================================
Running IoC unit tests...
✅ IoC tests passed (20 tests)

========================================
Test Suite 2/4: JWT Microservice Tests
========================================
Starting MongoDB...
✅ MongoDB is healthy
Starting JWT test service...
✅ JWT service is healthy
Running JWT tests...
Test Suites: 4 passed, 4 total
Tests:       40 passed, 40 total
✅ JWT tests passed

========================================
Test Suite 3.1/4: SmartLinks Integration Tests (JSON/Legacy mode)
========================================
Starting smartlinks test server (JSON/Legacy mode)...
Server started (PID: 12345, Database: LinksTest, USE_DSL_RULES=false)
✅ Server is ready
Running integration tests (JSON/Legacy mode)...
✅ Integration tests (JSON mode) passed (17 tests)

========================================
Test Suite 3.2/4: SmartLinks DSL Integration Tests (DSL mode)
========================================
Starting smartlinks test server (DSL mode)...
Server started (PID: 12346, Database: LinksDSLTest, USE_DSL_RULES=default)
✅ Server is ready
Running integration tests (DSL mode)...
✅ Integration tests (DSL mode) passed (18 tests)

========================================
Test Results Summary
========================================
Total execution time: 60s

✅ ALL TESTS PASSED! 🎉

Test suites completed:
  ✅ IoC Container Unit Tests (20 tests)
  ✅ JWT Microservice Tests (40 tests)
  ✅ SmartLinks Integration Tests (JSON/Legacy mode, 17 tests)
  ✅ SmartLinks DSL Integration Tests (DSL mode, 18 tests)

Total: 95 tests passed
```

---

## Тестовые наборы

### 1. IoC Container Unit Tests

**Описание:** Модульные тесты для IoC (Inversion of Control) контейнера.

**Расположение:** `tests/unit/ioc_tests.cpp`

**Запуск:**

```bash
cd build
./ioc_tests
```

**Или через ctest:**

```bash
cd build
ctest -R IoC_Tests --output-on-failure
```

**Что тестируется:**
- Регистрация и резолв зависимостей
- Singleton и transient lifetime
- Circular dependencies detection
- Type safety

---

### 2. JWT Microservice Tests

**Описание:** Интеграционные тесты JWT микросервиса (Node.js).

**Расположение:** `tests/jwt/`

**Запуск:**

```bash
./tests/jwt/run-tests.sh
```

**Или напрямую через docker-compose:**

```bash
docker-compose -f docker-compose.test.yml run --rm jwt-tests
```

**Что тестируется:**
- `POST /auth/login` - аутентификация (10 тестов)
- `POST /auth/refresh` - обновление токенов (6 тестов)
- `POST /auth/revoke` - отзыв токенов (5 тестов)
- `POST /auth/introspect` - валидация токенов (7 тестов)
- `GET /.well-known/jwks.json` - JWKS endpoint (7 тестов)
- `GET /health` - health check (5 тестов)

**Итого:** 40 тестов

**Подробная документация:** [tests/jwt/README.md](tests/jwt/README.md)

---

### 3. SmartLinks Integration Tests (JSON/Legacy Mode)

**Описание:** Интеграционные тесты основного приложения SmartLinks в режиме JSON/Legacy.

**Расположение:** `tests/bdd/smartlinks_tests.cpp`

**Запуск:**

```bash
# Через run_all_tests.sh (рекомендуется)
./run_all_tests.sh

# Или напрямую
cd build
MONGODB_DATABASE=LinksTest USE_DSL_RULES=false ./smartlinks &
sleep 5
ctest -R SmartLinks_Integration_Tests --output-on-failure
```

**Что тестируется:**
- Method Not Allowed (405) - 5 тестов
- Not Found (404) - 3 теста
- Unprocessable Content (422) - 2 теста
- Temporary Redirect (307/429) - 7 тестов
- Правила редиректа (JSON формат)
- Интеграция с MongoDB

**Тестовая БД:** `LinksTest` (отдельная от production)

---

### 4. SmartLinks DSL Integration Tests (DSL Mode)

**Описание:** Интеграционные тесты основного приложения SmartLinks в режиме DSL.

**Расположение:** `tests/bdd/smartlinks_dsl_tests.cpp`

**Запуск:**

```bash
# Через run_all_tests.sh (рекомендуется)
./run_all_tests.sh

# Или напрямую
cd build
MONGODB_DATABASE=LinksDSLTest JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json ./smartlinks &
sleep 5
ctest -R SmartLinks_DSL_Integration_Tests --output-on-failure
```

**Что тестируется:**
- Method Not Allowed (405) - 5 тестов
- Not Found (404) - 3 теста
- Unprocessable Content (422) - 2 теста
- Temporary Redirect (307/429) с DSL правилами - 5 тестов
- DSL парсинг и выполнение:
  - `LANGUAGE = ru-RU -> url`
  - `DATETIME IN[start, end] -> url`
  - `AUTHORIZED -> url`
  - Множественные правила с fallback
  - Комбинированные условия (AND, OR)
- JWT авторизация через AUTHORIZED expression - 3 теста

**Тестовая БД:** `LinksDSLTest` (отдельная от LinksTest и production)

**ВАЖНО - JWT конфигурация:**
```bash
# Необходимо указывать полный путь к JWKS endpoint
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json
```

---

## Запуск отдельных тестов

### IoC тесты - конкретный тест

```bash
cd build
./ioc_tests --gtest_filter="IoCContainerTest.RegisterAndResolve"
```

### JWT тесты - конкретный файл

```bash
docker-compose -f docker-compose.test.yml run --rm jwt-tests npm test auth.test.js
```

### JWT тесты - конкретный тест

```bash
docker-compose -f docker-compose.test.yml run --rm jwt-tests npm test -- -t "should return tokens"
```

### Integration тесты - конкретный тест

```bash
cd build
./smartlinks_tests --gtest_filter="SmartLinksIntegrationTest.CreateShortLink"
```

---

## Тестовое окружение

### Используемые порты

| Сервис | Порт | Использование |
|--------|------|---------------|
| SmartLinks (production) | 8080 | Основное приложение |
| SmartLinks (test) | 8080 | Тесты интеграции |
| JWT Service (production) | 3000 | JWT endpoints |
| JWT Service (test) | 3001 | JWT тесты |
| MongoDB | 27017 | БД (Links и LinksTest) |
| Mongo Express | 8081 | Web UI для MongoDB |

### Базы данных

| База данных | Использование |
|-------------|---------------|
| `Links` | Production данные |
| `LinksTest` | JSON/Legacy mode тесты |
| `LinksDSLTest` | DSL mode тесты + JWT авторизация |

**ВАЖНО:** Тесты используют отдельные БД (`LinksTest`, `LinksDSLTest`), поэтому production данные не затрагиваются.

### Docker контейнеры

**Production (docker-compose.yml):**
- `smartlinks-mongodb` - MongoDB
- `smartlinks-jwt` - JWT сервис
- `smartlinks-app` - SmartLinks приложение
- `mexpress` - Mongo Express

**Test (docker-compose.test.yml):**
- `jwt-service-test` - JWT сервис для тестов (порт 3001)
- `jwt-tests` - Контейнер для запуска Jest тестов

---

## Переменные окружения для тестов

### SmartLinks Integration Tests (JSON/Legacy Mode)

```bash
MONGODB_DATABASE=LinksTest  # Использовать тестовую БД
USE_DSL_RULES=false  # Режим JSON/Legacy
MONGODB_URI=mongodb://root:password@localhost:27017  # MongoDB connection
```

### SmartLinks DSL Integration Tests (DSL Mode)

```bash
MONGODB_DATABASE=LinksDSLTest  # Использовать DSL тестовую БД
USE_DSL_RULES=true  # Режим DSL (по умолчанию)
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json  # ⚠️ ПОЛНЫЙ путь к JWKS
JWT_ISSUER=smartlinks-jwt-service  # JWT issuer (опционально)
JWT_AUDIENCE=smartlinks-api  # JWT audience (опционально)
MONGODB_URI=mongodb://root:password@localhost:27017  # MongoDB connection
```

### JWT Tests

```bash
TEST_MONGO_URI=mongodb://root:password@mongodb:27017  # MongoDB для тестов
JWT_SERVICE_URL=http://jwt-service-test:3000  # URL тестового JWT сервиса
```

---

## CI/CD интеграция

### GitHub Actions пример

```yaml
name: SmartLinks Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v3

      - name: Build project
        run: |
          mkdir build && cd build
          cmake ..
          cmake --build .

      - name: Run all tests
        run: ./run_all_tests.sh

      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: test-results
          path: |
            build/*.log
            tests/*.log
```

---

## Troubleshooting

### Проблема: "JWT validation failed" или "Failed to fetch JWKS"

**Причина:** Неполный или неправильный JWT_JWKS_URI

**Решение:**

```bash
# ❌ НЕПРАВИЛЬНО - отсутствует путь к JWKS
JWT_JWKS_URI=http://localhost:3000

# ✅ ПРАВИЛЬНО - полный путь
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json
```

**Проверка:**
```bash
# Проверить что JWKS endpoint доступен
curl http://localhost:3000/auth/.well-known/jwks.json

# Должен вернуть JSON с публичными ключами
{
  "keys": [
    {
      "kty": "RSA",
      "use": "sig",
      ...
    }
  ]
}
```

**Исправление в run_all_tests.sh:**
Скрипт уже исправлен и использует правильный полный путь:
```bash
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json ./smartlinks
```

---

### Проблема: "MongoDB connection failed"

**Решение:**

```bash
# Проверить что MongoDB запущен
docker-compose ps mongodb

# Если нет - запустить
docker-compose up -d mongodb

# Проверить логи
docker logs smartlinks-mongodb
```

---

### Проблема: "Port 8080 already in use"

**Решение:**

```bash
# Найти процесс на порту 8080
lsof -i :8080

# Убить процесс
kill <PID>

# Или убить все smartlinks процессы
pkill -f smartlinks
```

---

### Проблема: "JWT tests timeout"

**Решение:**

```bash
# Проверить что JWT сервис запущен
curl http://localhost:3001/health

# Если нет - перезапустить
docker-compose -f docker-compose.test.yml restart jwt-service-test

# Проверить логи
docker logs smartlinks-jwt-test
```

---

### Проблема: "ioc_tests not found"

**Решение:**

```bash
# Пересобрать тесты
cd build
cmake --build . --target ioc_tests

# Проверить что файл создан
ls -la ./ioc_tests
```

---

### Полная очистка всего окружения

Если что-то пошло не так и нужно очистить все:

```bash
# Остановить все Docker контейнеры
docker-compose down
docker-compose -f docker-compose.test.yml down

# Убить все процессы smartlinks
pkill -f smartlinks

# Удалить логи
rm -f build/*.log tests/*.log

# Удалить тестовую БД (опционально)
docker exec -it smartlinks-mongodb mongosh -u root -p password --eval "db.getSiblingDB('LinksTest').dropDatabase()"

# Полная очистка Docker (ОПАСНО - удалит все volumes)
docker-compose down -v
docker-compose -f docker-compose.test.yml down -v
```

---

## Coverage (опционально)

### JWT tests coverage

```bash
docker-compose -f docker-compose.test.yml run --rm jwt-tests npm run test:coverage

# Отчёт в tests/jwt/coverage/lcov-report/index.html
```

### C++ tests coverage (если настроено)

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
cmake --build .
ctest
gcovr -r .. --html --html-details -o coverage.html
```

---

## Дополнительные скрипты

| Скрипт | Описание |
|--------|----------|
| `run_all_tests.sh` | ⭐ МАСТЕР-СКРИПТ - запустить ВСЕ тесты (95 тестов) |
| `run_integration_tests.sh` | Запустить только интеграционные тесты SmartLinks (JSON mode) |
| `tests/jwt/run-tests.sh` | Запустить только JWT тесты (40 тестов) |

**Примечания:**
- DSL тесты запускаются только через `run_all_tests.sh` для обеспечения правильной конфигурации
- Для корректной работы DSL тестов необходим запущенный JWT сервис с правильной конфигурацией `JWT_JWKS_URI`

---

## Структура тестов

```
smartlinks-cpp/
├── run_all_tests.sh              # МАСТЕР-СКРИПТ (все тесты)
├── run_integration_tests.sh      # Интеграционные тесты
├── TESTING.md                    # Эта документация
│
├── tests/
│   ├── unit/
│   │   └── ioc_tests.cpp         # IoC unit тесты
│   │
│   ├── bdd/
│   │   └── smartlinks_tests.cpp  # SmartLinks интеграционные тесты
│   │
│   ├── jwt/
│   │   ├── auth.test.js          # JWT: тесты аутентификации
│   │   ├── introspect.test.js    # JWT: тесты introspection
│   │   ├── jwks.test.js          # JWT: тесты JWKS
│   │   ├── health.test.js        # JWT: тесты health
│   │   ├── setup.js              # JWT: настройка тестового окружения
│   │   ├── Dockerfile            # JWT: контейнер для тестов
│   │   ├── run-tests.sh          # JWT: скрипт запуска
│   │   ├── README.md             # JWT: документация
│   │   └── TEST_RESULTS.md       # JWT: результаты тестов
│   │
│   ├── run_bdd_tests.sh          # BDD тесты (Cucumber)
│   └── CMakeLists.txt            # Конфигурация C++ тестов
│
├── docker-compose.yml            # Production окружение
└── docker-compose.test.yml       # Test окружение (JWT)
```

---

## Метрики

### Последние результаты (2026-03-16)

| Тестовый набор | Тесты | Пройдено | Время |
|----------------|-------|----------|-------|
| IoC Unit Tests | 20 | ✅ 20 | ~0.2s |
| JWT Tests | 40 | ✅ 40 | ~5s |
| Integration Tests (JSON) | 17 | ✅ 17 | ~2s |
| Integration Tests (DSL) | 18 | ✅ 18 | ~3s |
| **ИТОГО** | **95** | **✅ 95** | **~10s** |

---

## Лицензия

MIT

---

**Tested by:** Claude Code
**Last updated:** 2026-03-15
**SmartLinks Version:** 1.0.0
