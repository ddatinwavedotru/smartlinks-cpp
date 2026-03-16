#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

namespace smartlinks::middleware {

/**
 * @brief Тип RequestDelegate - функция для продолжения цепочки
 *
 * Аналог RequestDelegate из C# (функция-обёртка для next middleware).
 * Принимает request и response по ссылке (единый объект response через всю цепочку).
 */
using RequestDelegate = std::function<void(
    const drogon::HttpRequestPtr& request,
    drogon::HttpResponsePtr& response
)>;

/**
 * @brief Интерфейс IMiddleware
 *
 * Аналог IMiddleware из C# версии.
 * Middleware реализуют паттерн Chain of Responsibility.
 *
 * КРИТИЧНО:
 * - Зависимости передаются через конструктор (DI)
 * - НЕ вызывать IoC.Resolve внутри Invoke
 * - Response создаётся ОДИН РАЗ и передаётся по ссылке
 * - Middleware как Transient - создаются заново на каждый запрос
 */
class IMiddleware {
public:
    virtual ~IMiddleware() = default;

    /**
     * @brief Обработать HTTP запрос
     * @param request HTTP запрос (const ref)
     * @param response HTTP ответ (ref - единый объект для всей цепочки!)
     * @param next Функция для вызова следующего middleware в цепочке
     *
     * Логика:
     * - Выполнить проверки/обработку
     * - Если условия выполнены:
     *   - Установить статус в response
     *   - НЕ вызывать next (прервать цепочку)
     * - Иначе:
     *   - Вызвать next(request, response) для продолжения
     */
    virtual void Invoke(
        const drogon::HttpRequestPtr& request,
        drogon::HttpResponsePtr& response,
        RequestDelegate next
    ) = 0;
};

} // namespace smartlinks::middleware
