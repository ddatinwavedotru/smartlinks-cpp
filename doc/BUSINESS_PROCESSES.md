# SmartLinks: Бизнес-процессы

Документ описывает ключевые бизнес-процессы микросервиса SmartLinks с точки зрения пользовательских сценариев и технической реализации.

**Версия:** 1.0.0
**Дата:** 2026-03-16
**Статус:** Production

---

## 📋 Содержание

1. [Обзор бизнес-процессов](#обзор-бизнес-процессов)
2. [Процесс 1: Редирект по умной ссылке](#процесс-1-редирект-по-умной-ссылке)
3. [Процесс 2: JWT аутентификация](#процесс-2-jwt-аутентификация)
4. [Процесс 3: Создание SmartLink](#процесс-3-создание-smartlink)
5. [Процесс 4: Управление правилами DSL](#процесс-4-управление-правилами-dsl)
6. [Процесс 5: Freeze/Unfreeze ссылки](#процесс-5-freezeunfreeze-ссылки)
7. [Обработка ошибок](#обработка-ошибок)

---

## Обзор бизнес-процессов

### Участники системы

| Участник | Описание | Роль |
|----------|----------|------|
| **Конечный пользователь** | Человек, переходящий по SmartLink | Инициирует редиректы |
| **Контент-менеджер** | Сотрудник, управляющий ссылками | Создает и настраивает SmartLinks |
| **Администратор** | Технический специалист | Управляет системой, JWT токенами |
| **API Gateway (Nginx)** | Reverse proxy | Маршрутизация запросов |
| **SmartLinks Service** | Микросервис C++ | Обработка редиректов |
| **JWT Service** | Микросервис Node.js | Выдача JWT токенов |
| **MongoDB** | База данных | Хранение ссылок и правил |

---

### Карта бизнес-процессов

```
┌──────────────────────────────────────────────────────────────┐
│                    SmartLinks Platform                        │
├──────────────────────────────────────────────────────────────┤
│                                                                │
│  [1] Редирект по умной ссылке ◄─── 🔥 ОСНОВНОЙ ПРОЦЕСС       │
│       │                                                        │
│       ├─► Middleware Pipeline (405→404→422→307)               │
│       ├─► DSL Evaluation                                      │
│       └─► JWT Authorization (опционально)                     │
│                                                                │
│  [2] JWT аутентификация                                       │
│       │                                                        │
│       ├─► Login (username/password → tokens)                  │
│       ├─► Refresh (refresh_token → new access_token)          │
│       └─► Introspect (token → validation)                     │
│                                                                │
│  [3] Создание SmartLink                                       │
│       │                                                        │
│       ├─► MongoDB Insert                                      │
│       └─► DSL Rules Setup                                     │
│                                                                │
│  [4] Управление правилами DSL                                 │
│       │                                                        │
│       ├─► Add/Update/Delete Rules                             │
│       └─► DSL Validation                                      │
│                                                                │
│  [5] Freeze/Unfreeze ссылки                                   │
│       │                                                        │
│       └─► State Management                                    │
│                                                                │
└──────────────────────────────────────────────────────────────┘
```

---

## Процесс 1: Редирект по умной ссылке

### 🎯 Описание

**Цель:** Перенаправить пользователя на целевой URL в зависимости от контекста запроса (язык, время, авторизация).

**Триггер:** HTTP GET/HEAD запрос к `http://gateway/<link_alias>`

**Результат:**
- **Успех:** HTTP 307 Temporary Redirect на целевой URL
- **Ошибка:** HTTP 404/405/422/429

---

### 📊 Пошаговый процесс

#### Шаг 1: Получение HTTP запроса

**Актор:** Конечный пользователь

**Действие:**
```http
GET /my-link HTTP/1.1
Host: gateway
Accept-Language: en-US,en;q=0.9
Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...
```

**Результат:** Запрос попадает в API Gateway (Nginx)

---

#### Шаг 2: Маршрутизация через API Gateway

**Актор:** API Gateway (Nginx)

**Действие:**
1. Проверить маршрут запроса
2. Определить целевой микросервис
3. Проксировать запрос

**Routing:**
```nginx
location / {
    proxy_pass http://smartlinks:8080;
    proxy_http_version 1.1;
    proxy_set_header Host $host;
    proxy_set_header Authorization $http_authorization;
    proxy_set_header Accept-Language $http_accept_language;
}
```

**Результат:** Запрос передан в SmartLinks Service (port 8080)

---

#### Шаг 3: Создание Request Scope (DI Container)

**Актор:** DrogonMiddlewareAdapter

**Действие:**
1. Создать request-scoped IoC контейнер
2. Установить как текущий (thread_local)
3. Зарегистрировать HTTP-специфичные зависимости:
   - `IHttpRequest` → Drogon Request
   - `IHttpResponse` → Drogon Response
   - `IContext` → DSL Context с request data

**Код:**
```cpp
// 1. Create request scope
auto request_scope = ioc::IoC::Resolve<std::shared_ptr<ioc::IScopeDict>>(
    "IoC.Scope.Create"
);

// 2. Set as current (thread-local)
ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
    "IoC.Scope.Current.Set",
    ioc::Args{request_scope}
)->Execute();

// 3. Register HTTP dependencies
RegisterHttpDependencies(req);
```

**Результат:** Request-scoped контейнер готов для резолва зависимостей

---

#### Шаг 4: Middleware Pipeline - Валидация запроса

**Актор:** Middleware Pipeline (Chain of Responsibility)

**Последовательность:**

##### 4.1. MethodNotAllowedMiddleware (HTTP 405)

**Проверка:** Метод запроса = GET или HEAD?

```cpp
if (method != "GET" && method != "HEAD") {
    response->setStatusCode(k405MethodNotAllowed);
    response->addHeader("Allow", "GET, HEAD");
    return;  // ❌ Остановить pipeline
}
```

**Результат:**
- ✅ GET/HEAD → продолжить
- ❌ POST/PUT/DELETE → 405 Method Not Allowed

---

##### 4.2. NotFoundMiddleware (HTTP 404)

**Проверка:** Ссылка существует и не удалена?

```cpp
auto link_opt = repository->FindById(link_alias);

if (!link_opt || link_opt->IsDeleted()) {
    response->setStatusCode(k404NotFound);
    return;  // ❌ Остановить pipeline
}
```

**MongoDB Query:**
```javascript
db.links.findOne({
    _id: "my-link",
    is_deleted: { $ne: true }
})
```

**Результат:**
- ✅ Ссылка найдена → продолжить
- ❌ Не найдена или удалена → 404 Not Found

---

##### 4.3. UnprocessableContentMiddleware (HTTP 422)

**Проверка:** Ссылка не заморожена?

```cpp
auto link_opt = repository->FindById(link_alias);

if (link_opt->IsFrozen()) {
    response->setStatusCode(k422UnprocessableEntity);
    response->setBody("Link is frozen");
    return;  // ❌ Остановить pipeline
}
```

**Результат:**
- ✅ Активная ссылка → продолжить
- ❌ Заморожена → 422 Unprocessable Entity

---

##### 4.4. TemporaryRedirectMiddleware (HTTP 307 или 429)

**Действие:** Вычислить redirect URL через DSL правила

**Процесс:**

1. **Загрузить DSL правила из MongoDB:**

```cpp
auto rules_opt = repository->Read(context);
// MongoDB: db.links.findOne({ _id: "my-link" }).rules_dsl
```

**Пример DSL:**
```dsl
LANGUAGE = en-US -> https://example.com;
LANGUAGE = ru-RU -> https://example.ru;
AUTHORIZED -> https://example.com/premium
```

2. **Распарсить DSL в AST:**

```cpp
auto parse_result = parser->Parse(dsl_text, context);
// Создается дерево AST узлов:
//   OrExpression
//     ├─ LanguageEqualExpression("en-US")
//     ├─ LanguageEqualExpression("ru-RU")
//     └─ AuthorizedExpression()
```

3. **Заполнить Context данными запроса:**

```cpp
context->SetRequest(request);
context->SetCurrentTime(std::time(nullptr));
context->SetAcceptLanguage(request->getHeader("Accept-Language"));
context->SetJwtValidator(jwt_validator);
```

4. **Вычислить правила (AST Evaluation):**

```cpp
auto redirect_url = rules_set.Evaluate(context);
```

**Алгоритм:**
```
FOR each rule IN rules_set:
    result = Evaluate(rule.condition, context)
    IF result == true:
        RETURN rule.redirect_url
RETURN null  // No match
```

**Пример вычисления:**
```
Rule 1: LANGUAGE = en-US
  Context.AcceptLanguage = "en-US,en;q=0.9"
  Evaluate: "en-US" in "en-US,en;q=0.9" → TRUE ✅
  → RETURN "https://example.com"
```

5. **Установить HTTP 307 Redirect:**

```cpp
if (redirect_url) {
    response->setStatusCode(k307TemporaryRedirect);
    response->addHeader("Location", *redirect_url);
} else {
    response->setStatusCode(k429TooManyRequests);
    response->setBody("No matching rule found");
}
```

**Результат:**
- ✅ Правило совпало → 307 Temporary Redirect
- ❌ Ни одно правило не совпало → 429 Too Many Requests

---

#### Шаг 5: Отправка ответа клиенту

**Актор:** API Gateway → Конечный пользователь

**Успешный ответ (307):**
```http
HTTP/1.1 307 Temporary Redirect
Location: https://example.com
Date: Sun, 16 Mar 2026 12:00:00 GMT
Server: nginx/1.21.6
```

**Браузер автоматически перенаправляется на `https://example.com`**

---

### 🔄 Диаграмма последовательности (Sequence Diagram)

```
User          Gateway       SmartLinks      MongoDB      JWT Service
 │               │              │              │              │
 ├─GET /my-link─►│              │              │              │
 │               │              │              │              │
 │               ├─proxy─────►  │              │              │
 │               │              │              │              │
 │               │              ├─1. Create    │              │
 │               │              │   Scope      │              │
 │               │              │              │              │
 │               │              ├─2. M405──────┤              │
 │               │              │   (validate) │              │
 │               │              │              │              │
 │               │              ├─3. M404──────┼─FindById──►  │
 │               │              │              ◄────link─────┤
 │               │              │              │              │
 │               │              ├─4. M422──────┤              │
 │               │              │   (frozen?)  │              │
 │               │              │              │              │
 │               │              ├─5. M307──────┼─Read rules──►│
 │               │              │              ◄────DSL──────┤
 │               │              │              │              │
 │               │              ├─6. Parse DSL │              │
 │               │              │   (AST)      │              │
 │               │              │              │              │
 │               │              ├─7. JWT?──────┼──────────────┼─JWKS─►│
 │               │              │              │              ◄───────┤
 │               │              │              │              │
 │               │              ├─8. Evaluate  │              │
 │               │              │   AST        │              │
 │               │              │              │              │
 │               │              ├─9. 307──────►│              │
 │               │              │   Location   │              │
 │               │              │              │              │
 │               ◄───307────────┤              │              │
 │               │   Location   │              │              │
 │               │              │              │              │
 ◄────307────────┤              │              │              │
 │   Location    │              │              │              │
 │               │              │              │              │
 ├─GET Location──►              │              │              │
 │   (redirect)  │              │              │              │
```

---

### ⏱️ Временные характеристики

| Этап | Время | Критичность |
|------|-------|-------------|
| Gateway routing | ~0.1-0.5ms | Низкая |
| Создание Request Scope | ~0.05-0.1ms | Низкая |
| Middleware 405 | ~0.01-0.02ms | Низкая |
| Middleware 404 (MongoDB) | **1-5ms** | 🔴 Высокая |
| Middleware 422 | ~0.01-0.02ms | Низкая |
| DSL парсинг | **0.5-2ms** | 🟡 Средняя |
| JWKS fetch (если нужен) | **0.5-2ms** | 🟡 Средняя |
| AST evaluation | ~0.1-0.5ms | Низкая |
| Response отправка | ~0.1-0.5ms | Низкая |
| **ИТОГО** | **2-10ms** | - |

**Критические узкие места:**
- MongoDB FindById (1-5ms) - можно кэшировать
- DSL парсинг (0.5-2ms) - можно кэшировать AST

---

### 📈 Бизнес-метрики

| Метрика | Значение | Описание |
|---------|----------|----------|
| **Throughput** | 1000-2000 RPS | Запросов в секунду (на 1 core) |
| **Latency P50** | 2-5ms | Медианная задержка |
| **Latency P99** | 10-20ms | 99-й перцентиль |
| **Success Rate** | >99% | Доля успешных редиректов (307) |
| **Cache Hit Rate** | 80-95% | (при включенном кэшировании) |

---

## Процесс 2: JWT аутентификация

### 🎯 Описание

**Цель:** Получить JWT токены для доступа к защищенному контенту через SmartLinks.

**Триггер:** Пользователь хочет получить доступ к premium контенту

**Результат:** Access token (10 мин) + Refresh token (14 дней)

---

### 📊 Подпроцесс 2.1: Login (Вход в систему)

**Шаг 1: Отправка credentials**

```http
POST /jwt/auth/login HTTP/1.1
Host: gateway
Content-Type: application/json

{
  "username": "john",
  "password": "secret123"
}
```

**Шаг 2: Gateway маршрутизация**

```nginx
location /jwt/ {
    rewrite ^/jwt/(.*) /$1 break;  # Удалить /jwt prefix
    proxy_pass http://jwt-service:3000;
}
```

**Результат:** Запрос передан в JWT Service как `POST /auth/login`

**Шаг 3: Валидация credentials**

```javascript
// JWT Service (Node.js)
const user = await db.collection('users').findOne({ username });

if (!user) {
    return res.status(401).json({ error: 'Invalid credentials' });
}

const passwordValid = await bcrypt.compare(password, user.password_hash);

if (!passwordValid) {
    return res.status(401).json({ error: 'Invalid credentials' });
}
```

**Шаг 4: Генерация JWT токенов**

```javascript
// Access Token (RS256, 10 мин)
const accessToken = jwt.sign(
    {
        sub: user._id,
        username: user.username,
        roles: user.roles,
        iss: 'smartlinks-jwt-service',
        aud: 'smartlinks-api'
    },
    PRIVATE_KEY,
    {
        algorithm: 'RS256',
        expiresIn: '10m',
        keyid: getKeyId()
    }
);

// Refresh Token (RS256, 14 дней)
const refreshToken = jwt.sign(
    {
        sub: user._id,
        type: 'refresh',
        iss: 'smartlinks-jwt-service'
    },
    PRIVATE_KEY,
    {
        algorithm: 'RS256',
        expiresIn: '14d',
        keyid: getKeyId()
    }
);

// Сохранить refresh token в MongoDB
await db.collection('refresh_tokens').insertOne({
    user_id: user._id,
    token_hash: crypto.createHash('sha256').update(refreshToken).digest('hex'),
    expires_at: new Date(Date.now() + 14 * 24 * 60 * 60 * 1000),
    revoked: false
});
```

**Шаг 5: Возврат токенов клиенту**

```http
HTTP/1.1 200 OK
Content-Type: application/json

{
  "access_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImFiY2QxMjM0In0...",
  "refresh_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6ImFiY2QxMjM0In0...",
  "expires_in": 600
}
```

**Результат:** Пользователь получил токены для авторизации

---

### 📊 Подпроцесс 2.2: Использование Access Token для SmartLink

**Шаг 1: Запрос с Authorization header**

```http
GET /premium-content HTTP/1.1
Host: gateway
Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...
```

**Шаг 2: SmartLinks валидирует JWT**

```cpp
// В DSL Expression: AUTHORIZED
bool AuthorizedExpression::Evaluate(const IContext& ctx) const {
    // 1. Получить JWT из Authorization header
    std::string auth_header = ctx.GetRequest()->getHeader("Authorization");
    std::string token = ExtractBearerToken(auth_header);

    // 2. Получить JWKS public keys
    auto jwks = FetchJwks("http://jwt-service:3000/.well-known/jwks.json");

    // 3. Валидировать JWT (jwt-cpp библиотека)
    auto decoded = jwt::decode(token);
    auto verifier = jwt::verify()
        .allow_algorithm(jwt::algorithm::rs256(jwks.public_key))
        .with_issuer("smartlinks-jwt-service")
        .with_audience("smartlinks-api");

    try {
        verifier.verify(decoded);
        return true;  // ✅ Valid JWT
    } catch (const std::exception& e) {
        return false;  // ❌ Invalid JWT
    }
}
```

**Шаг 3: DSL правило с AUTHORIZED**

```dsl
AUTHORIZED -> https://example.com/premium/exclusive-content;
LANGUAGE = * -> https://example.com/signup
```

**Вычисление:**
```
Rule 1: AUTHORIZED
  Evaluate: JWT valid? → TRUE ✅
  → RETURN "https://example.com/premium/exclusive-content"
```

**Результат:**
- ✅ Valid JWT → 307 на premium контент
- ❌ Invalid/Missing JWT → 307 на signup страницу

---

### 📊 Подпроцесс 2.3: Refresh Token

**Цель:** Обновить access token без повторного ввода password

```http
POST /jwt/auth/refresh HTTP/1.1
Host: gateway
Content-Type: application/json

{
  "refresh_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

**Результат:**
```http
HTTP/1.1 200 OK

{
  "access_token": "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9...",  // NEW
  "expires_in": 600
}
```

---

## Процесс 3: Создание SmartLink

### 🎯 Описание

**Цель:** Создать новую умную ссылку с DSL правилами.

**Актор:** Контент-менеджер (через Admin Panel или API)

---

### 📊 Пошаговый процесс

**Шаг 1: Подготовка данных**

```json
{
  "alias": "summer-sale",
  "rules_dsl": [
    {
      "condition": "DATETIME IN[2026-06-01T00:00:00Z, 2026-08-31T23:59:59Z] AND LANGUAGE = en-US",
      "redirect_url": "https://example.com/summer-sale-us"
    },
    {
      "condition": "DATETIME IN[2026-06-01T00:00:00Z, 2026-08-31T23:59:59Z] AND LANGUAGE = ru-RU",
      "redirect_url": "https://example.ru/letnaya-rasprodazha"
    },
    {
      "condition": "LANGUAGE = en-US",
      "redirect_url": "https://example.com"
    },
    {
      "condition": "LANGUAGE = ru-RU",
      "redirect_url": "https://example.ru"
    }
  ],
  "created_at": "2026-03-16T12:00:00Z",
  "is_deleted": false,
  "is_frozen": false
}
```

**Шаг 2: Валидация DSL (опционально)**

```cpp
// Валидировать каждое правило
for (const auto& rule : rules_dsl) {
    auto parse_result = parser->Parse(rule.condition, test_context);

    if (!parse_result.success) {
        throw ValidationError("Invalid DSL: " + parse_result.error_message);
    }
}
```

**Шаг 3: Вставка в MongoDB**

```javascript
db.links.insertOne({
    _id: "summer-sale",
    rules_dsl: [ /* ... */ ],
    created_at: ISODate("2026-03-16T12:00:00Z"),
    is_deleted: false,
    is_frozen: false
})
```

**Результат:** SmartLink создан и готов к использованию

---

## Процесс 4: Управление правилами DSL

### 🎯 Описание

**Цель:** Обновить DSL правила существующей SmartLink.

---

### 📊 Пошаговый процесс

**Шаг 1: Добавление нового правила**

```javascript
db.links.updateOne(
    { _id: "summer-sale" },
    {
        $push: {
            rules_dsl: {
                condition: "AUTHORIZED",
                redirect_url: "https://example.com/vip-summer-sale"
            }
        }
    }
)
```

**Результат:** Новое правило добавлено в начало списка (приоритет)

**Шаг 2: Обновление существующего правила**

```javascript
db.links.updateOne(
    { _id: "summer-sale", "rules_dsl.condition": "LANGUAGE = en-US" },
    {
        $set: {
            "rules_dsl.$.redirect_url": "https://example.com/new-url"
        }
    }
)
```

**Шаг 3: Удаление правила**

```javascript
db.links.updateOne(
    { _id: "summer-sale" },
    {
        $pull: {
            rules_dsl: { condition: "LANGUAGE = en-US" }
        }
    }
)
```

**Результат:** Правило удалено из списка

---

## Процесс 5: Freeze/Unfreeze ссылки

### 🎯 Описание

**Цель:** Временно отключить SmartLink без удаления.

**Use case:** Техническое обслуживание, инцидент, плановое отключение

---

### 📊 Пошаговый процесс

**Freeze (заморозить):**

```javascript
db.links.updateOne(
    { _id: "summer-sale" },
    { $set: { is_frozen: true } }
)
```

**Результат:** Все запросы к `summer-sale` возвращают **HTTP 422 Unprocessable Entity**

**Unfreeze (разморозить):**

```javascript
db.links.updateOne(
    { _id: "summer-sale" },
    { $set: { is_frozen: false } }
)
```

**Результат:** Ссылка снова работает нормально

---

## Обработка ошибок

### Матрица HTTP статусов

| Код | Статус | Причина | Решение пользователя |
|-----|--------|---------|---------------------|
| **307** | Temporary Redirect | Успешный редирект | Браузер автоматически переходит |
| **404** | Not Found | Ссылка не существует или удалена | Проверить URL |
| **405** | Method Not Allowed | Использован POST/PUT/DELETE | Использовать GET/HEAD |
| **422** | Unprocessable Entity | Ссылка заморожена (is_frozen=true) | Подождать разморозки |
| **429** | Too Many Requests | Ни одно DSL правило не совпало | Проверить Accept-Language/Authorization |
| **500** | Internal Server Error | Исключение в middleware/service | Обратиться в поддержку |

---

### Типичные ошибки и их причины

#### Ошибка: 404 Not Found

**Причины:**
1. Ссылка еще не создана в MongoDB
2. Ссылка удалена (`is_deleted: true`)
3. Опечатка в link_alias

**Диагностика:**
```javascript
db.links.findOne({ _id: "my-link" })
// null → не создана
// { is_deleted: true } → удалена
```

---

#### Ошибка: 429 Too Many Requests

**Причины:**
1. DSL правила не покрывают текущий контекст
2. JWT токен невалиден для `AUTHORIZED` правила
3. Accept-Language не совпадает ни с одним `LANGUAGE` правилом

**Пример:**
```dsl
LANGUAGE = en-US -> https://example.com;
LANGUAGE = ru-RU -> https://example.ru
```

**Запрос:**
```http
GET /my-link
Accept-Language: de-DE
```

**Результат:** 429 (нет правила для `de-DE`)

**Решение:** Добавить fallback правило:
```dsl
LANGUAGE = en-US -> https://example.com;
LANGUAGE = ru-RU -> https://example.ru;
LANGUAGE = * -> https://example.com  // Fallback
```

---

#### Ошибка: JWT validation failed (в логах)

**Причины:**
1. Токен истек (expired)
2. Неверная подпись (signature mismatch)
3. JWKS endpoint недоступен

**Диагностика:**
```bash
# Проверить JWKS endpoint
curl http://jwt-service:3000/.well-known/jwks.json

# Декодировать JWT
echo "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9..." | base64 -d | jq .

# Проверить exp (expiration time)
```

**Решение:** Обновить access token через refresh token

---

## 🎯 Ключевые бизнес-правила

### Правило 1: Порядок вычисления DSL правил

DSL правила вычисляются **последовательно** (first match wins):

```dsl
AUTHORIZED -> https://example.com/premium;         # Проверяется ПЕРВЫМ
LANGUAGE = en-US -> https://example.com;           # Проверяется ВТОРЫМ
LANGUAGE = * -> https://example.com/default        # Проверяется ПОСЛЕДНИМ
```

**ВАЖНО:** Более специфичные правила должны быть в начале списка!

---

### Правило 2: Приоритет операторов в DSL

```
Приоритет (высокий → низкий):
1. Скобки: ( )
2. Primary expressions: LANGUAGE, DATETIME, AUTHORIZED
3. AND
4. OR
```

**Пример:**
```dsl
LANGUAGE = en-US AND AUTHORIZED OR LANGUAGE = ru-RU
```

**Интерпретация:**
```
(LANGUAGE = en-US AND AUTHORIZED) OR LANGUAGE = ru-RU
```

---

### Правило 3: Short-circuit evaluation

AND и OR используют **short-circuit evaluation**:

```cpp
// AND: если left = false, right не вычисляется
bool AndExpression::Evaluate(const IContext& ctx) const {
    if (!left_->Evaluate(ctx)) return false;  // Short-circuit!
    return right_->Evaluate(ctx);
}

// OR: если left = true, right не вычисляется
bool OrExpression::Evaluate(const IContext& ctx) const {
    if (left_->Evaluate(ctx)) return true;  // Short-circuit!
    return right_->Evaluate(ctx);
}
```

**Оптимизация:** Дорогие операции (AUTHORIZED с JWT) лучше ставить справа от AND:

```dsl
LANGUAGE = en-US AND AUTHORIZED  # ✅ Хорошо: JWT проверяется только если язык совпал
AUTHORIZED AND LANGUAGE = en-US  # ❌ Плохо: JWT проверяется всегда
```

---

## 📊 Бизнес-метрики и KPI

### Производительность

| Метрика | Текущее | Целевое | Критичность |
|---------|---------|---------|-------------|
| Throughput | 1000-2000 RPS | 10000+ RPS | 🔴 Высокая |
| Latency P50 | 2-5ms | <1ms | 🟡 Средняя |
| Latency P99 | 10-20ms | <5ms | 🟡 Средняя |
| Error Rate | <1% | <0.1% | 🔴 Высокая |

---

### Доступность

| Метрика | Значение | SLA |
|---------|----------|-----|
| Uptime | 99.9% | 3 nine's |
| MTTR (Mean Time To Recovery) | <5 мин | <10 мин |
| MTBF (Mean Time Between Failures) | >30 дней | >7 дней |

---

### Бизнес-метрики

| Метрика | Описание |
|---------|----------|
| Total SmartLinks | Общее количество активных ссылок |
| Total Redirects | Общее количество редиректов за период |
| Redirects by Link | Топ-10 самых популярных ссылок |
| Success Rate by Rule | Какие DSL правила срабатывают чаще |
| Geographic Distribution | Распределение по языкам (LANGUAGE) |
| Premium Access Rate | Доля редиректов с AUTHORIZED |

---

## 🔗 Связанная документация

- [ARCHITECTURE.md](../ARCHITECTURE.md) - Техническая архитектура
- [DSL_GRAMMAR.md](./DSL_GRAMMAR.md) - Формальная грамматика DSL
- [INFORMATION_SCHEMA.md](./INFORMATION_SCHEMA.md) - Схема базы данных MongoDB
- [INTEGRATIONS.md](../INTEGRATIONS.md) - Интеграции сервисов
- [COMPLEXITY.md](./COMPLEXITY.md) - Проблемы масштабируемости
- [JWT.md](./JWT.md) - JWT Service документация

---

## 📈 Диаграммы

Визуальное представление всех бизнес-процессов:

1. **business-process-redirect.mmd** - Редирект по умной ссылке (основной процесс)
2. **business-process-jwt-auth.mmd** - JWT аутентификация (login, use, refresh)
3. **business-process-create-link.mmd** - Создание SmartLink
4. **business-process-manage-rules.mmd** - Управление правилами DSL (add, update, delete)
5. **business-process-freeze.mmd** - Freeze/Unfreeze ссылки

См. также [diagrams/README.md](../diagrams/README.md) для подробного описания всех диаграмм.

---

**Автор:** SmartLinks Development Team
**Последнее обновление:** 2026-03-16
**Версия:** 1.0.0
