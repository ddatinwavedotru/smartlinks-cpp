# SmartLinks Integration Architecture

Документация интеграций между компонентами системы SmartLinks.

**Версия:** 1.0.0
**Дата:** 2026-03-16
**Статус:** Production-ready

---

## 📋 Содержание

1. [Архитектурный обзор](#архитектурный-обзор)
2. [Компоненты системы](#компоненты-системы)
3. [Сетевая топология](#сетевая-топология)
4. [Протоколы взаимодействия](#протоколы-взаимодействия)
5. [API Endpoints](#api-endpoints)
6. [Persistent Storage](#persistent-storage)
7. [Security](#security)
8. [Healthchecks](#healthchecks)
9. [External Access](#external-access)

---

## Архитектурный обзор

SmartLinks построен как распределенная микросервисная система, развернутая в Docker-контейнерах с использованием Docker Compose.

### Основные принципы

- **Reverse Proxy Pattern**: Nginx Gateway как единая точка входа
- **Service Mesh**: Внутренняя сеть Docker для межсервисного взаимодействия
- **Data Persistence**: Docker volumes для критических данных
- **Health Monitoring**: Healthcheck endpoints для всех сервисов
- **Security Layers**: API Gateway → JWT Authentication → Authorization

### Diagram

См. `diagrams/integrations.mmd` для визуального представления.

---

## Компоненты системы

### 1. Client Browser (External)

**Тип:** External User Agent
**Протокол:** HTTP/HTTPS
**Назначение:** Внешний пользователь, взаимодействующий с системой через браузер или HTTP-клиент

**Capabilities:**
- Отправка HTTP-запросов к API Gateway
- Получение редиректов от SmartLinks
- Аутентификация через JWT Service
- Опциональный прямой доступ к сервисам (для development)

---

### 2. API Gateway (Nginx)

**Image:** `nginx:alpine`
**Published Port:** `80` → `localhost:80`
**Network:** `smartlinks-network`
**Config:** `/nginx/nginx.conf`

**Назначение:** Единая точка входа для всех HTTP-запросов, маршрутизация на backend сервисы

#### Routing Rules

```nginx
# Healthcheck
GET /gateway/health → 200 OK (local response)

# JWT Service (with prefix strip)
/jwt/* → http://jwt-service:3000/* (strip /jwt prefix)

# JWKS Endpoint (direct proxy)
/.well-known/jwks.json → http://jwt-service:3000/.well-known/jwks.json

# SmartLinks Service (default route)
/* → http://smartlinks:8080/*
```

#### Features

- **Keepalive connections**: 32 connections pool для каждого upstream
- **Gzip compression**: Для JSON, JavaScript, CSS, fonts
- **Access logging**: Раздельные логи для jwt_access.log и smartlinks_access.log
- **Timeouts**: 60s для connect/send/read
- **Error handling**: Custom 502/503/504 pages

#### Healthcheck

```bash
curl http://localhost/gateway/health
# Response: "Gateway is healthy"
```

---

### 3. SmartLinks Microservice (C++)

**Image:** Custom multi-stage (Ubuntu 22.04 base)
**Exposed Port:** `8080` (NOT published, only via Gateway)
**Network:** `smartlinks-network`
**Language:** C++ (ISO C++17)
**Framework:** Drogon HTTP Framework

**Назначение:** Обработка redirect запросов с DSL rules и JWT authorization

#### Build Stages

1. **Base**: Установка зависимостей (Drogon, MongoDB C/C++ drivers, jwt-cpp)
2. **Builder**: Компиляция SmartLinks бинарника
3. **Runtime**: Минимальный образ с бинарником и shared libraries

#### Dependencies

**Compile-time:**
- Drogon framework (HTTP server + middleware)
- MongoDB C driver (libmongoc 1.24.4)
- MongoDB C++ driver (mongocxx 3.8.0)
- jwt-cpp v0.7.2 (header-only)
- OpenSSL, libcurl, jsoncpp, uuid

**Runtime:**
- libssl3, libjsoncpp25, libuuid1, libcurl4
- mongocxx/bsoncxx shared libraries

#### Endpoints

```http
GET /:link_alias
  → 307 Temporary Redirect (with DSL evaluation)
  → 404 Not Found (link not exists or deleted)
  → 422 Unprocessable Entity (link frozen)
  → 405 Method Not Allowed (only GET/HEAD)
  → 429 Too Many Requests (no matching rules)
```

#### External Integrations

**MongoDB:**
- **Protocol**: MongoDB Wire Protocol (mongocxx driver)
- **Operations**:
  - Read: `links` collection (rules_dsl field)
  - Write: State updates (freeze/unfreeze)
- **Connection**: `mongodb://root:password@mongodb:27017/Links`

**JWT Service:**
- **Protocol**: HTTP GET (libcurl)
- **Endpoint**: `http://jwt-service:3000/.well-known/jwks.json`
- **Purpose**: Fetch JWKS public keys for RS256 JWT validation
- **Caching**: Public keys cached in memory

#### Configuration

```bash
# Environment variables (docker-compose.yml)
MONGO_URI=mongodb://root:password@mongodb:27017/
MONGO_DB=Links
JWT_JWKS_URI=http://jwt-service:3000/.well-known/jwks.json
```

**ВАЖНО**: `JWT_JWKS_URI` должен содержать полный путь к JWKS endpoint, включая `/.well-known/jwks.json`!

---

### 4. JWT Service (Node.js)

**Image:** `node:18-alpine`
**Published Port:** `3000` → `localhost:3000`
**Network:** `smartlinks-network`
**Language:** JavaScript (Node.js 18)
**Main File:** `src/server.js`

**Назначение:** JWT authentication, token management, user administration

#### Features

- **RS256 JWT signing**: RSA key pair (2048 bits)
- **JWKS endpoint**: Public keys export для валидации
- **Token lifecycle**: Access tokens (10m) + Refresh tokens (14d)
- **User management**: Registration, login, token revocation
- **Admin panel**: User manipulation API с Basic Auth

#### Endpoints

```http
# JWKS Public Keys
GET /.well-known/jwks.json
  → 200 OK { keys: [ { kty, use, alg, kid, n, e } ] }

# Authentication
POST /auth/login
  Body: { username, password }
  → 200 OK { access_token, refresh_token, expires_in }

POST /auth/refresh
  Body: { refresh_token }
  → 200 OK { access_token, expires_in }

POST /auth/revoke
  Body: { refresh_token }
  → 200 OK { message: "Token revoked" }

# Token Introspection
POST /auth/introspect
  Body: { token }
  → 200 OK { active: true/false, ... }

# Admin Panel (Basic Auth: admin/&Admin123)
GET /auth/manipulate/
  → 200 OK (HTML page with user management UI)

GET /auth/admin/users
  → 200 OK [ { _id, username, email, roles, created_at } ]

POST /auth/admin/register
  Body: { username, password, email, roles }
  → 201 Created { user_id, username }

# Healthcheck
GET /health
  → 200 OK { status: "healthy", timestamp, uptime }
```

#### External Integrations

**MongoDB:**
- **Protocol**: MongoDB Wire Protocol (mongodb npm driver)
- **Collections**: `users`, `refresh_tokens`
- **Connection**: `mongodb://root:password@mongodb:27017/Links`

**RSA Keys:**
- **Location**: `/app/keys/` (Docker volume `jwt_keys`)
- **Generation**: entrypoint.sh script при первом запуске
- **Persistence**: Volume mount для сохранения ключей между перезапусками

#### Configuration

```bash
# Environment variables
MONGO_URI=mongodb://root:password@mongodb:27017
MONGO_DB=Links
ACCESS_TOKEN_TTL=10m
REFRESH_TOKEN_TTL=14d
JWT_ISSUER=smartlinks-jwt-service
JWT_AUDIENCE=smartlinks-api
PORT=3000
```

#### Healthcheck

```bash
curl http://localhost:3000/health
# Response: { "status": "healthy", "timestamp": "...", "uptime": 123 }
```

---

### 5. MongoDB

**Image:** `mongo:7.0`
**Published Port:** `27017` → `localhost:27017`
**Network:** `smartlinks-network`
**Volume:** `mongodb_data` → `/data/db`

**Назначение:** Persistent NoSQL database для хранения пользователей, ссылок и правил

#### Authentication

```bash
MONGO_INITDB_ROOT_USERNAME=root
MONGO_INITDB_ROOT_PASSWORD=password
```

#### Collections

**Database: `Links`**

1. **`links`** - SmartLinks collection
   ```json
   {
     "_id": ObjectId,
     "alias": "short-link",
     "rules_dsl": [
       { "condition": "LANGUAGE = en-US", "redirect_url": "https://example.com" }
     ],
     "created_at": ISODate,
     "is_deleted": false,
     "is_frozen": false
   }
   ```

2. **`users`** - JWT users
   ```json
   {
     "_id": ObjectId,
     "username": "john",
     "password_hash": "bcrypt$...",
     "email": "john@example.com",
     "roles": ["user"],
     "created_at": ISODate
   }
   ```

3. **`refresh_tokens`** - Active refresh tokens
   ```json
   {
     "_id": ObjectId,
     "user_id": ObjectId,
     "token_hash": "sha256...",
     "expires_at": ISODate,
     "revoked": false
   }
   ```

#### Healthcheck

```bash
mongosh --eval 'db.runCommand({ ping: 1 })'
# Response: { ok: 1 }
```

#### Clients

- **SmartLinks**: mongocxx 3.8.0
- **JWT Service**: mongodb npm driver
- **Mongo Express**: mongodb npm driver

---

### 6. Mongo Express

**Image:** `mongo-express:latest`
**Published Port:** `8081` → `localhost:8081`
**Network:** `smartlinks-network`

**Назначение:** Web-based MongoDB admin interface

#### Configuration

```bash
ME_CONFIG_MONGODB_ADMINUSERNAME=root
ME_CONFIG_MONGODB_ADMINPASSWORD=password
ME_CONFIG_MONGODB_URL=mongodb://root:password@mongodb:27017/
ME_CONFIG_BASICAUTH_USERNAME=admin
ME_CONFIG_BASICAUTH_PASSWORD=pass
```

#### Access

```
URL: http://localhost:8081
Basic Auth: admin / pass
```

#### Features

- View/edit collections
- Execute MongoDB queries
- Database statistics
- Index management
- Import/export data

---

## Сетевая топология

### Docker Network

**Name:** `smartlinks-network`
**Driver:** `bridge`
**Subnet:** Автоматически назначается Docker

#### Internal DNS

Docker автоматически создает DNS записи для сервисов:

```
mongodb        → 172.x.x.x:27017
mongo-express  → 172.x.x.x:8081
jwt-service    → 172.x.x.x:3000
smartlinks     → 172.x.x.x:8080
gateway        → 172.x.x.x:80
```

#### Service Discovery

Все сервисы обращаются друг к другу по **имени контейнера** (Docker DNS resolution):

```nginx
# Nginx upstream
upstream jwt_service {
    server jwt-service:3000;  # Resolved by Docker DNS
}
```

```cpp
// SmartLinks mongocxx connection
mongocxx::uri uri{"mongodb://root:password@mongodb:27017/"};
                                        // ↑ Docker DNS name
```

---

## Протоколы взаимодействия

### HTTP/1.1

**Используется:**
- Client ↔ Gateway (external)
- Gateway ↔ SmartLinks (internal proxy)
- Gateway ↔ JWT Service (internal proxy)
- SmartLinks → JWT Service (JWKS fetch via libcurl)
- Client ↔ Mongo Express (external, web UI)

**Features:**
- Keepalive connections (Connection: keep-alive)
- Gzip compression
- Chunked transfer encoding

---

### MongoDB Wire Protocol

**Используется:**
- SmartLinks → MongoDB (mongocxx driver)
- JWT Service → MongoDB (mongodb npm driver)
- Mongo Express → MongoDB (admin UI)

**Connection String Format:**
```
mongodb://[username:password@]host[:port]/[database][?options]
```

**Authentication:** SCRAM-SHA-256 (default for MongoDB 7.0)

---

### TLS/SSL

**Текущее состояние:** Не используется (development environment)

**Production рекомендации:**
- HTTPS для Gateway (Let's Encrypt)
- TLS для MongoDB connections (`?tls=true`)
- Certificate-based authentication для inter-service communication

---

## API Endpoints

### Gateway (Port 80)

| Method | Path | Target | Description |
|--------|------|--------|-------------|
| GET | `/gateway/health` | Local | Gateway healthcheck |
| GET/POST | `/jwt/*` | JWT Service | JWT API (strip `/jwt` prefix) |
| GET | `/.well-known/jwks.json` | JWT Service | JWKS public keys |
| GET | `/:link_alias` | SmartLinks | SmartLink redirect |

---

### JWT Service (Port 3000)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/.well-known/jwks.json` | None | JWKS public keys |
| POST | `/auth/login` | None | User login |
| POST | `/auth/refresh` | None | Refresh access token |
| POST | `/auth/revoke` | None | Revoke refresh token |
| POST | `/auth/introspect` | None | Token validation |
| GET | `/auth/manipulate/` | Basic | Admin UI (HTML) |
| GET | `/auth/admin/users` | Basic | List all users |
| POST | `/auth/admin/register` | Basic | Create new user |
| GET | `/health` | None | Service healthcheck |

**Basic Auth:** `admin:&Admin123`

---

### SmartLinks (Port 8080, via Gateway only)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET/HEAD | `/:link_alias` | Optional JWT | SmartLink redirect with DSL evaluation |

**Response Codes:**
- `307 Temporary Redirect` - Successful DSL match
- `404 Not Found` - Link doesn't exist or deleted
- `405 Method Not Allowed` - Only GET/HEAD allowed
- `422 Unprocessable Entity` - Link is frozen
- `429 Too Many Requests` - No matching DSL rules

---

### Mongo Express (Port 8081)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/` | Basic | MongoDB admin UI |

**Basic Auth:** `admin:pass`

---

## Persistent Storage

### Docker Volumes

#### 1. `mongodb_data`

**Mount:** `/data/db` в MongoDB контейнере
**Назначение:** Persistent storage для MongoDB data files
**Размер:** Не ограничен (зависит от диска хоста)

**Содержимое:**
- WiredTiger storage engine files
- Collection data (BSON)
- Indexes
- Journal files

**Backup:**
```bash
# Export database
docker exec mongodb mongodump --out /backup

# Restore database
docker exec mongodb mongorestore /backup
```

---

#### 2. `jwt_keys`

**Mount:** `/app/keys` в JWT Service контейнере
**Назначение:** Persistent storage для RSA key pair
**Генерация:** `entrypoint.sh` при первом запуске

**Файлы:**
- `private.key` - RSA-2048 private key (PKCS#8 PEM)
- `public.key` - RSA-2048 public key (PKCS#8 PEM)

**ВАЖНО:** Ключи должны сохраняться между перезапусками, иначе все выданные JWT токены станут невалидными!

**Rotation:**
```bash
# Удалить volume для генерации новых ключей
docker volume rm jwt_keys
docker-compose up jwt-service

# ⚠️ Все существующие JWT токены станут invalid!
```

---

## Security

### Authentication Layers

1. **Client → Gateway**: Без аутентификации (public access)
2. **Gateway → Services**: Internal Docker network (network isolation)
3. **SmartLinks → MongoDB**: Username/password (`root:password`)
4. **JWT Service → MongoDB**: Username/password (`root:password`)
5. **JWT Admin Panel**: HTTP Basic Auth (`admin:&Admin123`)
6. **Mongo Express**: HTTP Basic Auth (`admin:pass`)

---

### Authorization Flow

#### JWT-based Authorization (для protected SmartLinks)

```
1. Client → Gateway: GET /jwt/auth/login
   Body: { username, password }

2. Gateway → JWT Service: POST /auth/login

3. JWT Service → MongoDB: Validate user credentials

4. JWT Service → Client: { access_token, refresh_token }

5. Client → Gateway: GET /:link_alias
   Header: Authorization: Bearer <access_token>

6. Gateway → SmartLinks: GET /:link_alias
   Header: Authorization: Bearer <access_token>

7. SmartLinks → JWT Service: GET /.well-known/jwks.json
   (fetch public keys for validation)

8. SmartLinks: Validate JWT signature with JWKS public key

9. SmartLinks → MongoDB: Fetch DSL rules

10. SmartLinks: Evaluate DSL (AUTHORIZED keyword checks JWT)

11. SmartLinks → Client: 307 Redirect (if authorized)
                         429 Too Many Requests (if not authorized)
```

---

### Security Recommendations (Production)

1. **HTTPS Everywhere**:
   - Nginx с Let's Encrypt certificates
   - Redirect HTTP → HTTPS

2. **MongoDB Authentication**:
   - Создать отдельные пользователи для каждого сервиса
   - Использовать TLS для MongoDB connections
   - Ограничить IP-адреса через `bind_ip`

3. **JWT Security**:
   - Rotate RSA keys периодически
   - Использовать short-lived access tokens (5-15 min)
   - Implement token revocation blacklist

4. **Network Isolation**:
   - Не публиковать порты SmartLinks (8080) - только через Gateway
   - Не публиковать MongoDB (27017) для production
   - Использовать Docker secrets для паролей

5. **Rate Limiting**:
   - Nginx rate limiting для публичных endpoints
   - MongoDB connection pooling limits

6. **Admin Access**:
   - Сменить Basic Auth credentials
   - Использовать VPN для доступа к Mongo Express
   - Двухфакторная аутентификация для админ-панели

---

## Healthchecks

### Docker Compose Healthchecks

#### MongoDB
```yaml
healthcheck:
  test: ["CMD", "mongosh", "--eval", "db.runCommand({ ping: 1 })"]
  interval: 10s
  timeout: 5s
  retries: 5
  start_period: 40s
```

#### JWT Service
```yaml
healthcheck:
  test: ["CMD", "wget", "--spider", "-q", "http://localhost:3000/health"]
  interval: 10s
  timeout: 5s
  retries: 3
  start_period: 10s
```

#### Gateway
```yaml
healthcheck:
  test: ["CMD", "wget", "--spider", "-q", "http://localhost/gateway/health"]
  interval: 10s
  timeout: 5s
  retries: 3
  start_period: 5s
```

---

### Service Dependencies

```
Gateway depends_on:
  - jwt-service (healthy)
  - smartlinks

SmartLinks depends_on:
  - mongodb (healthy)

JWT Service depends_on:
  - mongodb (healthy)

Mongo Express depends_on:
  - mongodb
```

**Startup Order:** MongoDB → JWT Service → SmartLinks → Gateway

---

## External Access

### Published Ports (localhost)

| Port | Service | Access | Description |
|------|---------|--------|-------------|
| `80` | Gateway | Public | Main entry point |
| `3000` | JWT Service | Development | Direct JWT API access |
| `8081` | Mongo Express | Admin | MongoDB web UI |
| `27017` | MongoDB | Development | Direct DB access |

### Production Port Configuration

**Рекомендация для production:**

```yaml
# Публиковать ТОЛЬКО Gateway
gateway:
  ports:
    - "443:443"  # HTTPS
    - "80:80"    # HTTP → HTTPS redirect

# Все остальные сервисы - expose only (internal network)
jwt-service:
  expose:
    - "3000"
  # НЕ публиковать порт!

mongodb:
  expose:
    - "27017"
  # НЕ публиковать порт!

mongo-express:
  # Доступ только через VPN или SSH tunnel
  ports:
    - "127.0.0.1:8081:8081"  # Bind только к localhost
```

---

## Диаграмма интеграций

См. `diagrams/integrations.mmd` для визуального представления всех интеграций.

**Инструкция:**
1. Открыть https://mermaid.js.org/
2. Выбрать "Live Editor"
3. Скопировать содержимое `diagrams/integrations.mmd`
4. Экспортировать как PNG/SVG

---

## Примеры использования

### 1. Client получает JWT токен

```bash
# Login
curl -X POST http://localhost/jwt/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"john","password":"secret123"}'

# Response:
# {
#   "access_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",
#   "refresh_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",
#   "expires_in": 600
# }
```

---

### 2. Client использует SmartLink с JWT авторизацией

```bash
# Request to protected SmartLink
curl -i http://localhost/my-link \
  -H "Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9..."

# Response:
# HTTP/1.1 307 Temporary Redirect
# Location: https://private-resource.com
```

---

### 3. SmartLinks проверяет JWT токен

**Внутренний процесс:**

1. SmartLinks извлекает Bearer token из заголовка
2. SmartLinks делает HTTP GET к `http://jwt-service:3000/.well-known/jwks.json`
3. SmartLinks валидирует JWT signature с помощью JWKS public key
4. SmartLinks проверяет claims: `iss`, `aud`, `exp`, `nbf`
5. DSL evaluator использует результат валидации для `AUTHORIZED` keyword

---

### 4. Прямой доступ к MongoDB (development only)

```bash
# MongoDB Shell
mongosh "mongodb://root:password@localhost:27017/Links"

# List collections
show collections

# Query links
db.links.find().pretty()
```

---

### 5. Управление пользователями через Admin API

```bash
# List users
curl http://localhost/jwt/auth/admin/users \
  -u admin:'&Admin123'

# Register new user
curl -X POST http://localhost/jwt/auth/admin/register \
  -u admin:'&Admin123' \
  -H "Content-Type: application/json" \
  -d '{
    "username": "alice",
    "password": "secret456",
    "email": "alice@example.com",
    "roles": ["user"]
  }'
```

---

## Troubleshooting

### Проблема: Gateway не может достучаться до SmartLinks

**Симптомы:** `502 Bad Gateway` при обращении к `/:link_alias`

**Решения:**
1. Проверить, что SmartLinks запущен:
   ```bash
   docker-compose ps smartlinks
   ```
2. Проверить логи SmartLinks:
   ```bash
   docker-compose logs smartlinks
   ```
3. Проверить Docker DNS resolution:
   ```bash
   docker exec gateway ping smartlinks
   ```

---

### Проблема: JWT валидация падает с ошибкой

**Симптомы:** `429 Too Many Requests` для `AUTHORIZED` правил

**Решения:**
1. Проверить `JWT_JWKS_URI` в SmartLinks env:
   ```bash
   docker-compose exec smartlinks env | grep JWT_JWKS_URI
   # Должно быть: http://jwt-service:3000/.well-known/jwks.json
   ```
2. Проверить доступность JWKS endpoint:
   ```bash
   docker-compose exec smartlinks curl http://jwt-service:3000/.well-known/jwks.json
   ```
3. Проверить формат JWT токена:
   ```bash
   # Декодировать токен
   echo "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9..." | \
     base64 -d | jq .
   ```

---

### Проблема: MongoDB connection timeout

**Симптомы:** `MongoTimeoutError: Server selection timed out`

**Решения:**
1. Проверить healthcheck MongoDB:
   ```bash
   docker-compose ps mongodb
   # Должен быть: healthy
   ```
2. Проверить MongoDB logs:
   ```bash
   docker-compose logs mongodb
   ```
3. Проверить credentials:
   ```bash
   docker-compose exec mongodb mongosh \
     -u root -p password --eval "db.runCommand({ ping: 1 })"
   ```

---

### Проблема: Volume permissions

**Симптомы:** `Permission denied` при записи в `/data/db` или `/app/keys`

**Решения:**
1. Проверить владельца volume:
   ```bash
   docker volume inspect mongodb_data
   docker volume inspect jwt_keys
   ```
2. Изменить permissions (Linux):
   ```bash
   # Для mongodb_data
   sudo chown -R 999:999 /var/lib/docker/volumes/mongodb_data/_data

   # Для jwt_keys
   sudo chown -R 1000:1000 /var/lib/docker/volumes/jwt_keys/_data
   ```

---

## Связанная документация

- [ARCHITECTURE.md](./ARCHITECTURE.md) - Архитектура SmartLinks микросервиса
- [GATEWAY.md](./GATEWAY.md) - Конфигурация API Gateway
- [JWT.md](./JWT.md) - JWT Service документация
- [README.md](./README.md) - Главная документация проекта
- [TESTING.md](./TESTING.md) - Тестирование и CI/CD
- [diagrams/README.md](./diagrams/README.md) - Архитектурные диаграммы

---

## Метрики и мониторинг

### Healthcheck Endpoints

```bash
# Gateway
curl http://localhost/gateway/health

# JWT Service
curl http://localhost:3000/health

# MongoDB (через mongosh)
docker exec mongodb mongosh --eval 'db.runCommand({ ping: 1 })'
```

### Логи

```bash
# Все сервисы
docker-compose logs -f

# Конкретный сервис
docker-compose logs -f smartlinks
docker-compose logs -f jwt-service
docker-compose logs -f gateway

# Nginx access logs
docker exec gateway tail -f /var/log/nginx/smartlinks_access.log
docker exec gateway tail -f /var/log/nginx/jwt_access.log
```

### Метрики контейнеров

```bash
# Resource usage
docker stats

# Inspect specific container
docker inspect smartlinks
```

---

**Автор:** SmartLinks Development Team
**Последнее обновление:** 2026-03-16
**Контакт:** <noreply@anthropic.com>
