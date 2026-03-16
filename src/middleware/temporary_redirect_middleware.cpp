#include "middleware/temporary_redirect_middleware.hpp"

namespace smartlinks::middleware {

void TemporaryRedirectMiddleware::Invoke(
    const drogon::HttpRequestPtr& request,
    drogon::HttpResponsePtr& response,
    RequestDelegate next
) {
    // Найти подходящее правило редиректа
    auto redirect_opt = smart_link_redirect_service_->Evaluate();

    if (redirect_opt.has_value()) {
        // Правило найдено → 307 Temporary Redirect
        response->setStatusCode(drogon::k307TemporaryRedirect);
        response->addHeader("Location", redirect_opt.value());
        // НЕ вызываем next - прерываем цепочку
    } else {
        // Правило НЕ найдено → продолжаем цепочку
        // (конец цепочки установит 429)
        next(request, response);
    }
}

} // namespace smartlinks::middleware
