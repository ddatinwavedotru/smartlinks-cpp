# МАСТЕР-СКРИПТ ТЕСТИРОВАНИЯ
## SmartLinks Project

---

## ✅ СОЗДАННЫЕ ФАЙЛЫ

### 1. run_all_tests.sh (7.9 KB)
Мастер-скрипт для последовательного запуска всех тестов

### 2. TESTING.md (14 KB)
Полная документация по тестированию проекта

### 3. README.md (обновлён)
Добавлена секция о тестировании и мастер-скрипте

---

## 📋 ФУНКЦИОНАЛ МАСТЕР-СКРИПТА

### 1️⃣ ЗАПУСК ТЕСТОВ (последовательно)

- **IoC Container Unit Tests** (C++, GoogleTest)
- **JWT Microservice Tests** (Node.js, Jest, Docker)
- **SmartLinks Integration Tests** (C++, GoogleTest)

### 2️⃣ АВТОМАТИЧЕСКАЯ ОЧИСТКА

- Останавливает Docker контейнеры (production + test)
- Убивает процессы smartlinks и wireserver
- Удаляет лог-файлы (*.log)
- Освобождает порты (8080, 3000, 3001)

### 3️⃣ ОТЧЁТНОСТЬ

- Цветной вывод (зелёный = успех, красный = ошибка)
- Суммарное время выполнения
- Список проваленных тестов (если есть)
- Exit code (0 = success, 1 = failure)

### 4️⃣ ЗАЩИТА ОТ ОШИБОК

- `trap` для очистки при Ctrl+C
- Проверка зависимостей (build/, MongoDB)
- Логирование всех этапов

---

## 🚀 ИСПОЛЬЗОВАНИЕ

### Простой запуск

```bash
./run_all_tests.sh
```

### В CI/CD

```bash
./run_all_tests.sh 2>&1 | tee test-results.log
if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo "All tests passed, deploying..."
fi
```

---

## 📊 ТЕСТОВОЕ ПОКРЫТИЕ

| Тестовый набор | Тесты | Технология | Время |
|----------------|-------|------------|-------|
| IoC Unit Tests | 15 | GoogleTest | ~0.1s |
| JWT Microservice Tests | 40 | Jest | ~5s |
| SmartLinks Integration Tests | 12 | GoogleTest | ~2s |
| **ИТОГО** | **67** | **Mixed** | **~7s** |

---

## 📁 СТРУКТУРА ТЕСТОВЫХ ФАЙЛОВ

```
smartlinks-cpp/
├── run_all_tests.sh              ← МАСТЕР-СКРИПТ
├── run_integration_tests.sh      ← Только интеграционные
├── TESTING.md                    ← Полная документация
├── README.md                     ← Обновлён
│
├── tests/
│   ├── unit/
│   │   └── ioc_tests.cpp         (IoC тесты)
│   ├── bdd/
│   │   └── smartlinks_tests.cpp  (Интеграционные тесты)
│   ├── jwt/
│   │   ├── *.test.js             (40 JWT тестов)
│   │   ├── run-tests.sh          ← Только JWT тесты
│   │   └── README.md
│   └── run_bdd_tests.sh          ← BDD тесты (Cucumber)
│
├── docker-compose.yml            (Production)
└── docker-compose.test.yml       (Test)
```

---

## 📚 ДОКУМЕНТАЦИЯ

### 1. TESTING.md
- Общая информация о тестировании
- Описание всех тестовых наборов
- Команды запуска отдельных тестов
- Тестовое окружение (порты, БД, контейнеры)
- Troubleshooting
- CI/CD интеграция

### 2. tests/jwt/README.md
- JWT тесты подробно
- Тестовые пользователи
- Endpoints и примеры

### 3. tests/jwt/TEST_RESULTS.md
- Результаты последнего прогона
- Детали по каждому тесту

---

## 🎯 ПРИМЕРЫ ИСПОЛЬЗОВАНИЯ

### Запустить все тесты

```bash
./run_all_tests.sh
```

### Только IoC тесты

```bash
cd build && ./ioc_tests
```

### Только JWT тесты

```bash
./tests/jwt/run-tests.sh
```

### Только интеграционные тесты

```bash
./run_integration_tests.sh
```

### Конкретный тест IoC

```bash
cd build && ./ioc_tests --gtest_filter="IoCContainerTest.RegisterAndResolve"
```

### Конкретный файл JWT

```bash
docker-compose -f docker-compose.test.yml run --rm jwt-tests npm test auth.test.js
```

### С выводом в файл

```bash
./run_all_tests.sh 2>&1 | tee test-output.log
```

---

## ✨ ОСОБЕННОСТИ

✓ **Полная изоляция тестов** - отдельная БД LinksTest
✓ **Автоматическая очистка ресурсов** - при любом завершении
✓ **Цветной вывод** - для лучшей читаемости
✓ **Подробное логирование** - каждого этапа
✓ **Exit codes для CI/CD** - интеграция
✓ **Независимые тестовые наборы** - можно запускать отдельно
✓ **Быстрое выполнение** - ~7 секунд для всех тестов

---

## 🔍 ПОДРОБНОЕ ОПИСАНИЕ РАБОТЫ СКРИПТА

### Последовательность выполнения

1. **Инициализация**
   - Определение путей (PROJECT_ROOT, BUILD_DIR)
   - Установка обработчиков сигналов (trap)
   - Проверка зависимостей

2. **Начальная очистка**
   - Остановка Docker контейнеров
   - Завершение процессов smartlinks
   - Удаление старых логов

3. **IoC Container Unit Tests**
   - Запуск `./build/ioc_tests`
   - Проверка результата
   - Логирование

4. **JWT Microservice Tests**
   - Проверка MongoDB
   - Запуск тестового JWT сервиса (порт 3001)
   - Health check
   - Запуск Jest тестов в Docker
   - Остановка тестового сервиса

5. **SmartLinks Integration Tests**
   - Остановка production экземпляров
   - Запуск smartlinks с тестовой БД (LinksTest)
   - Ожидание готовности сервера
   - Запуск интеграционных тестов через ctest
   - Остановка тестового сервера

6. **Финальная очистка**
   - Остановка всех Docker контейнеров
   - Завершение всех процессов
   - Удаление логов
   - Освобождение портов

7. **Отчёт**
   - Время выполнения
   - Список проваленных тестов
   - Exit code

---

## 🛠️ ТЕХНИЧЕСКИЕ ДЕТАЛИ

### Переменные окружения

```bash
PROJECT_ROOT        # Корень проекта
BUILD_DIR           # Директория сборки (build/)
FAILED_TESTS        # Массив проваленных тестов
TEST_START_TIME     # Время начала выполнения
SERVER_PID          # PID тестового сервера
```

### Функции очистки

```bash
cleanup_all()       # Полная очистка всех ресурсов
print_header()      # Печать заголовков
print_success()     # Зелёный вывод успеха
print_error()       # Красный вывод ошибки
print_warning()     # Жёлтый вывод предупреждения
```

### Exit codes

- `0` - все тесты пройдены успешно
- `1` - один или более тестов провалились

---

## 🔧 ТРЕБОВАНИЯ

### Обязательные

- ✅ Проект собран (директория `build/` существует)
- ✅ Docker и Docker Compose установлены
- ✅ MongoDB контейнер доступен (запускается автоматически)

### Рекомендуемые

- Git (для контроля версий)
- Curl (для health checks)
- Node.js 18+ (если запускать JWT тесты локально)

---

## 🐛 TROUBLESHOOTING

### Проблема: "Build directory not found"

**Решение:**

```bash
mkdir -p build && cd build
cmake .. && cmake --build .
```

### Проблема: "MongoDB connection failed"

**Решение:**

```bash
docker-compose up -d mongodb
```

### Проблема: "Port 8080 already in use"

**Решение:**

```bash
pkill -f smartlinks
# или
lsof -i :8080 | grep LISTEN | awk '{print $2}' | xargs kill
```

### Проблема: "JWT tests timeout"

**Решение:**

```bash
docker-compose -f docker-compose.test.yml restart jwt-service-test
docker logs smartlinks-jwt-test
```

### Полная очистка всего (если что-то сломалось)

```bash
# Остановить все контейнеры
docker-compose down
docker-compose -f docker-compose.test.yml down

# Убить все процессы
pkill -f smartlinks
pkill -f wireserver

# Удалить логи
rm -f build/*.log tests/*.log

# Удалить тестовую БД (опционально)
docker exec -it smartlinks-mongodb mongosh -u root -p password \
  --eval "db.getSiblingDB('LinksTest').dropDatabase()"

# Полная очистка Docker volumes (ОПАСНО!)
docker-compose down -v
docker-compose -f docker-compose.test.yml down -v
```

---

## 📈 CI/CD ИНТЕГРАЦИЯ

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

      - name: Upload coverage
        if: success()
        uses: codecov/codecov-action@v3
        with:
          files: ./coverage/lcov.info
```

### GitLab CI пример

```yaml
test:
  stage: test
  image: docker:latest
  services:
    - docker:dind
  script:
    - apk add --no-cache cmake make g++ git
    - mkdir build && cd build
    - cmake .. && cmake --build .
    - cd ..
    - ./run_all_tests.sh
  artifacts:
    when: always
    paths:
      - build/*.log
      - tests/*.log
    reports:
      junit: test-results.xml
```

---

## 📊 ПРИМЕР ВЫВОДА

```
========================================
SmartLinks - All Tests Runner
========================================

Project: /home/user/smartlinks-cpp
Build: /home/user/smartlinks-cpp/build

Test sequence:
  1. IoC Container Unit Tests
  2. JWT Microservice Tests
  3. SmartLinks Integration Tests

========================================
Initial cleanup
========================================

Stopping Docker containers...
Stopping smartlinks processes...
Cleaning up log files...
✅ Cleanup completed

========================================
Test Suite 1/3: IoC Container Unit Tests
========================================

Running IoC unit tests...
[==========] Running 15 tests from 3 test suites.
[----------] Global test environment set-up.
[----------] 5 tests from IoCContainerTest
[ RUN      ] IoCContainerTest.RegisterAndResolve
[       OK ] IoCContainerTest.RegisterAndResolve (0 ms)
...
[----------] 15 tests from IoCContainerTest (2 ms total)

[==========] 15 tests from 3 test suites ran. (2 ms total)
[  PASSED  ] 15 tests.
✅ IoC tests passed

========================================
Test Suite 2/3: JWT Microservice Tests
========================================

Checking MongoDB...
✅ MongoDB is running
Starting JWT test service...
Server started (PID: 12345)
Checking JWT service health...
✅ JWT service is healthy
Running JWT tests...

 PASS  tests/auth.test.js (4.532 s)
 PASS  tests/introspect.test.js (2.145 s)
 PASS  tests/jwks.test.js (1.876 s)
 PASS  tests/health.test.js (1.234 s)

Test Suites: 4 passed, 4 total
Tests:       40 passed, 40 total
Snapshots:   0 total
Time:        4.913 s
Ran all test suites.
✅ JWT tests passed

Stopping JWT test service...

========================================
Test Suite 3/3: SmartLinks Integration Tests
========================================

Stopping existing smartlinks instances...
Starting smartlinks test server...
Server started (PID: 12346)
Waiting for server to start...
✅ Server is ready
Running integration tests...

[==========] Running 12 tests from 2 test suites.
[----------] Global test environment set-up.
[----------] 6 tests from SmartLinksIntegrationTest
[ RUN      ] SmartLinksIntegrationTest.CreateShortLink
[       OK ] SmartLinksIntegrationTest.CreateShortLink (15 ms)
...
[----------] 12 tests from SmartLinksIntegrationTest (187 ms total)

[==========] 12 tests from 2 test suites ran. (187 ms total)
[  PASSED  ] 12 tests.
✅ Integration tests passed

Stopping test server...

========================================
Cleaning up all resources
========================================

Stopping Docker containers...
Stopping smartlinks processes...
Cleaning up log files...
✅ Cleanup completed

========================================
Test Results Summary
========================================

Total execution time: 45s

✅ ALL TESTS PASSED! 🎉

Test suites completed:
  ✅ IoC Container Unit Tests
  ✅ JWT Microservice Tests
  ✅ SmartLinks Integration Tests
```

---

## 🎯 BEST PRACTICES

### При разработке

1. Запускайте тесты перед каждым коммитом
2. Проверяйте вывод на наличие warnings
3. Используйте отдельные скрипты для быстрой итерации

### В CI/CD

1. Кешируйте Docker образы для ускорения
2. Сохраняйте логи как артефакты
3. Используйте parallel jobs для разных тестовых наборов

### При отладке

1. Запускайте конкретные тесты, а не весь набор
2. Используйте `--verbose` флаги для детального вывода
3. Проверяйте логи в `build/*.log` и `tests/*.log`

---

## 📞 ПОДДЕРЖКА

Если возникли проблемы:

1. Проверьте **TESTING.md** - полная документация
2. Проверьте **tests/jwt/README.md** - JWT специфика
3. Запустите отдельные тестовые наборы для изоляции проблемы
4. Проверьте логи Docker контейнеров

---

## 📄 ЛИЦЕНЗИЯ

MIT

---

**Автор:** Claude Code
**Дата создания:** 2026-03-15
**Версия:** 1.0.0
**Проект:** SmartLinks C++
