#include "context_2_language_accessible.hpp"
#include <drogon/HttpRequest.h>
#include <algorithm>
#include <cctype>

namespace smartlinks::dsl::plugins::language {

Context2LanguageAccessible::Context2LanguageAccessible(std::shared_ptr<dsl::IContext> context)
    : context_(std::move(context)) {}

bool Context2LanguageAccessible::language_matches(const std::string& pattern) const {
    // Переиспользуем логику из Context::LanguageMatches (context.cpp:42-60)

    auto accept_language = GetAcceptLanguage();

    // 1. Если pattern == "any" → всегда true
    if (pattern == "any") {
        return true;
    }

    // 2. Если Accept-Language содержит "*" → true
    if (accept_language.find('*') != std::string::npos) {
        return true;
    }

    // 3. Case-insensitive поиск подстроки
    std::string lower_accept = ToLower(accept_language);
    std::string lower_pattern = ToLower(pattern);

    return lower_accept.find(lower_pattern) != std::string::npos;
}

std::string Context2LanguageAccessible::GetAcceptLanguage() const {
    // Извлечь request из контекста
    auto request_any = context_->get("request");
    if (!request_any.has_value()) {
        return "";
    }

    auto request = std::any_cast<drogon::HttpRequestPtr>(request_any);
    return request->getHeader("Accept-Language");
}

// Вспомогательная функция для приведения к нижнему регистру
// Переиспользуем логику из context.cpp:9-14
std::string Context2LanguageAccessible::ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace smartlinks::dsl::plugins::language
