# Design Patterns в SmartLinks C++

## 1. Dependency Injection (DI)

**Описание**: Паттерн, при котором зависимости объекта предоставляются извне (обычно через конструктор), а не создаются внутри объекта. Это обеспечивает слабую связанность компонентов и упрощает тестирование.

**Использование в проекте**:
- **Класс/Файл**: `include/impl/mongodb_repository.hpp:31-35`
- **Как реализован**: Все зависимости передаются через конструкторы классов. Например, `MongoDbRepository` получает `mongocxx::collection` и `ISmartLink` через конструктор, а не создает их самостоятельно. Аналогично, все middleware получают свои сервисы и репозитории через конструкторы.
- **Почему это этот паттерн**:
  - Зависимости передаются извне (через конструктор), а не создаются внутри класса
  - Класс зависит от абстракций (интерфейсов `ISmartLink`, `IMongoDbRepository`), а не от конкретных реализаций
  - Обеспечивает принцип инверсии зависимостей (Dependency Inversion Principle)
  - Упрощает тестирование за счет возможности подстановки mock-объектов

**Пример кода**:
```cpp
// include/impl/mongodb_repository.hpp
class MongoDbRepository : public domain::IMongoDbRepository {
private:
    mongocxx::collection collection_;
    std::shared_ptr<domain::ISmartLink> smart_link_;

public:
    /**
     * @brief Конструктор с DI
     * @param collection MongoDB коллекция (DI через конструктор)
     * @param smart_link ISmartLink для получения slug (DI через конструктор)
     */
    MongoDbRepository(
        mongocxx::collection collection,
        std::shared_ptr<domain::ISmartLink> smart_link
    ) : collection_(std::move(collection)),
        smart_link_(std::move(smart_link)) {}
};

// include/middleware/temporary_redirect_middleware.hpp
class TemporaryRedirectMiddleware : public IMiddleware {
private:
    std::shared_ptr<domain::ISmartLinkRedirectService> smart_link_redirect_service_;

public:
    explicit TemporaryRedirectMiddleware(
        std::shared_ptr<domain::ISmartLinkRedirectService> smartLinkRedirectService
    ) : smart_link_redirect_service_(std::move(smartLinkRedirectService)) {}
};
```

## 2. Inversion of Control (IoC) Container

**Описание**: Паттерн, при котором контейнер управляет созданием объектов и их зависимостями. Контейнер отвечает за разрешение зависимостей и их внедрение в объекты.

**Использование в проекте**:
- **Класс/Файл**: `include/ioc/ioc.hpp:17-32`
- **Как реализован**: Класс `IoC` с статическим методом `Resolve<T>()` для разрешения зависимостей. Использует `DependencyResolver` с иерархией scope'ов для поиска зарегистрированных фабрик. Регистрация зависимостей происходит через команду `IoC.Register` в `main.cpp`.
- **Почему это этот паттерн**:
  - Централизованное управление созданием объектов
  - Автоматическое разрешение зависимостей через `IoC::Resolve<T>()`
  - Поддержка различных времен жизни (Singleton, Scoped, Transient)
  - Использует Service Locator паттерн для доступа к зависимостям
  - Инверсия управления: фреймворк вызывает код приложения, а не наоборот

**Пример кода**:
```cpp
// include/ioc/ioc.hpp
class IoC {
private:
    static ResolveStrategy strategy_;

public:
    // Разрешение зависимости
    template<typename T>
    static T Resolve(const std::string& dependency, const Args& args = {}) {
        std::any result = strategy_(dependency, args);
        return std::any_cast<T>(result);
    }
};

// src/main.cpp:104-118
// Регистрация зависимости
ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
    "IoC.Register",
    ioc::Args{
        std::string("IMongoDbRepository"),
        ioc::DependencyFactory([](const ioc::Args&) -> std::any {
            auto collection = ioc::IoC::Resolve<mongocxx::collection>("MongoCollection");
            auto smart_link = ioc::IoC::Resolve<std::shared_ptr<domain::ISmartLink>>(
                "ISmartLink"
            );
            return std::static_pointer_cast<domain::IMongoDbRepository>(
                std::make_shared<impl::MongoDbRepository>(collection, smart_link)
            );
        })
    }
)->Execute();

// Использование
auto repository = ioc::IoC::Resolve<std::shared_ptr<domain::IMongoDbRepository>>(
    "IMongoDbRepository"
);
```

## 3. Command Pattern

**Описание**: Паттерн, инкапсулирующий запрос как объект, тем самым позволяя параметризовать клиентов с различными запросами, ставить запросы в очередь, логировать запросы и поддерживать отмену операций.

**Использование в проекте**:
- **Класс/Файл**: `include/ioc/command.hpp:6-16`
- **Как реализован**: Интерфейс `ICommand` с методом `Execute()`. Конкретные команды: `RegisterDependencyCommand`, `SetCurrentScopeCommand`, `ClearCurrentScopeCommand`, `UpdateIocResolveDependencyStrategyCommand`, `InitCommand`. Все операции с IoC контейнером выполняются через команды.
- **Почему это этот паттерн**:
  - Интерфейс `ICommand` с единственным методом `Execute()`
  - Каждая команда инкапсулирует конкретное действие (регистрация зависимости, установка scope и т.д.)
  - Команды могут быть созданы и выполнены в разное время
  - Унифицированный интерфейс для различных операций
  - Присутствует Null Object паттерн (`NullCommand`) для случаев, когда действие не требуется

**Пример кода**:
```cpp
// include/ioc/command.hpp
// Интерфейс команды
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0;
};

// Команда-заглушка (Null Object Pattern)
class NullCommand : public ICommand {
public:
    void Execute() override {}
};

// include/ioc/register_dependency_command.hpp
class RegisterDependencyCommand : public ICommand {
private:
    std::string dependency_;
    DependencyFactory factory_;

public:
    RegisterDependencyCommand(
        const std::string& dependency,
        const DependencyFactory& factory
    ) : dependency_(dependency), factory_(factory) {}

    void Execute() override {
        auto current_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>(
            "IoC.Scope.Current"
        );
        current_scope->set(dependency_, factory_);
    }
};

// Использование
ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
    "IoC.Register",
    ioc::Args{dependency_name, factory}
)->Execute();
```

## 4. Chain of Responsibility

**Описание**: Паттерн, позволяющий избежать жесткой привязки отправителя запроса к его получателю, давая возможность обработать запрос нескольким объектам. Связывает объекты-получатели в цепочку и передает запрос по цепочке, пока какой-нибудь объект его не обработает.

**Использование в проекте**:
- **Класс/Файл**: `include/middleware/i_middleware.hpp:32-54`
- **Как реализован**: Middleware pipeline построен как цепочка обработчиков. Каждый middleware реализует интерфейс `IMiddleware` с методом `Invoke(request, response, next)`. Метод `next` является делегатом следующего обработчика в цепочке. Цепочка строится в `DrogonMiddlewareAdapter::BuildPipeline()` путем рекурсивного оборачивания middleware в lambda-функции.
- **Почему это этот паттерн**:
  - Цепочка обработчиков: MethodNotAllowed → NotFound → UnprocessableContent → TemporaryRedirect
  - Каждый middleware может обработать запрос и прервать цепочку или передать управление следующему через `next()`
  - Обработчики связаны неявно через параметр `RequestDelegate next`
  - Порядок обработки определяется порядком регистрации middleware
  - Если ни один обработчик не установил статус, используется значение по умолчанию (429)

**Пример кода**:
```cpp
// include/middleware/i_middleware.hpp
using RequestDelegate = std::function<void(
    const drogon::HttpRequestPtr& request,
    drogon::HttpResponsePtr& response
)>;

class IMiddleware {
public:
    virtual void Invoke(
        const drogon::HttpRequestPtr& request,
        drogon::HttpResponsePtr& response,
        RequestDelegate next
    ) = 0;
};

// src/middleware/temporary_redirect_middleware.cpp
void TemporaryRedirectMiddleware::Invoke(
    const drogon::HttpRequestPtr& request,
    drogon::HttpResponsePtr& response,
    RequestDelegate next
) {
    auto redirect_opt = smart_link_redirect_service_->Evaluate();

    if (redirect_opt.has_value()) {
        // Обработали запрос - прерываем цепочку
        response->setStatusCode(drogon::k307TemporaryRedirect);
        response->addHeader("Location", redirect_opt.value());
        // НЕ вызываем next
    } else {
        // Передаем следующему обработчику
        next(request, response);
    }
}

// src/middleware/drogon_middleware_adapter.cpp:181-209
RequestDelegate DrogonMiddlewareAdapter::BuildPipeline(
    const std::vector<std::shared_ptr<IMiddleware>>& middlewares
) {
    // Конец цепочки
    RequestDelegate pipeline = [](
        const drogon::HttpRequestPtr& req,
        drogon::HttpResponsePtr& resp
    ) {
        if (resp->statusCode() == drogon::k200OK) {
            resp->setStatusCode(static_cast<drogon::HttpStatusCode>(429));
        }
    };

    // Строим цепочку с конца
    for (auto it = middlewares.rbegin(); it != middlewares.rend(); ++it) {
        auto middleware = *it;
        auto next = pipeline;

        pipeline = [middleware, next](
            const drogon::HttpRequestPtr& req,
            drogon::HttpResponsePtr& resp
        ) {
            middleware->Invoke(req, resp, next);
        };
    }

    return pipeline;
}
```

## 5. Repository Pattern

**Описание**: Паттерн, который инкапсулирует логику доступа к данным и предоставляет абстракцию над источником данных. Репозиторий предоставляет коллекционно-подобный интерфейс для доступа к доменным объектам.

**Использование в проекте**:
- **Класс/Файл**: `include/domain/i_mongodb_repository.hpp:14-25`
- **Как реализован**: Интерфейсы репозиториев (`IMongoDbRepository`, `IStatableSmartLinkRepository`, `ISmartLinkRedirectRulesRepository`) определяют контракт для доступа к данным. Конкретные реализации (`MongoDbRepository`, `StatableSmartLinkRepository`, `SmartLinkRedirectRulesRepository`) инкапсулируют работу с MongoDB. Репозитории возвращают доменные объекты, скрывая детали работы с BSON.
- **Почему это этот паттерн**:
  - Абстракция доступа к данным через интерфейсы (IMongoDbRepository, IStatableSmartLinkRepository)
  - Инкапсуляция логики работы с MongoDB (BSON, queries)
  - Возврат доменных объектов вместо сырых данных
  - Разделение бизнес-логики и логики доступа к данным
  - Упрощение тестирования через возможность подмены реализации

**Пример кода**:
```cpp
// include/domain/i_mongodb_repository.hpp
class IMongoDbRepository {
public:
    virtual ~IMongoDbRepository() = default;
    virtual std::optional<bsoncxx::document::value> Read() = 0;
};

// include/impl/mongodb_repository.hpp
class MongoDbRepository : public domain::IMongoDbRepository {
private:
    mongocxx::collection collection_;
    std::shared_ptr<domain::ISmartLink> smart_link_;

public:
    std::optional<bsoncxx::document::value> Read() override {
        auto slug = smart_link_->GetValue();

        // Инкапсуляция работы с MongoDB
        auto filter = document{}
            << "slug" << slug
            << finalize;

        auto result = collection_.find_one(filter.view());

        if (result) {
            return bsoncxx::document::value(result->view());
        }
        return std::nullopt;
    }
};

// include/impl/statable_smart_link_repository.hpp
class StatableSmartLinkRepository : public domain::IStatableSmartLinkRepository {
private:
    std::shared_ptr<domain::IMongoDbRepository> repository_;

public:
    std::optional<std::shared_ptr<domain::IStatableSmartLink>> Read() override {
        // Использует низкоуровневый репозиторий
        auto doc_opt = repository_->Read();
        if (!doc_opt.has_value()) {
            return std::nullopt;
        }

        auto doc = doc_opt->view();
        auto state_element = doc["state"];
        std::string state(state_element.get_string().value);

        // Возвращает доменный объект
        return std::make_shared<StatableSmartLink>(state);
    }
};
```

## 6. Strategy Pattern

**Описание**: Паттерн, определяющий семейство алгоритмов, инкапсулирующий каждый из них и делающий их взаимозаменяемыми. Стратегия позволяет изменять алгоритм независимо от клиентов, которые его используют.

**Использование в проекte**:
- **Класс/Файл**: `include/ioc/ioc.hpp:15-19`
- **Как реализован**: В IoC контейнере используется `ResolveStrategy` - функциональный объект, который определяет стратегию разрешения зависимостей. Стратегия может быть изменена через `UpdateIocResolveDependencyStrategyCommand`. Изначально используется простая стратегия, затем она обновляется на `DependencyResolver` с поддержкой иерархии scope'ов.
- **Почему это этот паттерн**:
  - Определено семейство алгоритмов разрешения зависимостей через тип `ResolveStrategy`
  - Стратегия может быть заменена во время выполнения через `UpdateIocResolveDependencyStrategyCommand`
  - Клиент (`IoC::Resolve`) не знает о конкретной реализации стратегии
  - Алгоритм разрешения зависимостей инкапсулирован в функциональном объекте
  - Используется функциональный подход (std::function) вместо классической иерархии классов

**Пример кода**:
```cpp
// include/ioc/ioc.hpp
// Определение типа стратегии
using ResolveStrategy = std::function<std::any(const std::string&, const Args&)>;

class IoC {
private:
    static ResolveStrategy strategy_;  // Текущая стратегия

public:
    template<typename T>
    static T Resolve(const std::string& dependency, const Args& args = {}) {
        std::any result = strategy_(dependency, args);  // Использование стратегии
        return std::any_cast<T>(result);
    }
};

// include/ioc/update_ioc_resolve_dependency_strategy_command.hpp
using StrategyUpdater = std::function<ResolveStrategy(ResolveStrategy)>;

class UpdateIocResolveDependencyStrategyCommand : public ICommand {
private:
    StrategyUpdater updater_;

public:
    void Execute() override {
        IoC::strategy_ = updater_(IoC::strategy_);  // Замена стратегии
    }
};

// src/ioc/init_command.cpp:90-105
// Обновление стратегии на использование DependencyResolver
IoC::Resolve<std::shared_ptr<ICommand>>(
    "Update Ioc Resolve Dependency Strategy",
    Args{
        StrategyUpdater([root](ResolveStrategy oldStrategy) -> ResolveStrategy {
            // Новая стратегия с поддержкой scope'ов
            return [root](const std::string& dependency, const Args& args) -> std::any {
                auto current = Scope::GetCurrent();
                if (!current) {
                    current = root;
                }
                DependencyResolver resolver(current);
                return resolver.Resolve(dependency, args);
            };
        })
    }
)->Execute();
```

## 7. Adapter Pattern

**Описание**: Паттерн, преобразующий интерфейс одного класса в интерфейс другого, который ожидают клиенты. Адаптер позволяет классам работать вместе, что было бы невозможно из-за несовместимых интерфейсов.

**Использование в проекте**:
- **Класс/Файл**: `include/middleware/drogon_middleware_adapter.hpp:32-76`
- **Как реализован**: Класс `DrogonMiddlewareAdapter` адаптирует собственный middleware pipeline проекта к интерфейсу Drogon фреймворка. Drogon ожидает callback-функцию с определенной сигнатурой, а адаптер преобразует это в вызовы методов `IMiddleware::Invoke()` с построением Chain of Responsibility.
- **Почему это этот паттерн**:
  - Преобразует интерфейс Drogon (callback-based) в интерфейс проекта (middleware pipeline)
  - Адаптирует сигнатуру `invoke(req, nextCb, mcb)` Drogon к `Invoke(request, response, next)` middleware
  - Позволяет использовать собственную архитектуру middleware с Drogon фреймворком
  - Скрывает детали интеграции с Drogon от остальной части приложения
  - Обеспечивает совместимость двух несовместимых интерфейсов

**Пример кода**:
```cpp
// include/middleware/drogon_middleware_adapter.hpp
class DrogonMiddlewareAdapter {
public:
    /**
     * @brief Обработать HTTP запрос (точка входа от Drogon)
     * Адаптирует интерфейс Drogon к нашему middleware pipeline
     */
    void invoke(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& nextCb,
        std::function<void(const drogon::HttpResponsePtr&)>&& mcb
    );

private:
    void RegisterHttpDependencies(const drogon::HttpRequestPtr& req);
    RequestDelegate BuildPipeline(
        const std::vector<std::shared_ptr<IMiddleware>>& middlewares
    );
};

// src/middleware/drogon_middleware_adapter.cpp:5-122
void DrogonMiddlewareAdapter::invoke(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& nextCb,
    std::function<void(const drogon::HttpResponsePtr&)>&& mcb
) {
    // 1. Создать request-scoped IoC контейнер
    auto request_scope = ioc::IoC::Resolve<std::shared_ptr<ioc::IScopeDict>>(
        "IoC.Scope.Create"
    );

    // 2. Установить как текущий
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Scope.Current.Set",
        ioc::Args{request_scope}
    )->Execute();

    // 3. Регистрировать HTTP-специфичные зависимости
    RegisterHttpDependencies(req);

    // 4. Создать Response объект
    auto response = drogon::HttpResponse::newHttpResponse();

    // 5. Получить middleware pipeline
    std::vector<std::shared_ptr<IMiddleware>> middlewares;
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "GetMiddlewarePipeline",
        ioc::Args{std::ref(middlewares)}
    )->Execute();

    // 6. Построить Chain of Responsibility
    auto pipeline = BuildPipeline(middlewares);

    // 7. Запустить pipeline
    pipeline(req, response);

    // 8. Вернуть response в Drogon через callback
    mcb(response);

    // 9. Очистить scope
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Scope.Current.Clear"
    )->Execute();
}

// src/main.cpp:402-415
// Регистрация адаптера в Drogon
drogon::app().registerHandler(
    "/{slug}",
    [](const drogon::HttpRequestPtr& req,
       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        middleware::DrogonMiddlewareAdapter adapter;
        std::function<void(const drogon::HttpResponsePtr&)> empty_next =
            [](const drogon::HttpResponsePtr&){};
        adapter.invoke(req, std::move(empty_next), std::move(callback));
    },
    {drogon::Get, drogon::Head, drogon::Post, drogon::Put, drogon::Delete, drogon::Patch, drogon::Options}
);
```

## 8. Null Object Pattern

**Описание**: Паттерн, который предоставляет объект-заглушку вместо null-ссылки. Null Object реализует тот же интерфейс, что и реальные объекты, но его методы ничего не делают. Это позволяет избежать проверок на null и упрощает код.

**Использование в проекте**:
- **Класс/Файл**: `include/ioc/command.hpp:12-16`
- **Как реализован**: Класс `NullCommand` реализует интерфейс `ICommand` с пустым методом `Execute()`. Используется как возвращаемое значение в случаях, когда команда не требуется, но должен быть возвращен валидный объект команды (например, в `GetMiddlewarePipeline`). Это позволяет избежать проверок на nullptr и упрощает код.
- **Почему это этот паттерн**:
  - Предоставляет объект-заглушку вместо nullptr
  - Реализует тот же интерфейс `ICommand`, что и реальные команды
  - Метод `Execute()` ничего не делает (no-op)
  - Избавляет от необходимости проверять указатель на nullptr
  - Упрощает код вызывающей стороны - можно безопасно вызвать `Execute()` без проверок

**Пример кода**:
```cpp
// include/ioc/command.hpp
// Интерфейс команды
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0;
};

// Null Object - команда-заглушка
class NullCommand : public ICommand {
public:
    void Execute() override {}  // Ничего не делает
};

// src/main.cpp:351-355
// Использование Null Object
ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
    "GetMiddlewarePipeline",
    ioc::Args{
        std::string("GetMiddlewarePipeline"),
        ioc::DependencyFactory([](const ioc::Args& args) -> std::any {
            // ... код получения middleware ...

            // Возвращаем NullCommand (работа уже сделана через побочные эффекты)
            return std::static_pointer_cast<ioc::ICommand>(
                std::make_shared<ioc::NullCommand>()
            );
        })
    }
)->Execute();  // Безопасно вызываем Execute() без проверок на nullptr
```

## 9. Factory Method Pattern

**Описание**: Паттерн, определяющий общий интерфейс для создания объектов, но позволяющий подклассам или функциям решать, какой конкретный класс инстанцировать. Factory Method делегирует создание объектов фабричному методу.

**Использование в проекте**:
- **Класс/Файл**: `include/ioc/scope.hpp:12`
- **Как реализован**: `DependencyFactory` - это функциональный объект (тип `std::function<std::any(const Args&)>`), который инкапсулирует создание зависимостей. Каждая фабрика знает, как создать конкретный объект. Фабрики регистрируются в IoC контейнере через команду `IoC.Register` и вызываются при разрешении зависимостей через `IoC::Resolve`. Это функциональный подход к Factory Method паттерну.
- **Почему это этот паттерн**:
  - Определяет общий интерфейс создания объектов через тип `DependencyFactory`
  - Каждая фабрика инкапсулирует логику создания конкретного типа объекта
  - Создание объектов делегировано фабричным функциям, а не выполняется напрямую
  - Фабрики взаимозаменяемы - все имеют одинаковую сигнатуру
  - Позволяет изменять логику создания объектов без изменения кода клиента
  - Используется функциональный подход (lambda) вместо классической иерархии классов

**Пример кода**:
```cpp
// include/ioc/scope.hpp
// Определение типа фабричного метода
using DependencyFactory = std::function<std::any(const Args&)>;

// src/main.cpp:75-83
// Регистрация фабричного метода для MongoDB клиента
ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
    "IoC.Register",
    ioc::Args{
        std::string("MongoClient"),
        ioc::DependencyFactory([mongo_client](const ioc::Args&) -> std::any {
            // Фабричная функция знает, как создать/вернуть MongoClient
            return mongo_client;
        })
    }
)->Execute();

// src/main.cpp:104-118
// Регистрация фабричного метода для IMongoDbRepository
ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
    "IoC.Register",
    ioc::Args{
        std::string("IMongoDbRepository"),
        ioc::DependencyFactory([](const ioc::Args&) -> std::any {
            // Фабричная функция знает, как создать IMongoDbRepository
            auto collection = ioc::IoC::Resolve<mongocxx::collection>("MongoCollection");
            auto smart_link = ioc::IoC::Resolve<std::shared_ptr<domain::ISmartLink>>(
                "ISmartLink"
            );
            return std::static_pointer_cast<domain::IMongoDbRepository>(
                std::make_shared<impl::MongoDbRepository>(collection, smart_link)
            );
        })
    }
)->Execute();

// include/ioc/dependency_resolver.hpp:16-23
// Использование фабричного метода для создания объекта
std::any Resolve(const std::string& dependency, const Args& args) {
    auto current = scope_;
    while (current) {
        auto factory_opt = current->get(dependency);
        if (factory_opt != std::nullopt) {
            // Вызов фабричного метода для создания объекта
            return factory_opt.value()(args);
        }
        // ...
    }
}
```

## 10. Singleton Pattern

**Описание**: Паттерн, гарантирующий, что класс имеет только один экземпляр, и предоставляющий глобальную точку доступа к этому экземпляру.

**Использование в проекте**:
- **Класс/Файл**: `src/ioc/scope.cpp:6-13` и `src/main.cpp:70-101`
- **Как реализован**: Паттерн реализован в двух вариантах:
  1. **Root Scope** - статическая переменная `Scope::root_scope_` создается один раз через `InitRootScope()` с проверкой на nullptr. Используется как корневой контейнер зависимостей.
  2. **MongoDB Client** - создается один раз в `main.cpp` и регистрируется в IoC контейнере как singleton через замыкание лямбды, которая захватывает shared_ptr. Все запросы возвращают тот же самый экземпляр.
  3. **Thread-local Current Scope** - `Scope::current_scope_` использует `thread_local` для хранения уникального экземпляра на каждый поток.
- **Почему это этот паттерн**:
  - Гарантируется существование только одного экземпляра root scope и MongoDB клиента
  - Глобальная точка доступа через `Scope::GetRoot()` и `IoC::Resolve<>("MongoClient")`
  - Lazy initialization - объекты создаются при первом обращении
  - Thread-safety обеспечивается через mutex (для root_scope_) и thread_local (для current_scope_)
  - Используется IoC контейнер для управления временем жизни singleton'ов
  - Захват shared_ptr в лямбде гарантирует, что один и тот же экземпляр возвращается при каждом resolve

**Пример кода**:
```cpp
// include/ioc/scope.hpp:79-94
class Scope {
private:
    // Thread-local singleton для текущего scope
    static thread_local std::shared_ptr<IScopeDict> current_scope_;
    // Глобальный singleton для root scope
    static std::shared_ptr<IScopeDict> root_scope_;

public:
    static void InitRootScope();
    static std::shared_ptr<IScopeDict> GetCurrent();
    static std::shared_ptr<IScopeDict> GetRoot();  // Глобальная точка доступа
};

// src/ioc/scope.cpp:6-13
// Инициализация singleton
thread_local std::shared_ptr<IScopeDict> Scope::current_scope_ = nullptr;
std::shared_ptr<IScopeDict> Scope::root_scope_ = nullptr;

void Scope::InitRootScope() {
    if (!root_scope_) {  // Проверка на существование
        root_scope_ = std::make_shared<ConcurrentScopeDict>();
    }
}

// src/ioc/init_command.cpp:10-22
void InitCommand::Execute() {
    if (initialized_) {
        return;  // Уже инициализирован
    }

    std::lock_guard<std::mutex> lock(init_mutex_);  // Thread-safety
    if (initialized_) {
        return;  // Double-check
    }

    Scope::InitRootScope();  // Создание singleton
    // ...
    initialized_ = true;
}

// src/main.cpp:70-83
// MongoDB Client как Singleton через IoC
auto mongo_client = std::make_shared<mongocxx::client>(
    mongocxx::uri(connection_uri)
);

ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
    "IoC.Register",
    ioc::Args{
        std::string("MongoClient"),
        // Захват mongo_client в лямбде - все вызовы вернут тот же экземпляр
        ioc::DependencyFactory([mongo_client](const ioc::Args&) -> std::any {
            return mongo_client;  // Всегда возвращается один и тот же объект
        })
    }
)->Execute();

// Использование - всегда возвращается один и тот же экземпляр
auto client1 = ioc::IoC::Resolve<std::shared_ptr<mongocxx::client>>("MongoClient");
auto client2 = ioc::IoC::Resolve<std::shared_ptr<mongocxx::client>>("MongoClient");
// client1 и client2 указывают на один и тот же объект
```
