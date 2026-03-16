# SmartLinks API Gateway

API Gateway на базе Nginx для маршрутизации запросов между микросервисами.

---

## 📋 Описание

API Gateway выполняет роль единой точки входа для всех клиентских запросов и маршрутизирует их к соответствующим микросервисам:

- **`/jwt/*`** → JWT Authentication Service (порт 3000)
- **`/.well-known/jwks.json`** → JWT JWKS endpoint
- **`/*`** (всё остальное) → SmartLinks Service (порт 8080)

---

## 🏗️ Архитектура

```
┌─────────────────────────────────────────────────────────────┐
│                    Client (Browser/App)                     │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
              ┌────────────────────────┐
              │   API Gateway (Nginx)  │
              │     localhost:80       │
              └────────────┬───────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
┌─────────────┐   ┌──────────────┐  ┌──────────────┐
│ JWT Service │   │  SmartLinks  │  │   MongoDB    │
│  (port 3000)│   │  (port 8080) │  │ (port 27017) │
└─────────────┘   └──────────────┘  └──────────────┘
```

---

## 🚀 Запуск

### Запустить все сервисы

```bash
docker-compose up -d
```

### Проверить статус

```bash
docker-compose ps
```

Вы должны увидеть:
```
smartlinks-gateway    running (healthy)   0.0.0.0:80->80/tcp
smartlinks-jwt        running (healthy)   (internal)
smartlinks-app        running             (internal)
smartlinks-mongodb    running (healthy)   0.0.0.0:27017->27017/tcp
```

### Проверить health check gateway

```bash
curl http://localhost/gateway/health
# Ответ: Gateway is healthy
```

---

## 📡 Маршрутизация запросов

### JWT Authentication Service

**Базовый URL:** `http://localhost/jwt/`

**Endpoints:**

| Endpoint | Маршрут | Backend |
|----------|---------|---------|
| `POST /jwt/auth/login` | `/jwt/auth/login` → `/auth/login` | JWT Service:3000 |
| `POST /jwt/auth/refresh` | `/jwt/auth/refresh` → `/auth/refresh` | JWT Service:3000 |
| `POST /jwt/auth/revoke` | `/jwt/auth/revoke` → `/auth/revoke` | JWT Service:3000 |
| `POST /jwt/auth/introspect` | `/jwt/auth/introspect` → `/auth/introspect` | JWT Service:3000 |
| `GET /jwt/health` | `/jwt/health` → `/health` | JWT Service:3000 |

**Примеры:**

```bash
# Login
curl -X POST http://localhost/jwt/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "testuser", "password": "password123"}'

# Health check
curl http://localhost/jwt/health
```

---

### JWKS Endpoint

**URL:** `http://localhost/.well-known/jwks.json`

Специальный маршрут для публичных ключей JWT (без префикса `/jwt/`).

```bash
curl http://localhost/.well-known/jwks.json
```

---

### SmartLinks Service

**Базовый URL:** `http://localhost/`

**Endpoints:**

| Endpoint | Backend |
|----------|---------|
| `GET /{slug}` | SmartLinks:8080 |
| `HEAD /{slug}` | SmartLinks:8080 |

**Примеры:**

```bash
# Проверка короткой ссылки
curl -I http://localhost/test-redirect

# Редирект
curl -L http://localhost/my-link
```

---

## 🔧 Конфигурация

### Nginx конфигурация

**Файл:** `nginx/nginx.conf`

**Основные параметры:**

```nginx
# Upstreams
upstream jwt_service {
    server jwt-service:3000;
    keepalive 32;
}

upstream smartlinks_service {
    server smartlinks:8080;
    keepalive 32;
}

# Маршрутизация /jwt/*
location /jwt/ {
    rewrite ^/jwt/(.*) /$1 break;
    proxy_pass http://jwt_service;
    # ... proxy headers ...
}

# Маршрутизация /*
location / {
    proxy_pass http://smartlinks_service;
    # ... proxy headers ...
}
```

### Таймауты

- `proxy_connect_timeout`: 60s
- `proxy_send_timeout`: 60s
- `proxy_read_timeout`: 60s

### Keepalive

- Keepalive connections: 32
- `proxy_http_version`: 1.1

### Максимальный размер тела запроса

- `client_max_body_size`: 10M

---

## 📊 Логирование

### Логи Nginx

**Access logs:**
```bash
# JWT service
docker exec smartlinks-gateway cat /var/log/nginx/jwt_access.log

# SmartLinks service
docker exec smartlinks-gateway cat /var/log/nginx/smartlinks_access.log

# All requests
docker exec smartlinks-gateway cat /var/log/nginx/access.log
```

**Error logs:**
```bash
# JWT errors
docker exec smartlinks-gateway cat /var/log/nginx/jwt_error.log

# SmartLinks errors
docker exec smartlinks-gateway cat /var/log/nginx/smartlinks_error.log

# Gateway errors
docker exec smartlinks-gateway cat /var/log/nginx/error.log
```

### Live logs

```bash
docker logs -f smartlinks-gateway
```

---

## 🔍 Отладка

### Проверка маршрутизации

```bash
# Проверка что запрос идёт в JWT service
curl -v http://localhost/jwt/health

# Проверка что запрос идёт в SmartLinks
curl -v http://localhost/test-redirect
```

### Проверка upstreams

```bash
# Проверить что backend доступен
docker exec smartlinks-gateway wget -O- http://jwt-service:3000/health
docker exec smartlinks-gateway wget -O- http://smartlinks:8080/test-redirect
```

### Перезагрузка конфигурации без даунтайма

```bash
# Проверить синтаксис
docker exec smartlinks-gateway nginx -t

# Перезагрузить конфигурацию
docker exec smartlinks-gateway nginx -s reload
```

---

## 🚨 Troubleshooting

### Проблема: 502 Bad Gateway

**Причина:** Backend сервис недоступен

**Решение:**
```bash
# Проверить статус backend сервисов
docker-compose ps jwt-service smartlinks

# Проверить логи
docker logs smartlinks-jwt
docker logs smartlinks-app
docker logs smartlinks-gateway
```

---

### Проблема: 404 Not Found для /jwt/*

**Причина:** Некорректная конфигурация маршрутизации

**Решение:**
```bash
# Проверить конфигурацию Nginx
docker exec smartlinks-gateway nginx -t

# Проверить что rewrite работает
docker exec smartlinks-gateway cat /etc/nginx/nginx.conf | grep -A 5 "location /jwt/"
```

---

### Проблема: Таймаут соединения

**Причина:** Backend не отвечает в пределах таймаута

**Решение:**
```bash
# Увеличить таймауты в nginx.conf
proxy_connect_timeout 120s;
proxy_send_timeout 120s;
proxy_read_timeout 120s;

# Перезапустить gateway
docker-compose restart gateway
```

---

## 🔐 Безопасность

### Headers

Gateway добавляет следующие заголовки:

```
X-Real-IP: <client_ip>
X-Forwarded-For: <client_ip>
X-Forwarded-Proto: http (или https)
```

### Rate Limiting (опционально)

Для добавления rate limiting в `nginx.conf`:

```nginx
http {
    # Rate limiting zone
    limit_req_zone $binary_remote_addr zone=jwt_limit:10m rate=10r/s;

    server {
        location /jwt/ {
            limit_req zone=jwt_limit burst=20 nodelay;
            # ... остальная конфигурация ...
        }
    }
}
```

---

## 📈 Производительность

### Настройки производительности

**Worker processes:** `auto` (по количеству CPU)
**Worker connections:** 1024
**Keepalive:** 32 соединения

### Gzip compression

Включено для:
- `text/plain`
- `text/css`
- `application/json`
- `application/javascript`

---

## 🔄 Обновление конфигурации

### 1. Отредактировать конфигурацию

```bash
nano nginx/nginx.conf
```

### 2. Проверить синтаксис

```bash
docker exec smartlinks-gateway nginx -t
```

### 3. Применить изменения

**Вариант A: Reload без даунтайма**
```bash
docker exec smartlinks-gateway nginx -s reload
```

**Вариант B: Полный перезапуск**
```bash
docker-compose restart gateway
```

---

## 📚 Примеры использования

### Полный workflow аутентификации

```bash
# 1. Login через gateway
RESPONSE=$(curl -s -X POST http://localhost/jwt/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username": "testuser", "password": "password123"}')

# 2. Извлечь access token
ACCESS_TOKEN=$(echo $RESPONSE | jq -r '.access_token')

# 3. Использовать токен для запроса в SmartLinks
curl -H "Authorization: Bearer $ACCESS_TOKEN" \
  http://localhost/my-protected-link
```

### Проверка JWKS

```bash
# Получить публичные ключи
curl http://localhost/.well-known/jwks.json | jq
```

### Redirect через SmartLinks

```bash
# Создать короткую ссылку (через прямой доступ к MongoDB или API)
# Затем использовать через gateway
curl -I http://localhost/my-short-link
```

---

## 🏷️ Порты

| Сервис | Внутренний порт | Внешний порт | Доступ |
|--------|-----------------|--------------|--------|
| Gateway | 80 | 80 | `http://localhost` |
| JWT Service | 3000 | - | Только через Gateway |
| SmartLinks | 8080 | - | Только через Gateway |
| MongoDB | 27017 | 27017 | `localhost:27017` |
| Mongo Express | 8081 | 8081 | `http://localhost:8081` |

---

## 📦 Docker Compose

### Запуск

```bash
docker-compose up -d
```

### Остановка

```bash
docker-compose down
```

### Перезапуск только gateway

```bash
docker-compose restart gateway
```

### Логи gateway

```bash
docker-compose logs -f gateway
```

---

## 🎯 Best Practices

1. **Всегда используйте Gateway** для доступа к сервисам
2. **Не открывайте порты** backend сервисов (3000, 8080) наружу
3. **Мониторьте логи** для выявления проблем
4. **Используйте healthcheck** для проверки доступности
5. **Добавьте rate limiting** для защиты от DDoS
6. **Настройте HTTPS** в production (Let's Encrypt)

---

## 🔗 Связанная документация

- [JWT.md](JWT.md) - Документация JWT сервиса
- [README.md](README.md) - Общая документация проекта
- [TESTING.md](TESTING.md) - Документация по тестированию

---

**Автор:** Claude Code
**Дата:** 2026-03-15
**Версия:** 1.0.0
