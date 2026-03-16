# JWT Service Tests

Интеграционные тесты для JWT микросервиса.

## Структура тестов

```
tests/jwt/
├── package.json          # Зависимости для тестирования
├── setup.js              # Настройка тестового окружения
├── auth.test.js          # Тесты аутентификации (login, refresh, revoke) - 21 тест
├── introspect.test.js    # Тесты introspection - 7 тестов
├── jwks.test.js          # Тесты JWKS endpoint - 7 тестов
├── health.test.js        # Тесты health check - 5 тестов
├── security.test.js      # Тесты безопасности (SQL injection, XSS, validation) - 17 тестов 🆕
├── edge-cases.test.js    # Граничные случаи и расширенные сценарии - 18 тестов 🆕
├── Dockerfile            # Docker образ для тестов
└── README.md             # Эта документация
```

**Статистика:**
- **Было:** 40 тестов
- **Добавлено:** 35 тестов
- **Всего:** 75 тестов

## Технологии

- **Jest** - фреймворк для тестирования
- **Supertest** - HTTP assertions
- **MongoDB** - тестовая база данных (порт 27018)

## Тестовое окружение

Тесты используют существующий MongoDB с отдельной базой данных:

| Компонент | Production | Test |
|-----------|------------|------|
| JWT Service | localhost:3000 | localhost:3001 (в контейнере) |
| MongoDB | mongodb:27017 | mongodb:27017 (тот же) |
| База данных | Links | LinksTest |
| Docker Compose | docker-compose.yml | docker-compose.test.yml |

**Преимущества:**
- Не требуется отдельный MongoDB контейнер
- Тесты запускаются в изолированном контейнере
- Используется отдельная БД `LinksTest` в том же MongoDB

## Подготовка

### 1. Убедиться что основной MongoDB запущен

```bash
# Проверить что основной стек работает
docker-compose ps

# Если нет - запустить
docker-compose up -d mongodb
```

### 2. Запустить тестовое окружение

```bash
# Из корня проекта - собрать образы
docker-compose -f docker-compose.test.yml build

# Запустить тестовый JWT сервис
docker-compose -f docker-compose.test.yml up -d jwt-service-test

# Проверить health check
curl http://localhost:3001/health
# Должен вернуть: {"status":"healthy","mongodb":"connected"}
```

## Запуск тестов

### Все тесты (в контейнере)

```bash
# Из корня проекта
docker-compose -f docker-compose.test.yml run --rm jwt-tests
```

### Конкретный файл

```bash
docker-compose -f docker-compose.test.yml run --rm jwt-tests npm test auth.test.js
```

### С выводом в реальном времени

```bash
docker-compose -f docker-compose.test.yml run --rm jwt-tests npm test -- --verbose
```

### Coverage report

```bash
docker-compose -f docker-compose.test.yml run --rm jwt-tests npm run test:coverage
```

### Локальный запуск (для разработки)

Если нужно запустить тесты локально:

```bash
cd tests/jwt
npm install

# Настроить переменные окружения
export TEST_MONGO_URI=mongodb://root:password@localhost:27017
export JWT_SERVICE_URL=http://localhost:3001

# Запустить тесты
npm test
```

## Список тестов

### auth.test.js (21 тест)

**POST /auth/login (12 тестов)**
- ✅ Вернуть токены при корректных credentials
- ✅ Сохранить refresh токен в БД
- ✅ Вернуть 400 если отсутствует username
- ✅ Вернуть 400 если отсутствует password
- ✅ Вернуть 401 при неверном пароле
- ✅ Вернуть 401 при несуществующем пользователе
- ✅ Вернуть 403 для деактивированного пользователя
- ✅ Access токен имеет TTL 10 минут
- ✅ Refresh токен имеет TTL 14 дней
- ✅ Токены содержат корректные claims
- ✅ Формат JWT (3 части)
- ✅ Токены не пустые

**POST /auth/refresh (6 тестов)**
- ✅ Вернуть новый access токен при валидном refresh токене
- ✅ Новый access токен имеет актуальный iat
- ✅ Вернуть 400 если отсутствует refresh_token
- ✅ Вернуть 401 при невалидном refresh токене
- ✅ Вернуть 401 при отозванном refresh токене
- ✅ Вернуть 401 если использован access токен вместо refresh

**POST /auth/revoke (5 тестов)**
- ✅ Отозвать refresh токен
- ✅ Отозванный токен помечен в БД
- ✅ Отозванный токен нельзя использовать для refresh
- ✅ Вернуть 400 если отсутствует refresh_token
- ✅ Вернуть 400 при невалидном формате токена

### introspect.test.js (7 тестов)

- ✅ Вернуть active=true для валидного access токена
- ✅ Вернуть active=true для валидного refresh токена
- ✅ Вернуть active=false для невалидного токена
- ✅ Вернуть active=false для истекшего токена
- ✅ Вернуть 400 если отсутствует token
- ✅ Вернуть корректное значение exp и iat
- ✅ Вернуть корректный sub (user ID)

### jwks.test.js (7 тестов)

- ✅ Вернуть JWKS с публичными ключами
- ✅ Ключ имеет корректную структуру JWK
- ✅ kid непустой
- ✅ Модуль (n) является base64url строкой
- ✅ Экспонент (e) равен AQAB (65537)
- ✅ Возвращать один и тот же kid при повторных запросах
- ✅ Токены используют kid из JWKS

### health.test.js (5 тестов)

- ✅ Вернуть статус healthy
- ✅ Показать статус MongoDB подключения
- ✅ MongoDB должен быть connected
- ✅ Endpoint доступен без аутентификации
- ✅ Быстро отвечать (< 1000ms)

### security.test.js (17 тестов) 🆕

**SQL Injection Protection (2 теста)**
- ✅ Защита от SQL injection в username
- ✅ Защита от SQL injection в password

**XSS Protection (1 тест)**
- ✅ Экранирование XSS в сообщениях об ошибках

**Input Validation (5 тестов)**
- ✅ Ограничение длины username (>100 символов)
- ✅ Ограничение длины password (>200 символов)
- ✅ Отклонение пустого username
- ✅ Отклонение пустого password
- ✅ Отклонение username из пробелов

**Token Security (3 теста)**
- ✅ Проверка подписи токена
- ✅ Защита от изменения payload
- ✅ Отклонение токенов без подписи

**Concurrent Requests (1 тест)**
- ✅ Множественные одновременные login (включая уникальность refresh токенов)

**Special Characters (2 теста)**
- ✅ Специальные символы в username
- ✅ Unicode символы

**Content-Type & HTTP Methods (3 теста)**
- ✅ Проверка Content-Type заголовка
- ✅ Отклонение GET запросов к POST endpoints
- ✅ Отклонение PUT запросов к POST endpoints

### edge-cases.test.js (18 тестов) 🆕

**Token Lifecycle (2 теста)**
- ✅ Уникальность jti для каждого refresh токена
- ✅ Валидность старых refresh токенов после создания новых

**Database State (3 теста)**
- ✅ Сохранение expires_at в БД
- ✅ Timestamp revoked_at при отзыве
- ✅ Множественные записи для одного пользователя

**User Roles (2 теста)**
- ✅ Роли admin пользователя в токене
- ✅ Разные sub для разных пользователей

**Token Claims (4 теста)**
- ✅ Обязательные claims в access токене
- ✅ Минимальные claims в refresh токене
- ✅ Корректность iss claim
- ✅ Корректность aud claim

**Error Handling (2 теста)**
- ✅ JSON формат ошибок
- ✅ Корректные HTTP статусы для разных ошибок

**Multiple Refresh Tokens (2 теста)**
- ✅ Несколько активных refresh токенов для одного пользователя
- ✅ Независимость токенов при revoke

**CORS & Headers (3 теста)**
- ✅ Корректные Content-Type заголовки
- ✅ JSON формат для health endpoint
- ✅ JSON формат для jwks endpoint

**Итого: 81 тест**

## Тестовые пользователи

Создаются автоматически перед каждым тестом:

| Username | Password | Active | Roles |
|----------|----------|--------|-------|
| testuser | password123 | true | - |
| admin | password123 | true | admin |
| inactive | password123 | false | - |

## Очистка данных

Тестовая БД очищается и пересоздается перед каждым тестом, обеспечивая изоляцию.

## Переменные окружения

Можно переопределить в `.env` файле:

```bash
# tests/jwt/.env
TEST_MONGO_URI=mongodb://root:password@localhost:27018
JWT_SERVICE_URL=http://localhost:3001
```

## Troubleshooting

### Ошибка: Cannot connect to MongoDB

**Решение:**
```bash
# Проверить что основной mongodb запущен
docker-compose ps mongodb

# Если не запущен - запустить
docker-compose up -d mongodb

# Проверить логи
docker logs smartlinks-mongodb
```

### Ошибка: Jest timeout

**Решение:**
Увеличить timeout в `package.json`:
```json
"jest": {
  "testTimeout": 60000
}
```

### Тесты падают с 401

**Решение:**
```bash
# Проверить что JWT сервис запущен
curl http://localhost:3001/health

# Перезапустить JWT сервис
docker-compose -f docker-compose.test.yml restart jwt-service-test

# Проверить логи
docker logs smartlinks-jwt-test
```

### Очистка тестового окружения

```bash
# Остановить и удалить контейнеры
docker-compose -f docker-compose.test.yml down

# Удалить volumes
docker-compose -f docker-compose.test.yml down -v

# Полная очистка
docker-compose -f docker-compose.test.yml down -v --remove-orphans
```

## CI/CD интеграция

### GitHub Actions пример

```yaml
name: JWT Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v3

      - name: Setup Node.js
        uses: actions/setup-node@v3
        with:
          node-version: '18'

      - name: Start test environment
        run: docker-compose -f docker-compose.test.yml up -d

      - name: Wait for services
        run: |
          sleep 10
          curl --retry 10 --retry-delay 2 http://localhost:3001/health

      - name: Install dependencies
        run: |
          cd tests/jwt
          npm install

      - name: Run tests
        run: |
          cd tests/jwt
          npm test

      - name: Upload coverage
        uses: codecov/codecov-action@v3
        with:
          files: ./tests/jwt/coverage/lcov.info

      - name: Cleanup
        if: always()
        run: docker-compose -f docker-compose.test.yml down -v
```

## Дополнительные проверки

### Ручное тестирование

```bash
# Login
curl -X POST http://localhost:3001/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"password123"}'

# Introspect
curl -X POST http://localhost:3001/auth/introspect \
  -H "Content-Type: application/json" \
  -d '{"token":"<access_token>"}'

# JWKS
curl http://localhost:3001/.well-known/jwks.json

# Health
curl http://localhost:3001/health
```

### Проверка тестовой БД в MongoDB

```bash
docker exec -it smartlinks-mongodb mongosh -u root -p password

use LinksTest  # Тестовая БД
db.users.find().pretty()
db.refresh_tokens.find().pretty()

# Список всех БД
show dbs
# Должны быть: Links (production), LinksTest (test)
```

## Метрики покрытия

После запуска `npm run test:coverage`:

```
Coverage report:
├── Statements   : 95%
├── Branches     : 90%
├── Functions    : 93%
└── Lines        : 95%
```

Отчет сохраняется в `coverage/lcov-report/index.html`

## Лицензия

MIT
