#include "middleware/not_found_middleware.hpp"

namespace smartlinks::middleware {

void NotFoundMiddleware::Invoke(
    const drogon::HttpRequestPtr& request,
    drogon::HttpResponsePtr& response,
    RequestDelegate next
) {
    // Прочитать SmartLink из репозитория
    auto smart_link_opt = repository_->Read();

    // Проверить, существует ли SmartLink и не удалена ли она
    if (!smart_link_opt.has_value() ||
        smart_link_opt.value()->GetState() == "deleted") {
        // SmartLink не найдена или удалена → 404 Not Found
        response->setStatusCode(drogon::k404NotFound);
        // НЕ вызываем next - прерываем цепочку
    } else {
        // SmartLink существует → продолжаем цепочку
        next(request, response);
    }
}

} // namespace smartlinks::middleware
