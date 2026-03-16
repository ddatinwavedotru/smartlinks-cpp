#include "context_2_authorized_accessible.hpp"
#include "domain/i_jwt_validator.hpp"
#include <drogon/HttpRequest.h>

namespace smartlinks::dsl::plugins::authorized {

Context2AuthorizedAccessible::Context2AuthorizedAccessible(std::shared_ptr<dsl::IContext> context)
    : context_(std::move(context)) {}

bool Context2AuthorizedAccessible::is_authorized() const {
    // Переиспользуем логику из Context::FromHttpRequest (context.cpp:27-37)

    // Извлечь request и jwt_validator из контекста
    auto request_any = context_->get("request");
    auto jwt_validator_any = context_->get("jwt_validator");

    if (!request_any.has_value() || !jwt_validator_any.has_value()) {
        return false;
    }

    auto request = std::any_cast<drogon::HttpRequestPtr>(request_any);
    auto jwt_validator = std::any_cast<std::shared_ptr<domain::IJwtValidator>>(jwt_validator_any);

    // Проверка JWT авторизации
    auto auth_header = request->getHeader("Authorization");
    if (auth_header.empty()) {
        return false;
    }

    const std::string bearer_prefix = "Bearer ";
    if (auth_header.find(bearer_prefix) != 0) {
        return false;
    }

    std::string token = auth_header.substr(bearer_prefix.length());
    return jwt_validator->Validate(token);
}

} // namespace smartlinks::dsl::plugins::authorized
