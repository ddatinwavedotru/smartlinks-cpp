#include "middleware/unprocessable_content_middleware.hpp"

namespace smartlinks::middleware {

void UnprocessableContentMiddleware::Invoke(
    const drogon::HttpRequestPtr& request,
    drogon::HttpResponsePtr& response,
    RequestDelegate next
) {
    // Проверить, заморожена ли SmartLink
    if (freeze_service_->ShouldSmartLinkBeFreezed()) {
        // SmartLink заморожена → 422 Unprocessable Entity
        response->setStatusCode(drogon::k422UnprocessableEntity);
        // НЕ вызываем next - прерываем цепочку
    } else {
        // SmartLink НЕ заморожена → продолжаем цепочку
        next(request, response);
    }
}

} // namespace smartlinks::middleware
