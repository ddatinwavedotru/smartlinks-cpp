# SmartLinks C++ - Архитектура

Этот документ описывает архитектурные решения и паттерны проектирования, использованные при портировании SmartLinks с C#/.NET на C++17.

## 🎯 Цель портирования

Создать **идентичную по архитектуре** версию SmartLinks на C++, которая:
- Работает без .NET framework
- Запускается как микросервис в Docker на Linux
- Поддерживает CI/CD через GitHub Actions
- Сохраняет все интерфейсы, паттерны и бизнес-логику C# версии

## 📐 Архитектурные слои

```
┌─────────────────────────────────────────┐
│         HTTP Layer (Drogon)             │
│    DrogonMiddlewareAdapter              │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│         Middleware Pipeline             │
│  405 → 404 → 422 → 307 (→ 429)          │
│  Chain of Responsibility pattern        │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│         Business Logic                  │
│  ┌──────────────────────────────────┐   │
│  │  JSON/Legacy Mode (deprecated)   │   │
│  │  SmartLinkRedirectService        │   │
│  └──────────────────────────────────┘   │
│  ┌──────────────────────────────────┐   │
│  │  DSL Mode (current)              │   │
│  │  DSL Parser → AST → Evaluator    │   │
│  │  Plugin System (Adapters)        │   │
│  └──────────────────────────────────┘   │
│  FreezeSmartLinkService                 │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│         Domain Model                    │
│  10 Interfaces + 11 Implementations     │
│  + DSL Components (Parser, AST, Eval)   │
│  + Plugin Adapters (AUTHORIZED, etc)    │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│         Data Access Layer               │
│  MongoDbRepository (mongocxx)           │
└─────────────────────────────────────────┘
```

## 🧩 Ключевые компоненты

### 1. IoC Container (Custom)

**Источник:** Порт из проекта appserver (C# → C++)

**Архитектура:**
```cpp
// Функциональная стратегия разрешения
using ResolveStrategy = std::function<std::any(const std::string&, const Args&)>;

class IoC {
private:
    static ResolveStrategy strategy_;  // Обновляется через команду
public:
    template<typename T>
    static T Resolve(const std::string& dependency, const Args& args = {});
};
```

**Особенности:**
- **Функциональный подход** - зависимости как `std::function`
- **Иерархические scopes** - поиск вверх по parent chain
- **Thread-local текущий scope** - изоляция между потоками
- **Command pattern** - все операции как `ICommand`
- **Самомодификация** - стратегия обновляется через `IoC.Register`

**Базовые зависимости:**
```
IoC.Scope.Current         → получить текущий scope (или root)
IoC.Scope.Create          → создать новый scope с parent
IoC.Scope.Create.Empty    → создать пустой scope
IoC.Register              → зарегистрировать зависимость
IoC.Scope.Current.Set     → установить thread_local scope
IoC.Scope.Current.Clear   → очистить thread_local scope
```

**Паттерн регистрации:**
```cpp
ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
    "IoC.Register",
    ioc::Args{
        std::string("DependencyName"),
        ioc::DependencyFactory([](const ioc::Args&) -> std::any {
            // КРИТИЧНО: возвращать указатель на ИНТЕРФЕЙС, не реализацию!
            return std::static_pointer_cast<domain::IInterface>(
                std::make_shared<impl::Implementation>(...)
            );
        })
    }
)->Execute();
```

**ВАЖНО - Type Safety:**

Фабрики ДОЛЖНЫ возвращать указатели на интерфейсы через `std::static_pointer_cast`, иначе будет `bad_any_cast`:

```cpp
// ❌ НЕПРАВИЛЬНО - вернёт std::shared_ptr<impl::SmartLink>
return std::make_shared<impl::SmartLink>(req);

// ✅ ПРАВИЛЬНО - вернёт std::shared_ptr<domain::ISmartLink>
return std::static_pointer_cast<domain::ISmartLink>(
    std::make_shared<impl::SmartLink>(req)
);
```

### 2. Middleware Pipeline

**Проблема:** Как реплицировать ASP.NET Core middleware infrastructure в C++?

**Решение:** Адаптер + Chain of Responsibility

#### 2.1 IMiddleware интерфейс

```cpp
using RequestDelegate = std::function<void(
    const drogon::HttpRequestPtr&,
    drogon::HttpResponsePtr&  // Единый Response!
)>;

class IMiddleware {
public:
    virtual void Invoke(
        const drogon::HttpRequestPtr& request,
        drogon::HttpResponsePtr& response,  // По ссылке!
        RequestDelegate next
    ) = 0;
};
```

**Критические особенности:**
- ✅ **Единый Response объект** - создаётся один раз, передаётся по ссылке
- ✅ **DI через конструктор** - зависимости передаются в конструктор
- ✅ **Transient lifetime** - middleware создаются заново на каждый запрос
- ✅ **Chain of Responsibility** - `next()` продолжает цепочку

#### 2.2 DrogonMiddlewareAdapter

Интеграция с Drogon framework:

```cpp
void DrogonMiddlewareAdapter::invoke(...) {
    try {
        // 1. Создать request-scoped IoC контейнер
        auto request_scope = IoC::Resolve<shared_ptr<IScopeDict>>(
            "IoC.Scope.Create"
        );

        // 2. Установить как текущий
        IoC::Resolve<shared_ptr<ICommand>>(
            "IoC.Scope.Current.Set",
            Args{request_scope}
        )->Execute();

        // 3. Зарегистрировать HTTP-зависимости
        RegisterHttpDependencies(req);

        // 4. Создать Response (ОДИН РАЗ!)
        auto response = HttpResponse::newHttpResponse();

        // 5. Получить middleware из IoC
        std::vector<shared_ptr<IMiddleware>> middlewares;
        IoC::Resolve<shared_ptr<ICommand>>(
            "GetMiddlewarePipeline",
            Args{std::ref(middlewares)}
        )->Execute();

        // 6. Получить ExceptionHandler из IoC
        auto exception_handler = IoC::Resolve<function<void(...)>>(
            "ExceptionHandler"
        );

        // 7. Построить Chain of Responsibility
        auto pipeline = BuildPipeline(middlewares);

        // 8. Выполнить с exception handling
        try {
            pipeline(req, response);
        } catch (...) {
            exception_handler(nullptr, std::current_exception());
            response->setStatusCode(500);
        }

        // 9. Вернуть response
        mcb(response);

        // 10. Очистить scope
        IoC::Resolve<shared_ptr<ICommand>>(
            "IoC.Scope.Current.Clear"
        )->Execute();

    } catch (const std::exception& e) {
        LOG_ERROR << "Fatal error: " << e.what();
        auto response = HttpResponse::newHttpResponse();
        response->setStatusCode(500);
        mcb(response);
    }
}
```

**BuildPipeline - рекурсивное построение цепочки:**

```cpp
RequestDelegate BuildPipeline(
    const std::vector<shared_ptr<IMiddleware>>& middlewares
) {
    // Конец цепочки (если никто не установил статус → 429)
    RequestDelegate pipeline = [](
        const HttpRequestPtr& req,
        HttpResponsePtr& resp
    ) {
        if (resp->statusCode() == drogon::k200OK) {
            resp->setStatusCode(static_cast<HttpStatusCode>(429));
        }
    };

    // Оборачиваем каждый middleware (с конца в начало)
    for (auto it = middlewares.rbegin(); it != middlewares.rend(); ++it) {
        auto middleware = *it;
        auto next = pipeline;

        pipeline = [middleware, next](
            const HttpRequestPtr& req,
            HttpResponsePtr& resp
        ) {
            middleware->Invoke(req, resp, next);
        };
    }

    return pipeline;
}
```

#### 2.3 Middleware реализации

**MethodNotAllowedMiddleware (405):**
```cpp
void Invoke(
    const HttpRequestPtr& req,
    HttpResponsePtr& response,
    RequestDelegate next
) override {
    if (!supported_request_->MethodIsSupported()) {
        response->setStatusCode(drogon::k405MethodNotAllowed);
        // НЕ вызываем next - прерываем цепочку
    } else {
        next(req, response);
    }
}
```

**NotFoundMiddleware (404):**
```cpp
void Invoke(
    const HttpRequestPtr& req,
    HttpResponsePtr& response,
    RequestDelegate next
) override {
    auto smart_link = repository_->Read();
    if (!smart_link.has_value() || smart_link->GetState() == "deleted") {
        response->setStatusCode(drogon::k404NotFound);
    } else {
        next(req, response);
    }
}
```

**UnprocessableContentMiddleware (422):**
```cpp
void Invoke(
    const HttpRequestPtr& req,
    HttpResponsePtr& response,
    RequestDelegate next
) override {
    if (freeze_service_->ShouldSmartLinkBeFreezed()) {
        response->setStatusCode(drogon::k422UnprocessableEntity);
    } else {
        next(req, response);
    }
}
```

**TemporaryRedirectMiddleware (307):**
```cpp
void Invoke(
    const HttpRequestPtr& req,
    HttpResponsePtr& response,
    RequestDelegate next
) override {
    auto redirect = redirect_service_->Evaluate();
    if (redirect.has_value()) {
        response->setStatusCode(drogon::k307TemporaryRedirect);
        response->addHeader("Location", redirect.value());
    } else {
        next(req, response);  // → конец цепочки → 429
    }
}
```

### 3. Доменная модель

**10 базовых интерфейсов (Legacy/JSON mode):**

| Интерфейс | Описание |
|-----------|----------|
| ISmartLink | Доступ к slug из URL |
| IRedirectRule | Правило редиректа (язык, URL, время) |
| IRedirectableSmartLink | Проверка условий (язык + время) |
| IStatableSmartLink | Состояние SmartLink |
| IMongoDbRepository | Низкоуровневый доступ к MongoDB |
| ISmartLinkRedirectService | Бизнес-логика редиректа |
| ISmartLinkRedirectRulesRepository | Чтение правил из MongoDB |
| IStatableSmartLinkRepository | Чтение состояния из MongoDB |
| IFreezeSmartLinkService | Проверка заморозки |
| ISupportedHttpRequest | Проверка HTTP метода |

**DSL компоненты (новая архитектура):**

| Компонент | Описание |
|-----------|----------|
| **DSL Parser** | Парсинг текстовых DSL правил в AST |
| **AST Nodes** | Abstract Syntax Tree узлы (IF, AND, OR, RULE) |
| **Evaluator** | Вычисление AST с использованием Context |
| **IContext** | Хранилище данных для DSL выражений |
| **IAdapter** | Интерфейс для плагинов (расширение DSL) |
| **Plugin System** | Динамическая загрузка адаптеров |

**11 реализаций:**

Все реализации используют **DI через конструктор**:

```cpp
class SmartLinkRedirectService : public ISmartLinkRedirectService {
private:
    std::shared_ptr<IRedirectableSmartLink> redirectable_;
    std::shared_ptr<ISmartLinkRedirectRulesRepository> repository_;

public:
    // DI через конструктор
    SmartLinkRedirectService(
        std::shared_ptr<IRedirectableSmartLink> redirectable,
        std::shared_ptr<ISmartLinkRedirectRulesRepository> repository
    ) : redirectable_(redirectable), repository_(repository) {}

    std::optional<std::string> Evaluate() override {
        auto rules = repository_->Read();
        if (!rules.has_value() || rules->empty()) {
            return std::nullopt;
        }

        // Найти первое подходящее правило
        auto it = std::find_if(rules->begin(), rules->end(),
            [this](const auto& rule) {
                return redirectable_->IsLanguageAccepted(rule->GetLanguage()) &&
                       redirectable_->IsInTime(rule->GetStart(), rule->GetEnd());
            }
        );

        return it != rules->end() ?
            std::make_optional((*it)->GetRedirect()) : std::nullopt;
    }
};
```

### 4. MongoDB интеграция

**MongoDbRepository:**
```cpp
class MongoDbRepository : public IMongoDbRepository {
private:
    mongocxx::collection collection_;
    std::shared_ptr<ISmartLink> smart_link_;

public:
    std::optional<bsoncxx::document::value> Read() override {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;

        auto filter = document{}
            << "slug" << smart_link_->GetValue()
            << finalize;

        auto result = collection_.find_one(filter.view());

        return result ?
            std::make_optional(bsoncxx::document::value(result->view())) :
            std::nullopt;
    }
};
```

**SmartLinkRedirectRulesRepository - BSON десериализация:**
```cpp
std::optional<std::vector<shared_ptr<IRedirectRule>>> Read() override {
    auto doc = repository_->Read();
    if (!doc.has_value()) {
        return std::nullopt;
    }

    auto rules_array = doc->view()["rules"].get_array().value;

    std::vector<shared_ptr<IRedirectRule>> result;
    for (const auto& rule : rules_array) {
        auto rule_doc = rule.get_document().value;
        auto language = rule_doc["language"].get_utf8().value.to_string();
        auto redirect = rule_doc["redirect"].get_utf8().value.to_string();

        auto start_bson = rule_doc["start"].get_date();
        auto end_bson = rule_doc["end"].get_date();
        auto start = std::chrono::system_clock::time_point(start_bson.to_int64());
        auto end = std::chrono::system_clock::time_point(end_bson.to_int64());

        result.push_back(std::make_shared<RedirectRule>(
            language, redirect, start, end
        ));
    }

    return result;
}
```

### 5. DSL архитектура (Domain Specific Language)

**Цель:** Заменить JSON правила редиректа на декларативный DSL язык с поддержкой условий и плагинов.

#### 5.1 DSL синтаксис

**Базовый синтаксис:**
```
<condition> -> <url>
<condition1> -> <url1>; <condition2> -> <url2>  # Множественные правила
```

**Примеры:**
```
// Простое правило с языком
LANGUAGE = en-US -> https://example.com/en

// Правило с временным интервалом
DATETIME IN[2020-01-01T00:00:00Z, 2030-12-31T23:59:59Z] -> https://example.com

// JWT авторизация
AUTHORIZED -> https://example-private.com

// Комбинация условий (AND)
AUTHORIZED AND LANGUAGE = en-US -> https://private.example.com/en

// Множественные правила (fallback паттерн)
AUTHORIZED -> https://example-private.com; DATETIME < 9999-12-31T23:59:59Z -> https://example-public.com

// Сложное правило
LANGUAGE = ru-RU AND DATETIME IN[2020-01-01T00:00:00Z, 2030-12-31T23:59:59Z] -> https://example.ru
```

**Операторы условий:**
- `LANGUAGE = en-US` - проверка соответствия языка
- `LANGUAGE != en-US` - проверка несоответствия языка
- `LANGUAGE = any` или `LANGUAGE = *` - любой язык
- `DATETIME < 2026-12-31T23:59:59Z` - дата меньше
- `DATETIME <= 2026-12-31T23:59:59Z` - дата меньше или равна
- `DATETIME > 2026-01-01T00:00:00Z` - дата больше
- `DATETIME >= 2026-01-01T00:00:00Z` - дата больше или равна
- `DATETIME = 2026-01-01T00:00:00Z` - точное совпадение даты
- `DATETIME != 2026-01-01T00:00:00Z` - дата не равна
- `DATETIME IN[start, end]` - дата в интервале
- `AUTHORIZED` - JWT токен валиден
- `<cond1> AND <cond2>` - логическое И
- `<cond1> OR <cond2>` - логическое ИЛИ

#### 5.2 AST структура

**AST узлы:**

```cpp
enum class AstNodeType {
    IF_RULE,        // IF <cond> THEN <url>
    AND_EXPR,       // <expr> AND <expr>
    OR_EXPR,        // <expr> OR <expr>
    LANGUAGE_CALL,  // LANGUAGE("en-US")
    TIMEWINDOW_CALL,// TIMEWINDOW(start, end)
    ADAPTER_CALL,   // AUTHORIZED (plugin adapter)
    LITERAL         // TRUE, FALSE, строковые литералы
};

class AstNode {
public:
    virtual AstNodeType GetType() const = 0;
    virtual bool Evaluate(const IContext& context) = 0;
};
```

**Пример AST для правила:**
```
AUTHORIZED AND LANGUAGE = en-US -> https://private.com

└── RedirectRule
    ├── condition: AndExprNode
    │   ├── left: AuthorizedExpression
    │   └── right: LanguageEqualsExpression("en-US")
    └── redirect_url: "https://private.com"
```

#### 5.3 Парсер

**Процесс парсинга:**
```cpp
class DslParser {
public:
    std::vector<std::unique_ptr<AstNode>> Parse(
        const std::string& dsl_text
    );

private:
    std::unique_ptr<AstNode> ParseIfRule();
    std::unique_ptr<AstNode> ParseCondition();
    std::unique_ptr<AstNode> ParseAndExpr();
    std::unique_ptr<AstNode> ParseOrExpr();
    std::unique_ptr<AstNode> ParsePrimaryExpr();
};
```

**Этапы:**
1. Лексический анализ (tokenization)
2. Синтаксический анализ (AST построение)
3. Создание узлов AST для каждого правила

#### 5.4 Evaluator

**Вычисление AST:**
```cpp
class Evaluator {
private:
    std::shared_ptr<IContext> context_;

public:
    std::optional<std::string> Evaluate(
        const std::vector<std::unique_ptr<AstNode>>& rules
    ) {
        for (const auto& rule : rules) {
            if (rule->Evaluate(*context_)) {
                // Получить URL из правила
                return rule->GetUrl();
            }
        }
        return std::nullopt;  // Нет подходящего правила
    }
};
```

#### 5.5 Context (хранилище данных)

**IContext интерфейс:**
```cpp
class IContext {
public:
    virtual void set(const std::string& key, std::any value) = 0;
    virtual std::any get(const std::string& key) const = 0;
};

class Context : public IContext {
private:
    std::unordered_map<std::string, std::any> storage_;

public:
    void set(const std::string& key, std::any value) override {
        storage_[key] = value;
    }

    std::any get(const std::string& key) const override {
        auto it = storage_.find(key);
        return it != storage_.end() ? it->second : std::any{};
    }
};
```

**Регистрация данных в Context:**
```cpp
// В middleware
auto context = std::make_shared<Context>();

// HTTP request
context->set("request", req);

// Accept-Language header
auto lang = req->getHeader("Accept-Language");
context->set("language", lang);

// JWT validator
auto jwt_validator = IoC::Resolve<std::shared_ptr<IJwtValidator>>(
    "IJwtValidator"
);
context->set("jwt_validator", jwt_validator);
```

#### 5.6 Plugin System (Adapters)

**IAdapter интерфейс:**
```cpp
class IAdapter {
public:
    virtual ~IAdapter() = default;
    virtual bool Evaluate(const IContext& context) const = 0;
};
```

**Пример адаптера - AUTHORIZED:**

```cpp
// plugins/authorized/src/context_2_authorized_accessible.cpp
class Context2AuthorizedAccessible : public IAdapter {
private:
    std::shared_ptr<IContext> context_;

public:
    Context2AuthorizedAccessible(std::shared_ptr<IContext> ctx)
        : context_(ctx) {}

    bool Evaluate(const IContext& context) const override {
        return is_authorized();
    }

private:
    bool is_authorized() const {
        // 1. Получить request из контекста
        auto request_any = context_->get("request");
        if (!request_any.has_value()) return false;
        auto request = std::any_cast<drogon::HttpRequestPtr>(request_any);

        // 2. Получить JWT validator из контекста
        auto jwt_validator_any = context_->get("jwt_validator");
        if (!jwt_validator_any.has_value()) return false;
        auto jwt_validator = std::any_cast<
            std::shared_ptr<domain::IJwtValidator>
        >(jwt_validator_any);

        // 3. Извлечь JWT токен из Authorization header
        auto auth_header = request->getHeader("Authorization");
        if (auth_header.empty()) return false;

        const std::string bearer_prefix = "Bearer ";
        if (auth_header.find(bearer_prefix) != 0) return false;

        std::string token = auth_header.substr(bearer_prefix.length());

        // 4. Валидировать токен
        return jwt_validator->Validate(token);
    }
};
```

**Регистрация адаптера:**
```cpp
// Создать фабрику адаптеров
AdapterFactory factory;
factory.Register("AUTHORIZED", [](std::shared_ptr<IContext> ctx) {
    return std::make_shared<Context2AuthorizedAccessible>(ctx);
});

// В парсере - создать AdapterCallNode
auto adapter = factory.Create("AUTHORIZED", context);
```

#### 5.7 JWT интеграция

**IJwtValidator интерфейс:**
```cpp
class IJwtValidator {
public:
    virtual ~IJwtValidator() = default;
    virtual bool Validate(const std::string& token) = 0;
};
```

**JwtValidator реализация:**
```cpp
class JwtValidator : public IJwtValidator {
private:
    std::string jwks_uri_;  // http://localhost:3000/auth/.well-known/jwks.json
    std::string issuer_;    // smartlinks-jwt-service
    std::string audience_;  // smartlinks-api

public:
    bool Validate(const std::string& token) override {
        try {
            // 1. Получить публичный ключ из JWKS endpoint
            auto public_key = FetchPublicKeyFromJWKS();

            // 2. Создать verifier с RS256
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::rs256(public_key))
                .with_issuer(issuer_)
                .with_audience(audience_);

            // 3. Декодировать и верифицировать токен
            auto decoded = jwt::decode(token);
            verifier.verify(decoded);

            return true;  // Токен валиден
        } catch (...) {
            return false;  // Ошибка валидации
        }
    }
};
```

**Конфигурация (main.cpp):**
```cpp
// Читать из переменных окружения или config.json
const char* env_jwks_uri = std::getenv("JWT_JWKS_URI");
std::string jwks_uri = env_jwks_uri
    ? env_jwks_uri
    : config.get("JWT", Json::Value())
        .get("JwksUri", "http://jwt-service:3000/auth/.well-known/jwks.json")
        .asString();

// Зарегистрировать IJwtValidator в IoC
IoC::Resolve<shared_ptr<ICommand>>("IoC.Register", Args{
    std::string("IJwtValidator"),
    DependencyFactory([jwks_uri, issuer, audience](const Args&) -> std::any {
        return std::static_pointer_cast<IJwtValidator>(
            std::make_shared<JwtValidator>(jwks_uri, issuer, audience)
        );
    })
})->Execute();
```

#### 5.8 Режимы работы

**USE_DSL_RULES environment variable:**

| Значение | Режим | Описание |
|----------|-------|----------|
| `false` | JSON/Legacy | Использовать старый SmartLinkRedirectService |
| `true` (default) | DSL | Использовать DSL Parser + Evaluator |

**Переключение в main.cpp:**
```cpp
const char* use_dsl = std::getenv("USE_DSL_RULES");
bool use_dsl_rules = (use_dsl == nullptr || std::string(use_dsl) != "false");

if (use_dsl_rules) {
    // Регистрировать DSL компоненты
    RegisterDslDependencies(root_scope);
} else {
    // Регистрировать Legacy компоненты
    RegisterLegacyDependencies(root_scope);
}
```

### 6. Dependency Injection стратегия

**Singleton:** MongoDB клиент, коллекция
```cpp
auto mongo_client = std::make_shared<mongocxx::client>(
    mongocxx::uri(connection_uri)
);

IoC::Resolve<shared_ptr<ICommand>>("IoC.Register", Args{
    std::string("MongoClient"),
    DependencyFactory([mongo_client](const Args&) -> std::any {
        return mongo_client;
    })
})->Execute();
```

**Scoped:** Repositories, Services
```cpp
IoC::Resolve<shared_ptr<ICommand>>("IoC.Register", Args{
    std::string("IMongoDbRepository"),
    DependencyFactory([](const Args&) -> std::any {
        auto collection = IoC::Resolve<mongocxx::collection>("MongoCollection");
        auto smart_link = IoC::Resolve<shared_ptr<ISmartLink>>("ISmartLink");

        // КРИТИЧНО: static_pointer_cast к интерфейсу!
        return std::static_pointer_cast<IMongoDbRepository>(
            std::make_shared<MongoDbRepository>(collection, smart_link)
        );
    })
})->Execute();
```

**Transient:** Middleware, HTTP-зависимые объекты
```cpp
IoC::Resolve<shared_ptr<ICommand>>("IoC.Register", Args{
    std::string("MethodNotAllowedMiddleware"),
    DependencyFactory([](const Args&) -> std::any {
        auto supported = IoC::Resolve<shared_ptr<ISupportedHttpRequest>>(
            "ISupportedHttpRequest"
        );

        // КРИТИЧНО: static_pointer_cast к IMiddleware!
        return std::static_pointer_cast<IMiddleware>(
            std::make_shared<MethodNotAllowedMiddleware>(supported)
        );
    })
})->Execute();
```

### 6. Exception Handling

**ExceptionHandler регистрация:**
```cpp
IoC::Resolve<shared_ptr<ICommand>>("IoC.Register", Args{
    std::string("ExceptionHandler"),
    DependencyFactory([](const Args&) -> std::any {
        return std::function<void(
            shared_ptr<IMiddleware>,
            std::exception_ptr
        )>([](auto middleware, auto ex_ptr) {
            try {
                std::rethrow_exception(ex_ptr);
            } catch (const std::exception& e) {
                LOG_ERROR << "Middleware exception: " << e.what();
            } catch (...) {
                LOG_ERROR << "Unknown middleware exception";
            }
        });
    })
})->Execute();
```

## 🔄 Request lifecycle

```
HTTP GET /{slug}
    ↓
DrogonMiddlewareAdapter::invoke()
    ↓
1. Создать request-scoped IoC контейнер
2. Установить как текущий (thread_local)
3. Зарегистрировать HTTP-зависимости:
   - HttpRequest
   - ISmartLink (из req->path())
   - ISupportedHttpRequest
   - IRedirectableSmartLink
4. Создать единый Response объект
5. Получить middleware pipeline из IoC (Transient - создаются заново!)
6. Получить ExceptionHandler из IoC
7. Построить Chain of Responsibility
8. Выполнить pipeline:
    ↓
    MethodNotAllowedMiddleware (GET/HEAD?) → 405 или next
        ↓
    NotFoundMiddleware (exists? state != deleted?) → 404 или next
        ↓
    UnprocessableContentMiddleware (state == freezed?) → 422 или next
        ↓
    TemporaryRedirectMiddleware (find rule?) → 307 или next
        ↓
    [End of chain] → 429 (если статус всё ещё 200)
    ↓
9. Обработка исключений через ExceptionHandler
10. Вернуть response в Drogon
11. Очистить request scope
```

## 🎨 Паттерны проектирования

| Паттерн | Использование |
|---------|---------------|
| **Dependency Injection** | Все классы получают зависимости через конструктор |
| **Inversion of Control** | Custom IoC контейнер с функциональным подходом |
| **Chain of Responsibility** | Middleware pipeline |
| **Command** | IoC операции обёрнуты в ICommand |
| **Repository** | MongoDB доступ через IMongoDbRepository |
| **Adapter** | DrogonMiddlewareAdapter интегрирует middleware с Drogon |
| **Strategy** | IoC::strategy_ обновляется динамически |
| **Template Method** | Middleware::Invoke с вызовом next() |
| **Factory** | DependencyFactory создаёт объекты через lambda |

## 🧪 Тестирование

### IoC Unit Tests (20 тест-кейсов)

**Файл:** `tests/unit/ioc_tests.cpp`

- ✅ Инициализация root scope
- ✅ Регистрация базовых зависимостей
- ✅ Регистрация через IoC.Register
- ✅ Регистрация с аргументами
- ✅ Создание новых scopes
- ✅ Thread-local текущий scope
- ✅ Очистка текущего scope
- ✅ Иерархическое разрешение (parent scopes)
- ✅ Child scope переопределяет parent
- ✅ Обработка отсутствующих зависимостей
- ✅ Thread-local изоляция между потоками
- ✅ GetCurrent возвращает root когда current == null
- ✅ CreateEmptyScope без parent
- ✅ CreateScope с явным parent
- ✅ NullCommand ничего не делает
- ✅ DependencyResolver прямой тест
- ✅ RegisterDependencyCommand регистрирует в текущем scope
- ✅ Двойная инициализация безопасна
- ✅ Resolve с неправильным типом → bad_any_cast
- ✅ Resolve с неправильным типом указателя → bad_any_cast

### Integration Tests (35+ тест-кейсов)

**NO Cucumber. NO Gherkin. Just clean GTest.**

#### JSON/Legacy Mode Tests (17 тестов)

**Файл:** `tests/bdd/smartlinks_tests.cpp`

**Method Not Allowed (405) - 5 tests:**
```cpp
TEST_F(SmartLinksTest, Returns405_WhenPostRequestSent)
TEST_F(SmartLinksTest, Returns405_WhenPutRequestSent)
TEST_F(SmartLinksTest, Returns405_WhenDeleteRequestSent)
TEST_F(SmartLinksTest, DoesNotReturn405_WhenGetRequestSent)
TEST_F(SmartLinksTest, DoesNotReturn405_WhenHeadRequestSent)
```

**Not Found (404) - 3 tests:**
```cpp
TEST_F(SmartLinksTest, Returns404_WhenSlugDoesNotExist)
TEST_F(SmartLinksTest, Returns404_WhenSlugIsDeleted)
TEST_F(SmartLinksTest, DoesNotReturn404_WhenSlugExistsAndIsActive)
```

**Unprocessable Content (422) - 2 tests:**
```cpp
TEST_F(SmartLinksTest, Returns422_WhenSlugIsFreezed)
TEST_F(SmartLinksTest, DoesNotReturn422_WhenSlugIsActive)
```

**Temporary Redirect (307/429) - 7 tests:**
```cpp
TEST_F(SmartLinksTest, Returns307_WithUnconditionalRedirectRule)
TEST_F(SmartLinksTest, Returns307_WhenAcceptLanguageMatchesRuleLanguage)
TEST_F(SmartLinksTest, Returns307_WhenWildcardLanguageIsUsed)
TEST_F(SmartLinksTest, Returns307_WhenCurrentTimeIsWithinRuleTimeWindow)
TEST_F(SmartLinksTest, DoesNotReturn307_WhenCurrentTimeIsOutsideRuleTimeWindow)
TEST_F(SmartLinksTest, Returns307_WithFirstMatchingRuleWhenMultipleRulesExist)
TEST_F(SmartLinksTest, Returns429_WhenNoRuleMatches)
```

#### DSL Mode Tests (18+ тестов)

**Файл:** `tests/bdd/smartlinks_dsl_tests.cpp`

**DSL Parser Tests:**
```cpp
TEST_F(SmartLinksDslTest, ParsesSimpleIfRule)
TEST_F(SmartLinksDslTest, ParsesAndExpression)
TEST_F(SmartLinksDslTest, ParsesOrExpression)
TEST_F(SmartLinksDslTest, ParsesLanguageCall)
TEST_F(SmartLinksDslTest, ParsesTimeWindowCall)
```

**DSL Evaluator Tests:**
```cpp
TEST_F(SmartLinksDslTest, Returns307_WithSimpleLanguageRule)
TEST_F(SmartLinksDslTest, Returns307_WithAndCondition)
TEST_F(SmartLinksDslTest, Returns307_WithOrCondition)
TEST_F(SmartLinksDslTest, Returns307_WithTimeWindowRule)
TEST_F(SmartLinksDslTest, Returns429_WhenNoRuleMatches)
```

**JWT Authorization Tests:**
```cpp
TEST_F(SmartLinksDslTest, Returns307_WithAuthorizedRule)
TEST_F(SmartLinksDslTest, Returns429_WithoutAuthorizationHeader)
TEST_F(SmartLinksDslTest, Returns429_WithInvalidToken)
TEST_F(SmartLinksDslTest, Returns429_WithExpiredToken)
```

**Plugin Adapter Tests:**
```cpp
TEST_F(SmartLinksDslTest, AdapterRegistersCorrectly)
TEST_F(SmartLinksDslTest, AdapterEvaluatesWithContext)
TEST_F(SmartLinksDslTest, AdapterThrowsOnMissingDependency)
```

**Test Context:**
```cpp
class TestContext {
    // MongoDB operations
    void InsertSmartLink(slug, state);
    void InsertSmartLinkWithRule(slug, language, redirect, start, end);
    void DeleteSmartLink(slug);

    // HTTP client
    void SendRequest(path, method, headers = {});

    // Assertions
    int GetLastStatusCode();
    std::string GetResponseHeader(name);
};
```

**Coverage: 100% всех C# BDD сценариев + 1 bonus test**

## 🔧 Технические решения

### Почему custom IoC вместо Boost.DI?

- ✅ Функциональный подход (как в appserver)
- ✅ Иерархические scopes из коробки
- ✅ Thread-local поддержка
- ✅ Динамическая регистрация через команды
- ✅ Соответствие архитектуре appserver
- ✅ Request-scoped dependencies

### Почему Drogon?

- ✅ Нативный C++ HTTP framework
- ✅ Поддержка middleware через handlers
- ✅ Async через callbacks (не coroutines)
- ✅ Встроенный JSON парсер
- ✅ Активное сообщество
- ✅ Production-ready

### Почему mongocxx?

- ✅ Официальный драйвер MongoDB
- ✅ Поддержка BSON builder stream API
- ✅ Modern C++ API (C++17)
- ✅ Connection pooling встроен
- ✅ Thread-safe

### Почему GTest вместо Cucumber?

- ✅ Простая установка (`find_package(GTest)`)
- ✅ Нативный C++ debugger
- ✅ Быстрое выполнение (no IPC overhead)
- ✅ Type-safe (компилируется)
- ✅ Работает везде (CI/CD friendly)
- ✅ Arrange-Act-Assert pattern
- ❌ Cucumber-cpp wire protocol сложен
- ❌ Gauge C++ plugin устарел

## 📊 Метрики

| Метрика | Значение |
|---------|----------|
| Строк кода | ~5000+ |
| Интерфейсов | 10 (базовых) + 3 (DSL) |
| Реализаций | 11 (базовых) + DSL компоненты |
| Middleware | 4 |
| DSL компонентов | Parser, AST, Evaluator, Context |
| Plugin адаптеров | 1+ (AUTHORIZED, расширяемо) |
| IoC unit tests | 20 |
| Integration tests (JSON) | 17 |
| Integration tests (DSL) | 18+ |
| JWT microservice tests | 40 |
| **Total tests** | **95+** |
| Файлов заголовков | 50+ |
| Файлов реализации | 35+ |
| Docker stages | 3 (multi-stage build) |
| GitHub workflows | 3 (build, test, coverage) |
| Поддержка режимов | 2 (JSON/Legacy + DSL) |

## 🚨 Критические моменты

### 1. Type Safety в IoC

**Проблема:** `std::any` теряет информацию о типах

**Решение:** Всегда использовать `std::static_pointer_cast` к интерфейсу:

```cpp
// ❌ BAD - возвращает std::shared_ptr<ConcreteClass>
return std::make_shared<ConcreteClass>(...);

// ✅ GOOD - возвращает std::shared_ptr<IInterface>
return std::static_pointer_cast<IInterface>(
    std::make_shared<ConcreteClass>(...)
);
```

### 2. BSON Array Construction

**Проблема:** `bsoncxx::builder::stream::array{}` не существует

**Решение:** Использовать `open_array` / `close_array`:

```cpp
// ❌ BAD
auto doc = document{} << "rules" << array{} << finalize;

// ✅ GOOD
auto doc = document{}
    << "rules" << open_array
        << open_document
            << "language" << "en-US"
        << close_document
    << close_array
    << finalize;
```

### 3. Единый Response объект

**Проблема:** Middleware не должны создавать новые Response

**Решение:** Один Response создаётся в адаптере, передаётся по ссылке:

```cpp
// В адаптере
auto response = HttpResponse::newHttpResponse();  // ОДИН РАЗ
pipeline(req, response);  // Передаём по ссылке

// В middleware
void Invoke(req, response, next) {
    response->setStatusCode(404);  // Модифицируем существующий
}
```

### 4. DSL критические моменты

#### Адаптер должен создаваться ОДИН РАЗ при парсинге

**Проблема:** Создание адаптера в Evaluate() приводит к проблемам с lifetime

**Решение:** Адаптер создаётся в конструкторе AdapterCallNode:

```cpp
// ❌ BAD - создание при каждом Evaluate
bool AdapterCallNode::Evaluate(const IContext& context) const {
    auto adapter = factory_->Create(adapter_name_, context);  // WRONG!
    return adapter->Evaluate(context);
}

// ✅ GOOD - создание в конструкторе
AdapterCallNode::AdapterCallNode(
    const std::string& name,
    std::shared_ptr<IAdapterFactory> factory,
    std::shared_ptr<IContext> context
) : adapter_name_(name), factory_(factory) {
    adapter_ = factory_->Create(name, context);  // Создаём ОДИН РАЗ
}

bool AdapterCallNode::Evaluate(const IContext& context) const {
    return adapter_->Evaluate(context);  // Используем существующий
}
```

#### Context lifetime и адаптеры

**Проблема:** Адаптер хранит shared_ptr<IContext>, который должен жить достаточно долго

**Решение:** Context создаётся в TemporaryRedirectMiddleware и живёт на протяжении всего request lifecycle:

```cpp
void TemporaryRedirectMiddleware::Invoke(...) {
    // 1. Создать context (будет жить весь запрос)
    auto context = std::make_shared<dsl::Context>();

    // 2. Заполнить данными
    context->set("request", req);
    context->set("jwt_validator", jwt_validator_);

    // 3. Парсинг создаст адаптеры с ссылками на context
    auto rules = parser_->Parse(dsl_text, context);

    // 4. Evaluate использует уже созданные адаптеры
    auto redirect = evaluator_->Evaluate(rules, context);

    // 5. Context и адаптеры уничтожаются в конце Invoke
}
```

#### JWT валидация - правильная конфигурация

**Проблема:** Неполный JWT_JWKS_URI приводит к ошибкам валидации

**Решение:** Всегда указывать полный путь к JWKS endpoint:

```bash
# ❌ BAD - отсутствует путь
JWT_JWKS_URI=http://localhost:3000

# ✅ GOOD - полный путь к JWKS
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json
```

**В тестах (run_all_tests.sh):**
```bash
# JSON mode
MONGODB_DATABASE=LinksTest USE_DSL_RULES=false \
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json \
./smartlinks > smartlinks_json_test.log 2>&1 &

# DSL mode
MONGODB_DATABASE=LinksDSLTest \
JWT_JWKS_URI=http://localhost:3000/auth/.well-known/jwks.json \
./smartlinks > smartlinks_dsl_test.log 2>&1 &
```

#### Адаптер получает зависимости из Context

**Ключевой момент:** Адаптеры НЕ должны напрямую использовать IoC - они получают всё из Context:

```cpp
// ✅ GOOD - всё из context
class Context2AuthorizedAccessible : public IAdapter {
public:
    bool Evaluate(const IContext& context) const override {
        // Получаем request из контекста
        auto request_any = context_->get("request");
        auto request = std::any_cast<drogon::HttpRequestPtr>(request_any);

        // Получаем jwt_validator из контекста
        auto jwt_validator_any = context_->get("jwt_validator");
        auto jwt_validator = std::any_cast<
            std::shared_ptr<IJwtValidator>
        >(jwt_validator_any);

        // Используем полученные зависимости
        return ValidateJwt(request, jwt_validator);
    }
};
```

#### Обработка ошибок в адаптерах

**Важно:** Адаптер должен возвращать false при любых ошибках, а не бросать исключения:

```cpp
bool Context2AuthorizedAccessible::is_authorized() const {
    try {
        auto request_any = context_->get("request");
        if (!request_any.has_value()) {
            return false;  // Нет request - не авторизован
        }

        auto jwt_validator_any = context_->get("jwt_validator");
        if (!jwt_validator_any.has_value()) {
            return false;  // Нет validator - не авторизован
        }

        // ... валидация токена
        return jwt_validator->Validate(token);

    } catch (...) {
        return false;  // Любая ошибка = не авторизован
    }
}
```

## 🔮 Будущие улучшения

### DSL расширения
- [ ] Дополнительные встроенные функции (COUNTRY, DEVICE, USER_AGENT)
- [ ] Поддержка переменных в DSL (`SET var = value`)
- [ ] NOT оператор (`IF NOT AUTHORIZED THEN ...`)
- [ ] Вложенные условия с приоритетом
- [ ] DSL валидация на этапе загрузки правил

### Производительность
- [ ] Redis caching для правил редиректа и JWT публичных ключей
- [ ] Кэширование AST после парсинга
- [ ] Performance benchmarks (ab, wrk)
- [ ] Connection pooling для JWT JWKS endpoint

### Мониторинг и наблюдаемость
- [ ] Prometheus метрики endpoint
- [ ] Health check endpoint (`/health`)
- [ ] Distributed tracing (OpenTelemetry)
- [ ] Structured logging (JSON format)
- [ ] Метрики DSL evaluator (время парсинга, кэш-хиты)

### Инфраструктура
- [ ] Graceful shutdown (SIGTERM handling)
- [ ] Rate limiting middleware (по IP, по токену)
- [ ] Kubernetes deployment manifests
- [ ] Horizontal scaling support
- [ ] Admin API для управления правилами

### JWT интеграция
- [x] ~~JWT валидация через AUTHORIZED expression~~ ✅
- [x] ~~RS256 поддержка~~ ✅
- [x] ~~JWKS endpoint интеграция~~ ✅
- [ ] JWT claims в DSL (`IF JWT.ROLE("admin") THEN ...`)
- [ ] Кэширование публичных ключей
- [ ] Мультитенантность (разные JWT issuers)
