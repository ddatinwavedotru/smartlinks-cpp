#pragma once

#include <string>
#include <chrono>

namespace smartlinks::domain {

/**
 * @brief Интерфейс правила редиректа
 *
 * Аналог IRedirectRule из C# версии.
 * Представляет одно правило редиректа с языком, URL и временным интервалом.
 */
class IRedirectRule {
public:
    virtual ~IRedirectRule() = default;

    /**
     * @brief Получить язык правила (например, "ru-RU", "en-US", "any")
     * @return Код языка
     */
    virtual std::string GetLanguage() const = 0;

    /**
     * @brief Получить URL редиректа
     * @return URL, на который нужно перенаправить
     */
    virtual std::string GetRedirect() const = 0;

    /**
     * @brief Получить время начала действия правила
     * @return Точка времени начала
     */
    virtual std::chrono::system_clock::time_point GetStart() const = 0;

    /**
     * @brief Получить время окончания действия правила
     * @return Точка времени окончания
     */
    virtual std::chrono::system_clock::time_point GetEnd() const = 0;

    /**
     * @brief Получить флаг требования авторизации
     * @return true если правило требует валидный JWT токен
     */
    virtual bool GetAuthorized() const = 0;
};

} // namespace smartlinks::domain
