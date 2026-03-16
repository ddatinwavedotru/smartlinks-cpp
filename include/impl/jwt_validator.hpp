#pragma once

#include "domain/i_jwt_validator.hpp"
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/kazuho-picojson/traits.h>
#include <string>
#include <memory>

namespace smartlinks::impl {

/**
 * @brief Реализация IJwtValidator с использованием jwt-cpp
 *
 * Загружает JWKS от JWT-микросервиса через HTTP при инициализации.
 */
class JwtValidator : public domain::IJwtValidator {
private:
    std::string jwks_uri_;       // URL JWT-сервиса (например, "http://jwt-service:3000")
    std::string expected_issuer_;
    std::string expected_audience_;

    // JWKS verifier (кэшируется после первой загрузки)
    mutable jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson> verifier_;
    mutable bool verifier_initialized_;
    mutable std::string verifier_kid_;

    /**
     * @brief Загрузить JWKS от микросервиса и инициализировать verifier
     *
     * Вызывается при первой валидации токена.
     * JWKS содержит публичный ключ для проверки подписи.
     */
    void InitializeVerifier(const std::string& token_kid) const;

    /**
     * @brief HTTP-запрос к микросервису для получения JWKS
     * @return JSON строка с JWKS
     */
    std::string FetchJwks() const;

public:
    /**
     * @brief Конструктор с DI
     * @param jwks_uri URL JWT-сервиса (например, "http://jwt-service:3000")
     * @param expected_issuer Ожидаемый issuer (например, "smartlinks-jwt-service")
     * @param expected_audience Ожидаемый audience (например, "smartlinks-api")
     */
    JwtValidator(
        std::string jwks_uri,
        std::string expected_issuer,
        std::string expected_audience
    );

    /**
     * @brief Проверить JWT токен
     *
     * Логика:
     * 1. Инициализировать verifier (если ещё не инициализирован)
     * 2. Декодировать JWT
     * 3. Проверить подпись через JWKS
     * 4. Проверить expiration
     * 5. Проверить issuer и audience
     *
     * @param token JWT токен
     * @return true если все проверки прошли
     */
    bool Validate(const std::string& token) const override;
};

} // namespace smartlinks::impl
