#include "middleware/method_not_allowed_middleware.hpp"

namespace smartlinks::middleware {

void MethodNotAllowedMiddleware::Invoke(
    const drogon::HttpRequestPtr& request,
    drogon::HttpResponsePtr& response,
    RequestDelegate next
) {
    // Проверить, поддерживается ли HTTP метод
    if (!supported_request_->MethodIsSupported()) {
        // Метод НЕ поддерживается → 405 Method Not Allowed
        response->setStatusCode(drogon::k405MethodNotAllowed);
        // НЕ вызываем next - прерываем цепочку
    } else {
        // Метод поддерживается → продолжаем цепочку
        next(request, response);
    }
}

} // namespace smartlinks::middleware
