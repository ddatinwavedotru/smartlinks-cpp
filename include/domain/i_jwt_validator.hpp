#pragma once

#include <string>

namespace smartlinks::domain {

/**
 * @brief Интерфейс для валидации JWT токенов
 *
 * Использует jwt-cpp для локальной проверки подписи через JWKS.
 */
class IJwtValidator {
public:
    virtual ~IJwtValidator() = default;

    /**
     * @brief Проверить валидность JWT токена
     * @param token JWT токен (без "Bearer " префикса)
     * @return true если токен валиден (подпись верна, не истёк, правильный issuer/audience)
     */
    virtual bool Validate(const std::string& token) const = 0;
};

} // namespace smartlinks::domain
