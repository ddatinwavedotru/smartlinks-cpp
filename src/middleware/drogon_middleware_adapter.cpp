#include "middleware/drogon_middleware_adapter.hpp"

namespace smartlinks::middleware {

void DrogonMiddlewareAdapter::invoke(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& nextCb,
    std::function<void(const drogon::HttpResponsePtr&)>&& mcb
) {
    // ========== REQUEST LOGGING ==========
    LOG_INFO << "========== INCOMING REQUEST ==========";
    LOG_INFO << "Method: " << drogon::to_string(req->method());
    LOG_INFO << "Path: " << req->path();
    LOG_INFO << "Headers:";
    for (const auto& header : req->headers()) {
        LOG_INFO << "  " << header.first << ": " << header.second;
    }
    LOG_INFO << "=====================================";

    try {
        LOG_INFO << "[DrogonMiddlewareAdapter] Step 1: Creating request scope";
        // 1. Создать request-scoped IoC контейнер
        auto request_scope = ioc::IoC::Resolve<std::shared_ptr<ioc::IScopeDict>>(
            "IoC.Scope.Create"
        );
        LOG_INFO << "[DrogonMiddlewareAdapter] Step 1: Request scope created";

        LOG_INFO << "[DrogonMiddlewareAdapter] Step 2: Setting current scope";
        // 2. Установить как текущий (thread_local)
        ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
            "IoC.Scope.Current.Set",
            ioc::Args{request_scope}
        )->Execute();
        LOG_INFO << "[DrogonMiddlewareAdapter] Step 2: Current scope set";

        LOG_INFO << "[DrogonMiddlewareAdapter] Step 3: Registering HTTP dependencies";
        // 3. Регистрировать HTTP-специфичные зависимости через IoC.Register
        RegisterHttpDependencies(req);
        LOG_INFO << "[DrogonMiddlewareAdapter] Step 3: HTTP dependencies registered";
    } catch (const std::bad_any_cast& e) {
        LOG_ERROR << "[DrogonMiddlewareAdapter] bad_any_cast in initialization: " << e.what();
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k500InternalServerError);
        mcb(response);
        return;
    } catch (const std::exception& e) {
        LOG_ERROR << "[DrogonMiddlewareAdapter] Exception in initialization: " << e.what();
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k500InternalServerError);
        mcb(response);
        return;
    }

    try {
        LOG_INFO << "[DrogonMiddlewareAdapter] Step 4: Creating response object";
        // 4. Создать Response объект ОДИН РАЗ (передаётся через всю цепочку)
        auto response = drogon::HttpResponse::newHttpResponse();

        LOG_INFO << "[DrogonMiddlewareAdapter] Step 5: Getting middleware pipeline";
        // 5. Получить набор middleware из IoC через команду
        std::vector<std::shared_ptr<IMiddleware>> middlewares;
        ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
            "GetMiddlewarePipeline",
            ioc::Args{std::ref(middlewares)}
        )->Execute();
        LOG_INFO << "[DrogonMiddlewareAdapter] Step 5: Got " << middlewares.size() << " middlewares";

        LOG_INFO << "[DrogonMiddlewareAdapter] Step 6: Getting exception handler";
        // 6. Получить ExceptionHandler из IoC
        auto exception_handler = ioc::IoC::Resolve<std::function<void(
            std::shared_ptr<IMiddleware>,
            std::exception_ptr
        )>>("ExceptionHandler");
        LOG_INFO << "[DrogonMiddlewareAdapter] Step 6: Exception handler retrieved";

        LOG_INFO << "[DrogonMiddlewareAdapter] Step 7: Building pipeline";
        // 7. Построить Chain of Responsibility
        auto pipeline = BuildPipeline(middlewares);
        LOG_INFO << "[DrogonMiddlewareAdapter] Step 7: Pipeline built";

        LOG_INFO << "[DrogonMiddlewareAdapter] Step 8: Running pipeline";
        // 8. Запустить pipeline с обработкой исключений
        try {
            pipeline(req, response);
            LOG_INFO << "[DrogonMiddlewareAdapter] Step 8: Pipeline executed";
        } catch (...) {
            LOG_ERROR << "[DrogonMiddlewareAdapter] Exception in pipeline execution";
            // Обработать исключение через ExceptionHandler
            exception_handler(nullptr, std::current_exception());
            // Установить 500 Internal Server Error
            response->setStatusCode(drogon::k500InternalServerError);
        }

        // 9. Вернуть response в Drogon
        LOG_INFO << "========== OUTGOING RESPONSE ==========";
        LOG_INFO << "Status: " << response->statusCode();
        LOG_INFO << "Headers:";
        for (const auto& header : response->headers()) {
            LOG_INFO << "  " << header.first << ": " << header.second;
        }
        LOG_INFO << "======================================";
        mcb(response);

        LOG_INFO << "[DrogonMiddlewareAdapter] Step 10: Clearing scope";
        // 10. Очистить scope после обработки
        ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
            "IoC.Scope.Current.Clear"
        )->Execute();
        LOG_INFO << "[DrogonMiddlewareAdapter] Step 10: Scope cleared";

    } catch (const std::bad_any_cast& e) {
        LOG_ERROR << "[DrogonMiddlewareAdapter] bad_any_cast in middleware execution: " << e.what();
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k500InternalServerError);
        mcb(response);
    } catch (const std::exception& e) {
        LOG_ERROR << "[DrogonMiddlewareAdapter] Exception in middleware execution: " << e.what();
        auto response = drogon::HttpResponse::newHttpResponse();
        response->setStatusCode(drogon::k500InternalServerError);
        mcb(response);
    }
}

void DrogonMiddlewareAdapter::RegisterHttpDependencies(
    const drogon::HttpRequestPtr& req
) {
    // Регистрировать HttpRequest
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("HttpRequest"),
            ioc::DependencyFactory([req](const ioc::Args&) -> std::any {
                return req;
            })
        }
    )->Execute();

    // Регистрировать ISmartLink (Transient - создаём заново)
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("ISmartLink"),
            ioc::DependencyFactory([req](const ioc::Args&) -> std::any {
                // ИСПРАВЛЕНО: возвращаем указатель на интерфейс
                return std::static_pointer_cast<domain::ISmartLink>(
                    std::make_shared<impl::SmartLink>(req)
                );
            })
        }
    )->Execute();

    // Регистрировать ISupportedHttpRequest (Transient)
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("ISupportedHttpRequest"),
            ioc::DependencyFactory([req](const ioc::Args&) -> std::any {
                // ИСПРАВЛЕНО: возвращаем указатель на интерфейс
                return std::static_pointer_cast<domain::ISupportedHttpRequest>(
                    std::make_shared<impl::SupportedHttpRequest>(req)
                );
            })
        }
    )->Execute();

    // Регистрировать IRedirectableSmartLink (Transient)
    ioc::IoC::Resolve<std::shared_ptr<ioc::ICommand>>(
        "IoC.Register.Transient",
        ioc::Args{
            std::string("IRedirectableSmartLink"),
            ioc::DependencyFactory([req](const ioc::Args&) -> std::any {
                // Resolve IJwtValidator from root scope
                auto jwt_validator = ioc::IoC::Resolve<
                    std::shared_ptr<domain::IJwtValidator>
                >("IJwtValidator");

                // ИСПРАВЛЕНО: возвращаем указатель на интерфейс с jwt_validator
                return std::static_pointer_cast<domain::IRedirectableSmartLink>(
                    std::make_shared<impl::RedirectableSmartLink>(req, jwt_validator)
                );
            })
        }
    )->Execute();
}

RequestDelegate DrogonMiddlewareAdapter::BuildPipeline(
    const std::vector<std::shared_ptr<IMiddleware>>& middlewares
) {
    // Рекурсивно строим цепочку с конца
    RequestDelegate pipeline = [](
        const drogon::HttpRequestPtr& req,
        drogon::HttpResponsePtr& resp
    ) {
        // Конец цепочки - если никто не установил статус, ставим 429
        if (resp->statusCode() == drogon::k200OK) {
            resp->setStatusCode(static_cast<drogon::HttpStatusCode>(429));
        }
    };

    // Оборачиваем каждый middleware (с конца в начало)
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

} // namespace smartlinks::middleware
