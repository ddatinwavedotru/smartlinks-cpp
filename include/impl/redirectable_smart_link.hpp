#pragma once

#include "domain/i_redirectable_smart_link.hpp"
#include "domain/i_jwt_validator.hpp"
#include <drogon/HttpRequest.h>
#include <memory>
#include <algorithm>
#include <cctype>

namespace smartlinks::impl {

/**
 * @brief Реализация IRedirectableSmartLink
 *
 * Проверяет соответствие HTTP запроса условиям редиректа:
 * - Язык (Accept-Language header)
 * - Временной интервал
 *
 * Аналог ImplRedirectableSmartLink из C# версии.
 */
class RedirectableSmartLink : public domain::IRedirectableSmartLink {
private:
    drogon::HttpRequestPtr request_;
    std::shared_ptr<domain::IJwtValidator> jwt_validator_;

    /**
     * @brief Вспомогательная функция для приведения строки к нижнему регистру
     */
    static std::string ToLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }

public:
    /**
     * @brief Конструктор с DI
     * @param request HTTP запрос (DI через конструктор)
     * @param jwt_validator JWT валидатор (DI через конструктор)
     */
    explicit RedirectableSmartLink(
        const drogon::HttpRequestPtr& request,
        std::shared_ptr<domain::IJwtValidator> jwt_validator
    ) : request_(request), jwt_validator_(std::move(jwt_validator)) {}

    /**
     * @brief Проверить соответствие языка
     *
     * Логика (как в C# версии):
     * 1. Если language == "any" → всегда true
     * 2. Если Accept-Language содержит "*" → true
     * 3. Иначе case-insensitive поиск подстроки language в Accept-Language
     */
    bool IsLanguageAccepted(const std::string& language) const override {
        // Правило с language="any" подходит всегда
        if (language == "any") {
            return true;
        }

        // Получить Accept-Language header
        auto accept_language = request_->getHeader("Accept-Language");

        // Если Accept-Language содержит "*", подходит любой язык
        if (accept_language.find('*') != std::string::npos) {
            return true;
        }

        // Case-insensitive поиск
        std::string lower_accept = ToLower(accept_language);
        std::string lower_lang = ToLower(language);

        return lower_accept.find(lower_lang) != std::string::npos;
    }

    /**
     * @brief Проверить, находится ли текущее время в интервале
     *
     * Логика: start < now < end
     */
    bool IsInTime(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end
    ) const override {
        auto now = std::chrono::system_clock::now();
        return start < now && now < end;
    }

    /**
     * @brief Проверить наличие валидного JWT токена
     *
     * Логика (аналогично IsLanguageAccepted):
     * 1. Извлечь Authorization header
     * 2. Проверить формат "Bearer <token>"
     * 3. Извлечь токен
     * 4. Валидировать через jwt_validator_
     */
    bool IsAuthorized() const override {
        // Получить Authorization header
        auto auth_header = request_->getHeader("Authorization");

        // Проверить наличие
        if (auth_header.empty()) {
            return false;
        }

        // Проверить формат "Bearer <token>"
        const std::string bearer_prefix = "Bearer ";
        if (auth_header.find(bearer_prefix) != 0) {
            return false;
        }

        // Извлечь токен (после "Bearer ")
        std::string token = auth_header.substr(bearer_prefix.length());

        // Валидировать через jwt_validator_
        return jwt_validator_->Validate(token);
    }
};

} // namespace smartlinks::impl
