# SmartLinks: Information Schema

Документ описывает структуру данных MongoDB для проекта SmartLinks.

**Версия:** 1.0.0
**Дата:** 2026-03-16
**Статус:** Production

---

## 📋 Содержание

1. [Обзор базы данных](#обзор-базы-данных)
2. [Коллекция: links](#коллекция-links)
3. [Коллекция: users](#коллекция-users)
4. [Коллекция: refresh_tokens](#коллекция-refresh_tokens)
5. [Индексы](#индексы)
6. [Связи между коллекциями](#связи-между-коллекциями)
7. [Миграции и версионирование](#миграции-и-версионирование)

---

## Обзор базы данных

### Databases

| Database | Назначение | Режим | Используется |
|----------|-----------|-------|--------------|
| **Links** | Production database | JSON (Legacy) | Production SmartLinks Service |
| **LinksTest** | Testing database | JSON (Legacy) | Integration tests (JSON mode) |
| **LinksDSL** | Production database | DSL | Production SmartLinks Service (DSL mode) |
| **LinksDSLTest** | Testing database | DSL | Integration tests (DSL mode) |

### Collections Overview

| Collection | Назначение | Записей (prod) | Размер документа |
|------------|-----------|----------------|------------------|
| **links** | Умные ссылки с правилами редиректа | ~100-10000 | 1-5 KB |
| **users** | Пользователи для JWT аутентификации | ~10-1000 | 0.5 KB |
| **refresh_tokens** | Refresh токены для JWT | ~100-10000 | 0.5 KB |

---

## Коллекция: links

Основная коллекция для хранения умных ссылок с правилами редиректа.

### Режим JSON (Legacy)

**Структура:**

```javascript
{
  "_id": ObjectId("507f1f77bcf86cd799439011"),  // MongoDB ID
  "slug": "/my-link",                            // Уникальный алиас ссылки
  "state": "active",                             // Состояние: active, deleted, freezed
  "rules": [                                     // Массив правил редиректа
    {
      "language": "ru-RU",                       // Язык (ru-RU, en-US, any)
      "redirect": "https://example.ru",          // Целевой URL
      "start": ISODate("2024-01-01T00:00:00Z"), // Начало действия правила
      "end": ISODate("2025-12-31T23:59:59Z")    // Окончание действия правила
    }
  ],
  "created_at": ISODate("2024-01-01T12:00:00Z"), // Дата создания (опционально)
  "updated_at": ISODate("2024-06-01T15:30:00Z")  // Дата обновления (опционально)
}
```

**Поля:**

| Поле | Тип | Обязательное | Описание | Constraints |
|------|-----|--------------|----------|-------------|
| `_id` | ObjectId | Авто | MongoDB document ID | Уникальный |
| `slug` | String | ✅ Да | Уникальный алиас ссылки | Unique index, начинается с `/` |
| `state` | String | ✅ Да | Состояние ссылки | Enum: `active`, `deleted`, `freezed` |
| `rules` | Array | ✅ Да | Массив правил редиректа | Может быть пустым `[]` |
| `rules[].language` | String | ✅ Да | Язык или "any" | BCP 47 format или "any" |
| `rules[].redirect` | String | ✅ Да | Целевой URL для редиректа | Valid URL (HTTP/HTTPS) |
| `rules[].start` | ISODate | ✅ Да | Начало действия правила | ISO 8601 datetime |
| `rules[].end` | ISODate | ✅ Да | Окончание действия правила | ISO 8601 datetime, >= start |
| `created_at` | ISODate | ❌ Нет | Дата создания ссылки | ISO 8601 datetime |
| `updated_at` | ISODate | ❌ Нет | Дата последнего обновления | ISO 8601 datetime |

**Пример документа:**

```javascript
{
  "_id": ObjectId("507f1f77bcf86cd799439011"),
  "slug": "/summer-sale",
  "state": "active",
  "rules": [
    {
      "language": "ru-RU",
      "redirect": "https://example.ru/letnaya-rasprodazha",
      "start": ISODate("2026-06-01T00:00:00Z"),
      "end": ISODate("2026-08-31T23:59:59Z")
    },
    {
      "language": "en-US",
      "redirect": "https://example.com/summer-sale",
      "start": ISODate("2026-06-01T00:00:00Z"),
      "end": ISODate("2026-08-31T23:59:59Z")
    },
    {
      "language": "any",
      "redirect": "https://example.com",
      "start": ISODate("2020-01-01T00:00:00Z"),
      "end": ISODate("2099-12-31T23:59:59Z")
    }
  ],
  "created_at": ISODate("2026-03-16T12:00:00Z"),
  "updated_at": ISODate("2026-03-16T12:00:00Z")
}
```

---

### Режим DSL (Current)

**Структура:**

```javascript
{
  "_id": ObjectId("507f1f77bcf86cd799439011"),  // MongoDB ID
  "slug": "/my-link",                            // Уникальный алиас ссылки
  "state": "active",                             // Состояние: active, deleted, freezed
  "rules_dsl": "LANGUAGE = ru-RU -> https://example.ru; LANGUAGE = en-US -> https://example.com",
  "created_at": ISODate("2024-01-01T12:00:00Z"), // Дата создания (опционально)
  "updated_at": ISODate("2024-06-01T15:30:00Z")  // Дата обновления (опционально)
}
```

**Поля:**

| Поле | Тип | Обязательное | Описание | Constraints |
|------|-----|--------------|----------|-------------|
| `_id` | ObjectId | Авто | MongoDB document ID | Уникальный |
| `slug` | String | ✅ Да | Уникальный алиас ссылки | Unique index, начинается с `/` |
| `state` | String | ✅ Да | Состояние ссылки | Enum: `active`, `deleted`, `freezed` |
| `rules_dsl` | String | ✅ Да | DSL правила в текстовом формате | Валидный DSL синтаксис |
| `created_at` | ISODate | ❌ Нет | Дата создания ссылки | ISO 8601 datetime |
| `updated_at` | ISODate | ❌ Нет | Дата последнего обновления | ISO 8601 datetime |

**Пример документа:**

```javascript
{
  "_id": ObjectId("507f1f77bcf86cd799439011"),
  "slug": "/premium-content",
  "state": "active",
  "rules_dsl": "AUTHORIZED AND LANGUAGE = en-US -> https://example.com/premium/en; AUTHORIZED AND LANGUAGE = ru-RU -> https://example.ru/premium; AUTHORIZED -> https://example.com/premium; LANGUAGE = en-US -> https://example.com/signup; DATETIME < 9999-12-31T23:59:59Z -> https://example.com",
  "created_at": ISODate("2026-03-16T12:00:00Z"),
  "updated_at": ISODate("2026-03-16T14:30:00Z")
}
```

**DSL Syntax:**

```ebnf
Rules        = Rule { ";" Rule }
Rule         = Condition "->" URL
Condition    = OrExpression
OrExpression = AndExpression { "OR" AndExpression }
AndExpression= PrimaryExpr { "AND" PrimaryExpr }
PrimaryExpr  = LANGUAGE_OP | DATETIME_OP | AUTHORIZED | "(" OrExpression ")"
```

Подробнее см. [DSL_GRAMMAR.md](./DSL_GRAMMAR.md)

---

### State Transitions

```
┌─────────┐
│ (new)   │
└────┬────┘
     │
     v
┌─────────┐     freeze      ┌─────────┐
│ active  ├─────────────────>│ freezed │
└─────┬───┘                  └────┬────┘
      │                           │
      │ delete            unfreeze│
      │                           │
      v                           v
┌─────────┐                  ┌─────────┐
│ deleted │<─────────────────┤ active  │
└─────────┘       delete     └─────────┘
```

**Правила переходов:**

| Из состояния | В состояние | Операция | HTTP Status после перехода |
|--------------|-------------|----------|----------------------------|
| active | freezed | Freeze | 422 Unprocessable Entity |
| freezed | active | Unfreeze | 307 Temporary Redirect |
| active | deleted | Delete | 404 Not Found |
| freezed | deleted | Delete | 404 Not Found |

---

## Коллекция: users

Пользователи для JWT аутентификации.

**Структура:**

```javascript
{
  "_id": ObjectId("507f1f77bcf86cd799439012"),
  "username": "testuser",
  "password_hash": "$2b$10$yIqlNxsCShTSeRwlT0bdzOJSwF9BM55lJ/qPdyWu/1ZVf05KYV/A2",
  "email": "testuser@example.com",
  "active": true,
  "created_at": ISODate("2026-03-16T12:00:00Z"),
  "roles": ["user", "admin"]  // Опционально
}
```

**Поля:**

| Поле | Тип | Обязательное | Описание | Constraints |
|------|-----|--------------|----------|-------------|
| `_id` | ObjectId | Авто | MongoDB document ID | Уникальный |
| `username` | String | ✅ Да | Уникальное имя пользователя | Unique, 3-50 символов |
| `password_hash` | String | ✅ Да | Bcrypt hash пароля | Bcrypt hash (bcrypt rounds: 10) |
| `email` | String | ✅ Да | Email адрес | Valid email format |
| `active` | Boolean | ✅ Да | Активен ли пользователь | true/false |
| `created_at` | ISODate | ✅ Да | Дата создания пользователя | ISO 8601 datetime |
| `roles` | Array[String] | ❌ Нет | Роли пользователя | ["user", "admin", "manager"] |

**Пример документа:**

```javascript
{
  "_id": ObjectId("507f1f77bcf86cd799439012"),
  "username": "john.doe",
  "password_hash": "$2b$10$yIqlNxsCShTSeRwlT0bdzOJSwF9BM55lJ/qPdyWu/1ZVf05KYV/A2",
  "email": "john.doe@example.com",
  "active": true,
  "created_at": ISODate("2026-03-16T12:00:00Z"),
  "roles": ["user", "manager"]
}
```

**Password Hashing:**

```javascript
// Bcrypt с 10 rounds
const bcrypt = require('bcrypt');
const passwordHash = await bcrypt.hash("password123", 10);
// Результат: $2b$10$yIqlNxsCShTSeRwlT0bdzOJSwF9BM55lJ/qPdyWu/1ZVf05KYV/A2
```

---

## Коллекция: refresh_tokens

Refresh токены для JWT аутентификации (создается JWT Service).

**Структура:**

```javascript
{
  "_id": ObjectId("507f1f77bcf86cd799439013"),
  "jti": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "user_id": ObjectId("507f1f77bcf86cd799439012"),
  "username": "testuser",
  "token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_at": ISODate("2026-03-30T12:00:00Z"),
  "created_at": ISODate("2026-03-16T12:00:00Z"),
  "revoked": false,
  "revoked_at": null  // Опционально
}
```

**Поля:**

| Поле | Тип | Обязательное | Описание | Constraints |
|------|-----|--------------|----------|-------------|
| `_id` | ObjectId | Авто | MongoDB document ID | Уникальный |
| `jti` | String | ✅ Да | JWT Token ID (UUID) | Уникальный, UUID v4 |
| `user_id` | ObjectId | ✅ Да | Ссылка на users._id | Foreign key to users |
| `username` | String | ✅ Да | Имя пользователя (денормализация) | Duplicate from users |
| `token` | String | ✅ Да | JWT refresh token | Полный JWT токен |
| `expires_at` | ISODate | ✅ Да | Дата истечения токена | ISO 8601 datetime, NOW + 14 дней |
| `created_at` | ISODate | ✅ Да | Дата создания токена | ISO 8601 datetime |
| `revoked` | Boolean | ✅ Да | Отозван ли токен | true/false, default: false |
| `revoked_at` | ISODate | ❌ Нет | Дата отзыва токена | ISO 8601 datetime, заполняется при revoke |

**Пример документа:**

```javascript
{
  "_id": ObjectId("507f1f77bcf86cd799439013"),
  "jti": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "user_id": ObjectId("507f1f77bcf86cd799439012"),
  "username": "john.doe",
  "token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6IjZmNTE5MWY3MzYwOTg5NWMifQ.eyJzdWIiOiI1MDdmMWY3N2JjZjg2Y2Q3OTk0MzkwMTIiLCJqdGkiOiJhMWIyYzNkNC1lNWY2LTc4OTAtYWJjZC1lZjEyMzQ1Njc4OTAiLCJ0eXBlIjoicmVmcmVzaCIsImlhdCI6MTcxMDU5NTIwMCwiZXhwIjoxNzExODA0ODAwLCJpc3MiOiJzbWFydGxpbmtzLWp3dC1zZXJ2aWNlIiwiYXVkIjoic21hcnRsaW5rcy1hcGkifQ...",
  "expires_at": ISODate("2026-03-30T12:00:00Z"),
  "created_at": ISODate("2026-03-16T12:00:00Z"),
  "revoked": false,
  "revoked_at": null
}
```

**Token Lifecycle:**

```
1. Login (POST /auth/login)
   → Создание refresh token
   → Вставка в refresh_tokens (revoked: false)

2. Refresh (POST /auth/refresh)
   → Проверка jti и revoked: false
   → Выдача нового access token

3. Revoke (POST /auth/revoke)
   → Обновление: revoked: true, revoked_at: NOW
   → Токен больше не может быть использован

4. Cleanup (cronjob)
   → Удаление expired токенов (expires_at < NOW - 7 дней)
```

---

## Индексы

### Коллекция: links

```javascript
// Уникальный индекс по slug (для быстрого поиска по алиасу)
db.links.createIndex({ slug: 1 }, { unique: true });

// Опциональный индекс по state (для фильтрации активных ссылок)
db.links.createIndex({ state: 1 });

// Составной индекс для поиска активных ссылок по slug
db.links.createIndex({ slug: 1, state: 1 });
```

**Производительность:**

| Операция | Индекс | Время | Записей/сек |
|----------|--------|-------|-------------|
| FindById (slug) | { slug: 1 } | 1-2ms | ~500-1000 |
| Find active links | { state: 1 } | 5-10ms | ~100-200 |
| Find by slug + state | { slug: 1, state: 1 } | 1-2ms | ~500-1000 |

---

### Коллекция: users

```javascript
// Уникальный индекс по username
db.users.createIndex({ username: 1 }, { unique: true });

// Опциональный индекс по email (если используется email login)
db.users.createIndex({ email: 1 }, { unique: true, sparse: true });

// Индекс по active (для фильтрации активных пользователей)
db.users.createIndex({ active: 1 });
```

---

### Коллекция: refresh_tokens

```javascript
// Уникальный индекс по jti (JWT Token ID)
db.refresh_tokens.createIndex({ jti: 1 }, { unique: true });

// Индекс по user_id (для поиска всех токенов пользователя)
db.refresh_tokens.createIndex({ user_id: 1 });

// Составной индекс для проверки валидности токена
db.refresh_tokens.createIndex({ jti: 1, revoked: 1 });

// TTL индекс для автоматического удаления истекших токенов
db.refresh_tokens.createIndex(
  { expires_at: 1 },
  { expireAfterSeconds: 604800 }  // 7 дней после expires_at
);
```

**TTL Index:**

MongoDB автоматически удаляет документы, где `expires_at + 7 дней < NOW`.

---

## Связи между коллекциями

### ER Diagram

```
┌─────────────────┐
│     users       │
│─────────────────│
│ _id (PK)        │
│ username        │
│ password_hash   │
│ email           │
│ active          │
│ created_at      │
└────────┬────────┘
         │
         │ 1:N
         │
         v
┌─────────────────┐
│ refresh_tokens  │
│─────────────────│
│ _id (PK)        │
│ jti (UK)        │
│ user_id (FK) ───┘
│ username        │
│ token           │
│ expires_at      │
│ created_at      │
│ revoked         │
└─────────────────┘


┌─────────────────┐
│     links       │
│─────────────────│
│ _id (PK)        │
│ slug (UK)       │
│ state           │
│ rules / rules_dsl│
│ created_at      │
│ updated_at      │
└─────────────────┘
  (Независимая коллекция)
```

### Связи

| Родительская коллекция | Дочерняя коллекция | Тип связи | Foreign Key | Описание |
|------------------------|-------------------|-----------|-------------|----------|
| `users` | `refresh_tokens` | 1:N | `refresh_tokens.user_id` → `users._id` | Один пользователь может иметь несколько refresh токенов |

**Примечание:** Коллекция `links` не имеет внешних ключей и является независимой.

---

## Миграции и версионирование

### Текущая версия схемы

**Version:** 1.0.0

### История изменений

| Версия | Дата | Изменения |
|--------|------|-----------|
| 1.0.0 | 2026-03-16 | Initial schema: links, users, refresh_tokens |

### Миграция с JSON на DSL режим

**Стратегия:** Два режима работают параллельно, используя разные databases.

**Переход:**

1. **Создать новую ссылку в LinksDSL**:
   ```javascript
   // Из Links (JSON)
   {
     slug: "/my-link",
     state: "active",
     rules: [
       { language: "ru-RU", redirect: "https://example.ru", start: ..., end: ... },
       { language: "en-US", redirect: "https://example.com", start: ..., end: ... }
     ]
   }

   // В LinksDSL (DSL)
   {
     slug: "/my-link",
     state: "active",
     rules_dsl: "LANGUAGE = ru-RU -> https://example.ru; LANGUAGE = en-US -> https://example.com"
   }
   ```

2. **Конвертация правил**:
   - JSON `rules[].language` → DSL `LANGUAGE = <value>`
   - JSON `rules[].start/end` → DSL `DATETIME IN[start, end]`
   - JSON `rules[].redirect` → DSL `-> <url>`

3. **Переключение режима**:
   ```bash
   # JSON mode (default в Links DB)
   USE_DSL_RULES=false ./smartlinks

   # DSL mode (default в LinksDSL DB)
   USE_DSL_RULES=true ./smartlinks
   ```

---

## Валидация данных

### MongoDB Schema Validation

**Пример для коллекции links (DSL режим):**

```javascript
db.createCollection("links", {
  validator: {
    $jsonSchema: {
      bsonType: "object",
      required: ["slug", "state", "rules_dsl"],
      properties: {
        slug: {
          bsonType: "string",
          pattern: "^/[a-zA-Z0-9_-]+$",
          description: "slug must be a string starting with / and is required"
        },
        state: {
          enum: ["active", "deleted", "freezed"],
          description: "state must be one of: active, deleted, freezed"
        },
        rules_dsl: {
          bsonType: "string",
          description: "rules_dsl must be a valid DSL string"
        },
        created_at: {
          bsonType: "date"
        },
        updated_at: {
          bsonType: "date"
        }
      }
    }
  }
});
```

**Применение валидации:**

```javascript
// Для существующей коллекции
db.runCommand({
  collMod: "links",
  validator: { /* ... */ },
  validationLevel: "moderate",  // moderate или strict
  validationAction: "warn"      // warn или error
});
```

---

## Backup и Recovery

### Backup Strategy

**Полный backup (ежедневно):**

```bash
# Backup всех databases
mongodump --uri="mongodb://root:password@localhost:27017" --out=/backup/$(date +%Y%m%d)

# Backup конкретной database
mongodump --uri="mongodb://root:password@localhost:27017" --db=Links --out=/backup/Links_$(date +%Y%m%d)
```

**Incremental backup (каждый час):**

```bash
# Oplog replay для point-in-time recovery
mongodump --uri="mongodb://root:password@localhost:27017" --oplog --out=/backup/oplog_$(date +%Y%m%d_%H%M)
```

### Recovery

**Восстановление полного backup:**

```bash
mongorestore --uri="mongodb://root:password@localhost:27017" /backup/20260316
```

**Point-in-time recovery:**

```bash
# 1. Восстановить базовый backup
mongorestore --uri="mongodb://root:password@localhost:27017" /backup/20260316

# 2. Применить oplog
mongorestore --uri="mongodb://root:password@localhost:27017" --oplogReplay /backup/oplog_20260316_1200
```

---

## Производительность

### Sizing Guidelines

| Коллекция | Документов | Размер документа | Общий размер | Индексы |
|-----------|-----------|------------------|--------------|---------|
| links | 10,000 | 2 KB | 20 MB | 5 MB |
| users | 1,000 | 0.5 KB | 500 KB | 100 KB |
| refresh_tokens | 5,000 | 0.5 KB | 2.5 MB | 500 KB |
| **ИТОГО** | **16,000** | - | **~23 MB** | **~6 MB** |

**Рекомендации:**

- **RAM для working set:** 64-128 MB минимум
- **Disk space:** 1 GB минимум (с учетом роста)
- **Connection pool:** 10-50 соединений

---

## Безопасность

### Access Control

**MongoDB Users:**

```javascript
// Admin user (полный доступ)
db.createUser({
  user: "admin",
  pwd: "securePassword123",
  roles: [{ role: "root", db: "admin" }]
});

// Application user (read/write на Links)
db.createUser({
  user: "smartlinks_app",
  pwd: "appPassword456",
  roles: [
    { role: "readWrite", db: "Links" },
    { role: "readWrite", db: "LinksDSL" }
  ]
});

// Read-only user (для мониторинга)
db.createUser({
  user: "monitoring",
  pwd: "monitorPassword789",
  roles: [
    { role: "read", db: "Links" },
    { role: "read", db: "LinksDSL" }
  ]
});
```

### Encryption

**В транспорте:**
- TLS/SSL для MongoDB connections
- Сертификаты для production

**В покое (at-rest):**
- MongoDB Enterprise: encrypted storage engine
- Filesystem encryption (LUKS, dm-crypt)

**Sensitive data:**
- `password_hash` - bcrypt (необратимый)
- `token` - JWT (подписанный, не зашифрованный)

---

## Мониторинг

### Key Metrics

**Database metrics:**

```javascript
// Database stats
db.stats()

// Collection stats
db.links.stats()
db.users.stats()
db.refresh_tokens.stats()

// Index stats
db.links.aggregate([{ $indexStats: {} }])
```

**Performance metrics:**

- Query execution time (p50, p95, p99)
- Index hit rate (>95% желательно)
- Working set size (<= RAM)
- Connection pool usage
- Oplog window (>24h для replica set)

---

## Troubleshooting

### Common Issues

**1. Slow queries на links:**

```javascript
// Проверить explain plan
db.links.find({ slug: "/my-link" }).explain("executionStats")

// Проверить, используется ли индекс
// executionStats.executionStages.indexName должен быть "slug_1"
```

**2. Дубликаты slug:**

```javascript
// Найти дубликаты
db.links.aggregate([
  { $group: { _id: "$slug", count: { $sum: 1 } } },
  { $match: { count: { $gt: 1 } } }
])

// Удалить дубликаты (оставить первый)
db.links.aggregate([
  { $group: { _id: "$slug", ids: { $push: "$_id" } } },
  { $match: { "ids.1": { $exists: true } } }
]).forEach(doc => {
  db.links.deleteMany({ _id: { $in: doc.ids.slice(1) } })
})
```

**3. Истекшие refresh токены не удаляются:**

```javascript
// Проверить TTL индекс
db.refresh_tokens.getIndexes()

// Вручную удалить истекшие токены
db.refresh_tokens.deleteMany({
  expires_at: { $lt: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000) }
})
```

---

## Связанная документация

- [ARCHITECTURE.md](../ARCHITECTURE.md) - Архитектура проекта
- [DSL_GRAMMAR.md](./DSL_GRAMMAR.md) - Грамматика DSL
- [JWT.md](./JWT.md) - JWT Service документация
- [BUSINESS_PROCESSES.md](./BUSINESS_PROCESSES.md) - Бизнес-процессы

---

**Автор:** SmartLinks Development Team
**Последнее обновление:** 2026-03-16
**Версия:** 1.0.0
