#pragma once

#include "middleware/i_middleware.hpp"
#include "ioc/ioc.hpp"
#include "ioc/command.hpp"
#include "ioc/scope.hpp"
#include "impl/smart_link.hpp"
#include "impl/redirectable_smart_link.hpp"
#include "impl/supported_http_request.hpp"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <vector>
#include <memory>
#include <functional>

namespace smartlinks::middleware {

/**
 * @brief Адаптер для интеграции нашего Middleware pipeline с Drogon
 *
 * Этот класс - точка входа для HTTP запросов в Drogon.
 * Выполняет следующие задачи:
 * 1. Создаёт request-scoped IoC контейнер
 * 2. Регистрирует HTTP-специфичные зависимости (через IoC.Register)
 * 3. Получает набор middleware из IoC
 * 4. Строит Chain of Responsibility
 * 5. Выполняет pipeline с обработкой исключений
 * 6. Очищает scope после запроса
 *
 * Аналог middleware pipeline из C# Program.cs + ASP.NET middleware infrastructure.
 */
class DrogonMiddlewareAdapter {
public:
    DrogonMiddlewareAdapter() = default;

    /**
     * @brief Обработать HTTP запрос (точка входа от Drogon)
     * @param req HTTP запрос от Drogon
     * @param nextCb Callback для следующего middleware Drogon (не используется, может быть nullptr)
     * @param mcb Callback для возврата response в Drogon
     */
    void invoke(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& nextCb,
        std::function<void(const drogon::HttpResponsePtr&)>&& mcb
    );

private:
    /**
     * @brief Зарегистрировать HTTP-специфичные зависимости
     *
     * Использует паттерн IoC.Register (как в main.cpp):
     * - HttpRequest
     * - ISmartLink
     * - ISupportedHttpRequest
     * - IRedirectableSmartLink
     *
     * Все регистрируются как Transient (создаются заново на каждый resolve).
     */
    void RegisterHttpDependencies(const drogon::HttpRequestPtr& req);

    /**
     * @brief Построить Chain of Responsibility из middleware
     *
     * Рекурсивно оборачивает каждый middleware в lambda,
     * формируя цепочку вызовов (с конца в начало).
     *
     * Конец цепочки: если никто не установил статус, ставим 429.
     *
     * @param middlewares Вектор middleware (в порядке выполнения)
     * @return RequestDelegate - корневая функция pipeline
     */
    RequestDelegate BuildPipeline(
        const std::vector<std::shared_ptr<IMiddleware>>& middlewares
    );
};

} // namespace smartlinks::middleware
