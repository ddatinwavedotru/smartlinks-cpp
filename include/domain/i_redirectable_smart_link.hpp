#pragma once

#include <string>
#include <chrono>

namespace smartlinks::domain {

/**
 * @brief Интерфейс для проверки условий редиректа
 *
 * Аналог IRedirectableSmartLink из C# версии.
 * Проверяет, подходит ли HTTP запрос под условия правила редиректа
 * (язык и временной интервал).
 */
class IRedirectableSmartLink {
public:
    virtual ~IRedirectableSmartLink() = default;

    /**
     * @brief Проверить, соответствует ли Accept-Language заголовок указанному языку
     * @param language Язык для проверки (например, "ru-RU", "en-US", "any")
     * @return true если язык подходит, false иначе
     *
     * Логика:
     * - "any" всегда возвращает true
     * - "*" в Accept-Language возвращает true
     * - Иначе case-insensitive поиск подстроки
     */
    virtual bool IsLanguageAccepted(const std::string& language) const = 0;

    /**
     * @brief Проверить, находится ли текущее время в указанном интервале
     * @param start Время начала интервала
     * @param end Время окончания интервала
     * @return true если текущее время в интервале (start < now < end)
     */
    virtual bool IsInTime(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end
    ) const = 0;

    /**
     * @brief Проверить, есть ли валидный JWT токен авторизации
     * @return true если Authorization header содержит валидный JWT токен
     *
     * Логика:
     * - Извлечь "Authorization: Bearer <token>" header
     * - Валидировать токен через IJwtValidator
     * - Вернуть true если токен валиден
     */
    virtual bool IsAuthorized() const = 0;
};

} // namespace smartlinks::domain
