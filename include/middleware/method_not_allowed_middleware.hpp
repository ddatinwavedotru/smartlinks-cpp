#pragma once

#include "middleware/i_middleware.hpp"
#include "domain/i_supported_http_request.hpp"
#include <memory>

namespace smartlinks::middleware {

/**
 * @brief Middleware для проверки HTTP метода (405 Method Not Allowed)
 *
 * Аналог The_App_Responses_405_Method_Not_Allowed_Middleware из C# версии.
 *
 * Логика:
 * - Проверить, поддерживается ли HTTP метод (GET/HEAD)
 * - Если НЕ поддерживается → 405 Method Not Allowed
 * - Иначе → вызвать next
 */
class MethodNotAllowedMiddleware : public IMiddleware {
private:
    std::shared_ptr<domain::ISupportedHttpRequest> supported_request_;

public:
    /**
     * @brief Конструктор с DI
     * @param supportedHttpRequest ISupportedHttpRequest (DI через конструктор!)
     */
    explicit MethodNotAllowedMiddleware(
        std::shared_ptr<domain::ISupportedHttpRequest> supportedHttpRequest
    ) : supported_request_(std::move(supportedHttpRequest)) {}

    void Invoke(
        const drogon::HttpRequestPtr& request,
        drogon::HttpResponsePtr& response,
        RequestDelegate next
    ) override;
};

} // namespace smartlinks::middleware
