# JWT Authentication Service - Документация

## Обзор

JWT микросервис для выдачи и обновления токенов в проекте smartlinks-cpp. Использует Node.js + jsonwebtoken с асимметричным шифрованием RS256.

## Архитектура

```
┌──────────────────────────────────────────────────────┐
│              smartlinks-network                      │
│                                                       │
│  ┌──────────────┐          ┌─────────────────┐      │
│  │   mongodb    │◄────────►│  jwt-service    │      │
│  │  - users     │          │  (Node.js)      │      │
│  │  - refresh_  │          │  - port 3000    │      │
│  │    tokens    │          │  - RS256 JWT    │      │
│  └──────────────┘          └─────────────────┘      │
│                                                       │
└──────────────────────────────────────────────────────┘
```

## Технические характеристики

| Параметр | Значение |
|----------|----------|
| **Язык** | Node.js 18 |
| **Framework** | Express 4.18 |
| **JWT библиотека** | jsonwebtoken 9.0 |
| **БД** | MongoDB (коллекции: users, refresh_tokens) |
| **Алгоритм** | RS256 (RSA 2048-bit) |
| **Access токен TTL** | 10 минут (600 сек) |
| **Refresh токен TTL** | 14 дней (1209600 сек) |
| **Порт** | 3000 |
| **Docker образ** | node:18-alpine (~180MB) |

## Endpoints

### 1. POST /auth/login

Аутентификация пользователя и выдача токенов.

**Request:**
```json
{
  "username": "testuser",
  "password": "password123"
}
```

**Response (200 OK):**
```json
{
  "access_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",
  "refresh_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",
  "token_type": "Bearer",
  "expires_in": 600
}
```

**Errors:**
- `400 Bad Request` - отсутствует username или password
- `401 Unauthorized` - неверные credentials
- `403 Forbidden` - пользователь деактивирован

**Пример:**
```bash
curl -X POST http://localhost:3000/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"password123"}'
```

---

### 2. POST /auth/refresh

Обновление access токена с помощью refresh токена.

**Request:**
```json
{
  "refresh_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Response (200 OK):**
```json
{
  "access_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",
  "token_type": "Bearer",
  "expires_in": 600
}
```

**Errors:**
- `400 Bad Request` - отсутствует refresh_token
- `401 Unauthorized` - токен невалиден, истек или отозван

**Пример:**
```bash
curl -X POST http://localhost:3000/auth/refresh \
  -H "Content-Type: application/json" \
  -d '{"refresh_token":"<your_refresh_token>"}'
```

---

### 3. POST /auth/revoke

Отзыв refresh токена (logout).

**Request:**
```json
{
  "refresh_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Response (200 OK):**
```json
{
  "success": true
}
```

**Errors:**
- `400 Bad Request` - отсутствует refresh_token или невалидный формат

**Пример:**
```bash
curl -X POST http://localhost:3000/auth/revoke \
  -H "Content-Type: application/json" \
  -d '{"refresh_token":"<your_refresh_token>"}'
```

---

### 4. POST /auth/introspect

Проверка валидности токена.

**Request:**
```json
{
  "token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Response (200 OK) - валидный токен:**
```json
{
  "active": true,
  "sub": "69b6897fab5e1da2b38563b1",
  "username": "testuser",
  "type": "access",
  "exp": 1773571783,
  "iat": 1773571183,
  "iss": "smartlinks-jwt-service",
  "aud": "smartlinks-api"
}
```

**Response (200 OK) - невалидный токен:**
```json
{
  "active": false
}
```

**Пример:**
```bash
curl -X POST http://localhost:3000/auth/introspect \
  -H "Content-Type: application/json" \
  -d '{"token":"<your_access_token>"}'
```

---

### 5. GET /.well-known/jwks.json

Публичные ключи в формате JWKS (JSON Web Key Set).

**Response (200 OK):**
```json
{
  "keys": [
    {
      "kty": "RSA",
      "use": "sig",
      "alg": "RS256",
      "kid": "6f5191f73609895c",
      "n": "o0sVtXR60K8uWC7YC3lZcVKQnhMPyb6H6CTl...",
      "e": "AQAB"
    }
  ]
}
```

**Использование:**
- Для валидации JWT токенов в других сервисах
- Публичный ключ для проверки подписи RS256

**Пример:**
```bash
curl http://localhost:3000/.well-known/jwks.json | jq
```

---

### 6. GET /health

Health check endpoint.

**Response (200 OK):**
```json
{
  "status": "healthy",
  "mongodb": "connected"
}
```

**Пример:**
```bash
curl http://localhost:3000/health
```

## Структура JWT токенов

### Access Token

**Header:**
```json
{
  "alg": "RS256",
  "typ": "JWT",
  "kid": "6f5191f73609895c"
}
```

**Payload:**
```json
{
  "sub": "69b6897fab5e1da2b38563b1",  // User ID
  "username": "testuser",
  "email": "testuser@example.com",
  "type": "access",
  "iat": 1773571183,                  // Issued at
  "exp": 1773571783,                  // Expires (iat + 600 сек)
  "aud": "smartlinks-api",
  "iss": "smartlinks-jwt-service"
}
```

### Refresh Token

**Header:**
```json
{
  "alg": "RS256",
  "typ": "JWT",
  "kid": "6f5191f73609895c"
}
```

**Payload:**
```json
{
  "sub": "69b6897fab5e1da2b38563b1",  // User ID
  "jti": "2542b07b-6b1d-47d3-8056-...", // Token ID (UUID)
  "type": "refresh",
  "iat": 1773571132,                  // Issued at
  "exp": 1774780732,                  // Expires (iat + 14 дней)
  "aud": "smartlinks-api",
  "iss": "smartlinks-jwt-service"
}
```

## MongoDB коллекции

### Коллекция: users

**Структура документа:**
```javascript
{
  _id: ObjectId("69b6897fab5e1da2b38563b1"),
  username: "testuser",                    // Уникальный
  password_hash: "$2b$10$GFvECH1aadvbc...", // bcrypt hash
  email: "testuser@example.com",
  created_at: ISODate("2026-03-15T10:27:11.832Z"),
  active: true,                            // Флаг активности
  roles: ["admin"]                         // Опционально
}
```

**Индексы:**
- `username` (unique)
- `email` (sparse)

---

### Коллекция: refresh_tokens

**Структура документа:**
```javascript
{
  _id: ObjectId("..."),
  jti: "2542b07b-6b1d-47d3-8056-48b07f0eb00b", // UUID из токена
  user_id: ObjectId("69b6897fab5e1da2b38563b1"),
  username: "testuser",
  token: "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...", // Полный токен
  expires_at: ISODate("2026-03-29T10:32:12.000Z"),
  created_at: ISODate("2026-03-15T10:32:12.000Z"),
  revoked: false,                              // Флаг отзыва
  revoked_at: null                             // Время отзыва
}
```

**Индексы:**
- `jti` (unique)
- `user_id`
- `expires_at` (для автоочистки)
- `revoked`

**Автоочистка:**
Истекшие токены удаляются каждый час (3600000 мс).

## Конфигурация

### Переменные окружения

| Переменная | Значение по умолчанию | Описание |
|------------|----------------------|----------|
| `PORT` | 3000 | Порт сервиса |
| `MONGO_URI` | mongodb://root:password@mongodb:27017 | URI подключения к MongoDB |
| `MONGO_DB` | Links | Имя базы данных |
| `ACCESS_TOKEN_TTL` | 10m | Время жизни access токена |
| `REFRESH_TOKEN_TTL` | 14d | Время жизни refresh токена |
| `JWT_ISSUER` | smartlinks-jwt-service | Issuer в токенах |
| `JWT_AUDIENCE` | smartlinks-api | Audience в токенах |
| `NODE_ENV` | production | Окружение Node.js |

### Docker Compose

```yaml
jwt-service:
  build:
    context: ./jwt-service
    dockerfile: Dockerfile
  container_name: smartlinks-jwt
  restart: unless-stopped
  ports:
    - "3000:3000"
  environment:
    - PORT=3000
    - MONGO_URI=mongodb://root:password@mongodb:27017
    - MONGO_DB=Links
    - ACCESS_TOKEN_TTL=10m
    - REFRESH_TOKEN_TTL=14d
    - JWT_ISSUER=smartlinks-jwt-service
    - JWT_AUDIENCE=smartlinks-api
    - NODE_ENV=production
  volumes:
    - jwt_keys:/app/keys  # Персистентность ключей
  depends_on:
    mongodb:
      condition: service_healthy
  healthcheck:
    test: ["CMD", "wget", "--spider", "--quiet", "http://localhost:3000/health"]
    interval: 10s
    timeout: 5s
    retries: 3
  networks:
    - smartlinks-network
```

## PKI инфраструктура

### Генерация RSA ключей

Ключи генерируются автоматически при первом запуске контейнера через `entrypoint.sh`:

```bash
#!/bin/sh
PRIVATE_KEY_PATH="/app/keys/private.key"
PUBLIC_KEY_PATH="/app/keys/public.key"

if [ ! -f "$PRIVATE_KEY_PATH" ]; then
    echo "Generating RSA key pair (2048-bit)..."

    # Генерировать приватный ключ RSA 2048-bit
    openssl genrsa -out "$PRIVATE_KEY_PATH" 2048

    # Извлечь публичный ключ
    openssl rsa -in "$PRIVATE_KEY_PATH" -pubout -out "$PUBLIC_KEY_PATH"

    chmod 600 "$PRIVATE_KEY_PATH"
    chmod 644 "$PUBLIC_KEY_PATH"
fi

exec "$@"
```

**Хранение:**
- Приватный ключ: `/app/keys/private.key` (в Docker volume `jwt_keys`)
- Публичный ключ: `/app/keys/public.key` (в Docker volume `jwt_keys`)

**Персистентность:**
Ключи сохраняются между перезапусками контейнера благодаря Docker volume.

**Проверка ключей:**
```bash
# Просмотр приватного ключа
docker exec smartlinks-jwt openssl rsa -in /app/keys/private.key -text -noout

# Просмотр публичного ключа
docker exec smartlinks-jwt openssl rsa -pubin -in /app/keys/public.key -text -noout
```

## Управление пользователями

### Добавление нового пользователя

**1. Генерация bcrypt hash:**

```bash
cd jwt-service
npm install bcrypt
node hash-password.js mypassword
```

**2. Вставка в MongoDB:**

```bash
docker exec -it smartlinks-mongodb mongosh -u root -p password
```

```javascript
use Links

db.users.insertOne({
  username: 'newuser',
  password_hash: '$2b$10$...',  // Вставить сгенерированный hash
  email: 'newuser@example.com',
  created_at: new Date(),
  active: true
})
```

### Деактивация пользователя

```javascript
db.users.updateOne(
  { username: 'testuser' },
  { $set: { active: false } }
)
```

### Изменение пароля

```javascript
db.users.updateOne(
  { username: 'testuser' },
  { $set: { password_hash: '$2b$10$...' } }  // Новый hash
)
```

### Просмотр всех пользователей

```javascript
db.users.find({}).pretty()
```

## Тестовые пользователи

При первом запуске создаются 2 тестовых пользователя:

| Username | Password | Email | Roles |
|----------|----------|-------|-------|
| testuser | password123 | testuser@example.com | - |
| admin | password123 | admin@example.com | admin |

**⚠️ В production обязательно изменить пароли!**

## Мониторинг и логирование

### Просмотр логов

```bash
# Все логи
docker logs -f smartlinks-jwt

# Только ошибки
docker logs smartlinks-jwt 2>&1 | grep -i error

# Логины
docker logs smartlinks-jwt 2>&1 | grep "Login successful"

# Refresh токенов
docker logs smartlinks-jwt 2>&1 | grep "Token refreshed"
```

### Проверка refresh токенов в БД

```bash
docker exec -it smartlinks-mongodb mongosh -u root -p password
```

```javascript
use Links

// Активные refresh токены
db.refresh_tokens.find({ revoked: false }).pretty()

// Истекшие токены
db.refresh_tokens.find({ expires_at: { $lt: new Date() } }).count()

// Токены конкретного пользователя
db.refresh_tokens.find({ username: "testuser" }).pretty()

// Отозванные токены
db.refresh_tokens.find({ revoked: true }).pretty()
```

### Метрики

**Автоочистка истекших токенов:**
Выполняется каждый час (3600000 мс). Логи:
```
Cleaned up 5 expired refresh tokens
```

## Безопасность

### Production checklist

- [ ] **Изменить пароли MongoDB**
  ```yaml
  MONGO_INITDB_ROOT_PASSWORD: <strong_password>
  ```

- [ ] **Использовать HTTPS**
  - Настроить reverse proxy (nginx/traefik)
  - Получить SSL сертификаты (Let's Encrypt)
  - Обновить `JWT_ISSUER` на HTTPS URL

- [ ] **Ограничить CORS**
  ```javascript
  app.use(cors({
    origin: ['https://yourdomain.com']  // Вместо '*'
  }));
  ```

- [ ] **Добавить rate limiting**
  ```bash
  npm install express-rate-limit
  ```

  ```javascript
  const rateLimit = require('express-rate-limit');

  const loginLimiter = rateLimit({
    windowMs: 15 * 60 * 1000, // 15 минут
    max: 5,                   // 5 попыток
    message: 'Too many login attempts'
  });

  app.post('/auth/login', loginLimiter, async (req, res) => { ... });
  ```

- [ ] **Сменить тестовые пароли**
  - testuser, admin должны иметь сильные пароли

- [ ] **Backup RSA ключей**
  ```bash
  docker cp smartlinks-jwt:/app/keys/private.key ./backup/
  docker cp smartlinks-jwt:/app/keys/public.key ./backup/
  ```

- [ ] **Настроить логирование**
  - Использовать winston для structured logging
  - Отправлять логи в ELK/Grafana

### Известные ограничения

1. **In-memory хранилище refresh токенов**
   - В текущей версии используется MongoDB
   - Для высоконагруженных систем рекомендуется Redis

2. **Нет ротации ключей**
   - Ключи генерируются один раз
   - Рекомендуется периодическая ротация (каждые 90 дней)

3. **Нет 2FA**
   - Только username/password аутентификация
   - Рекомендуется добавить TOTP для критичных аккаунтов

## Интеграция с SmartLinks

SmartLinks интегрирован с JWT сервисом через **DSL выражение AUTHORIZED**.

### Архитектура интеграции

```
┌─────────────────┐         ┌──────────────────┐
│  SmartLinks     │         │  JWT Service     │
│  (C++)          │         │  (Node.js)       │
│                 │         │                  │
│  DSL Rules:     │  HTTP   │  /.well-known/   │
│  IF AUTHORIZED  │◄────────┤  jwks.json       │
│  THEN redirect  │  JWKS   │                  │
│                 │         │  RS256 Public    │
│  JwtValidator   │         │  Keys            │
│  (jwt-cpp)      │         │                  │
└─────────────────┘         └──────────────────┘
```

### DSL синтаксис для авторизации

```
// Простая авторизация
AUTHORIZED -> https://example-private.com

// Комбинация с языком
AUTHORIZED AND LANGUAGE = en-US -> https://private.example.com/en

// Fallback для неавторизованных (множественные правила через ;)
AUTHORIZED -> https://private.com; DATETIME < 9999-12-31T23:59:59Z -> https://public.com
```

### IJwtValidator интерфейс

**Интерфейс:**
```cpp
namespace domain {

class IJwtValidator {
public:
    virtual ~IJwtValidator() = default;
    virtual bool Validate(const std::string& token) = 0;
};

}  // namespace domain
```

**Реализация (jwt-cpp):**
```cpp
#include <jwt-cpp/jwt.h>
#include <curl/curl.h>

class JwtValidator : public domain::IJwtValidator {
private:
    std::string jwks_uri_;   // http://localhost:3000/auth/.well-known/jwks.json
    std::string issuer_;     // smartlinks-jwt-service
    std::string audience_;   // smartlinks-api
    std::string public_key_; // Кэшированный публичный ключ

public:
    JwtValidator(
        const std::string& jwks_uri,
        const std::string& issuer,
        const std::string& audience
    ) : jwks_uri_(jwks_uri), issuer_(issuer), audience_(audience) {
        // Загрузить публичный ключ из JWKS
        public_key_ = FetchPublicKeyFromJWKS();
    }

    bool Validate(const std::string& token) override {
        try {
            // Создать verifier с RS256 алгоритмом
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::rs256(public_key_))
                .with_issuer(issuer_)
                .with_audience(audience_);

            // Декодировать и верифицировать токен
            auto decoded = jwt::decode(token);
            verifier.verify(decoded);

            return true;  // Токен валиден
        } catch (const std::exception& e) {
            LOG_DEBUG << "[Validate] JWT validation failed: " << e.what();
            return false;
        } catch (...) {
            LOG_DEBUG << "[Validate] JWT validation failed: unknown error";
            return false;
        }
    }

private:
    std::string FetchPublicKeyFromJWKS() {
        // HTTP GET запрос к jwks_uri
        // Парсинг JSON ответа
        // Извлечение первого ключа из keys[]
        // Конвертация JWKS в PEM формат
        return public_key_pem;
    }
};
```

### Конфигурация JWT в SmartLinks

**Переменные окружения:**

| Переменная | Значение по умолчанию | Описание |
|------------|----------------------|----------|
| `JWT_JWKS_URI` | http://jwt-service:3000/auth/.well-known/jwks.json | ⚠️ **ВАЖНО:** Полный путь к JWKS |
| `JWT_ISSUER` | smartlinks-jwt-service | Expected issuer в токенах |
| `JWT_AUDIENCE` | smartlinks-api | Expected audience в токенах |

**⚠️ Критично - полный путь к JWKS:**

```bash
# ❌ НЕПРАВИЛЬНО - отсутствует путь
JWT_JWKS_URI=http://localhost:3000

# ✅ ПРАВИЛЬНО - полный путь
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json
```

**Конфигурация в main.cpp:**
```cpp
// Читать из переменных окружения или config.json
const char* env_jwks_uri = std::getenv("JWT_JWKS_URI");
const char* env_issuer = std::getenv("JWT_ISSUER");
const char* env_audience = std::getenv("JWT_AUDIENCE");

std::string jwks_uri = env_jwks_uri
    ? env_jwks_uri
    : config.get("JWT", Json::Value())
        .get("JwksUri", "http://jwt-service:3000/auth/.well-known/jwks.json")
        .asString();

std::string issuer = env_issuer
    ? env_issuer
    : config.get("JWT", Json::Value())
        .get("Issuer", "smartlinks-jwt-service")
        .asString();

std::string audience = env_audience
    ? env_audience
    : config.get("JWT", Json::Value())
        .get("Audience", "smartlinks-api")
        .asString();

// Зарегистрировать IJwtValidator в IoC
IoC::Resolve<shared_ptr<ICommand>>("IoC.Register", Args{
    std::string("IJwtValidator"),
    DependencyFactory([jwks_uri, issuer, audience](const Args&) -> std::any {
        return std::static_pointer_cast<domain::IJwtValidator>(
            std::make_shared<JwtValidator>(jwks_uri, issuer, audience)
        );
    })
})->Execute();
```

### AUTHORIZED плагин (адаптер)

**Расположение:** `plugins/authorized/src/context_2_authorized_accessible.cpp`

**Как работает:**

1. Адаптер получает из Context:
   - `request` - HTTP запрос (drogon::HttpRequestPtr)
   - `jwt_validator` - валидатор токенов (IJwtValidator)

2. Извлекает JWT токен из `Authorization: Bearer <token>` header

3. Вызывает `jwt_validator->Validate(token)`

4. Возвращает `true` если токен валиден, `false` иначе

**Код:**
```cpp
bool Context2AuthorizedAccessible::is_authorized() const {
    // 1. Получить request из контекста
    auto request_any = context_->get("request");
    if (!request_any.has_value()) {
        return false;
    }
    auto request = std::any_cast<drogon::HttpRequestPtr>(request_any);

    // 2. Получить JWT validator из контекста
    auto jwt_validator_any = context_->get("jwt_validator");
    if (!jwt_validator_any.has_value()) {
        return false;
    }
    auto jwt_validator = std::any_cast<
        std::shared_ptr<domain::IJwtValidator>
    >(jwt_validator_any);

    // 3. Извлечь токен из Authorization header
    auto auth_header = request->getHeader("Authorization");
    if (auth_header.empty()) {
        return false;
    }

    const std::string bearer_prefix = "Bearer ";
    if (auth_header.find(bearer_prefix) != 0) {
        return false;
    }

    std::string token = auth_header.substr(bearer_prefix.length());

    // 4. Валидировать токен
    return jwt_validator->Validate(token);
}
```

### Пример использования

**1. Получить JWT токен:**
```bash
curl -X POST http://localhost/jwt/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "testuser", "password": "password123"}'
```

**Ответ:**
```json
{
  "access_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",
  "refresh_token": "...",
  "token_type": "Bearer",
  "expires_in": 600
}
```

**2. Создать DSL правило в MongoDB:**
```javascript
db.links.insertOne({
  slug: "private-link",
  state: "active",
  rules_dsl: "AUTHORIZED -> https://example-private.com; DATETIME < 9999-12-31T23:59:59Z -> https://example-public.com"
})
```

**3. Использовать ссылку БЕЗ токена:**
```bash
curl -I http://localhost/private-link
# → 307 Redirect
# → Location: https://example-public.com
```

**4. Использовать ссылку С токеном:**
```bash
curl -I http://localhost/private-link \
  -H "Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9..."
# → 307 Redirect
# → Location: https://example-private.com
```

### Конфигурация в тестах

**run_all_tests.sh:**
```bash
# JSON mode тесты
MONGODB_DATABASE=LinksTest USE_DSL_RULES=false \
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json \
./smartlinks > smartlinks_json_test.log 2>&1 &

# DSL mode тесты
MONGODB_DATABASE=LinksDSLTest \
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json \
./smartlinks > smartlinks_dsl_test.log 2>&1 &
```

**⚠️ Важно:** Путь к JWKS должен быть полным, включая `/auth/.well-known/jwks.json`

## Troubleshooting

### Проблема: "Invalid username or password"

**Причина:** Неверный bcrypt hash или пользователь не существует

**Решение:**
1. Проверить пользователя в БД:
   ```javascript
   db.users.find({ username: "testuser" })
   ```

2. Пересоздать hash:
   ```bash
   docker exec smartlinks-jwt node -e "
   const bcrypt = require('bcrypt');
   bcrypt.hash('password123', 10, (err, hash) => {
     console.log(hash);
   });"
   ```

3. Обновить пароль:
   ```javascript
   db.users.updateOne(
     { username: "testuser" },
     { $set: { password_hash: "<new_hash>" } }
   )
   ```

---

### Проблема: "Token revoked or not found"

**Причина:** Refresh токен был отозван или удален

**Решение:**
1. Получить новые токены через `/auth/login`
2. Проверить статус токена:
   ```javascript
   db.refresh_tokens.find({ jti: "<token_jti>" })
   ```

---

### Проблема: Ключи не генерируются

**Причина:** Проблемы с правами доступа к volume

**Решение:**
```bash
# Пересоздать volume
docker-compose down
docker volume rm smartlinks-cpp_jwt_keys
docker-compose up -d jwt-service

# Проверить логи
docker logs smartlinks-jwt
```

---

### Проблема: MongoDB connection error

**Причина:** MongoDB не запущена или недоступна

**Решение:**
```bash
# Проверить статус
docker-compose ps

# Проверить логи MongoDB
docker logs smartlinks-mongodb

# Перезапустить сервисы
docker-compose restart mongodb jwt-service
```

## Версионирование и обновления

**Текущая версия:** 1.0.0

### Changelog

**v1.0.0 (2026-03-15)**
- ✅ Первый релиз
- ✅ JWT выдача и обновление
- ✅ RS256 асимметричное шифрование
- ✅ MongoDB интеграция
- ✅ Docker поддержка
- ✅ Health checks
- ✅ JWKS endpoint

### Roadmap

**v1.1.0**
- [ ] Rate limiting
- [ ] Redis для refresh токенов
- [ ] Structured logging (winston)
- [ ] Prometheus metrics

**v1.2.0**
- [ ] Ротация ключей
- [ ] 2FA (TOTP)
- [ ] OAuth2 endpoints
- [ ] Admin API для управления пользователями

## Контакты и поддержка

**Репозиторий:** `/home/d-demin/Development/Vibe/Otus/SL/smartlinks-cpp`

**Логи:** `docker logs smartlinks-jwt`

**Health check:** `http://localhost:3000/health`

**JWKS:** `http://localhost:3000/.well-known/jwks.json`

---

**Дата создания:** 2026-03-15
**Последнее обновление:** 2026-03-15
