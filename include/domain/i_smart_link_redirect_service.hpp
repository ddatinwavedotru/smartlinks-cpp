#pragma once

#include <optional>
#include <string>

namespace smartlinks::domain {

/**
 * @brief Интерфейс сервиса редиректа SmartLink
 *
 * Аналог ISmartLinkRedirectService из C# версии.
 * Выполняет основную бизнес-логику поиска подходящего правила редиректа.
 */
class ISmartLinkRedirectService {
public:
    virtual ~ISmartLinkRedirectService() = default;

    /**
     * @brief Найти подходящее правило редиректа и вернуть URL
     * @return URL редиректа или std::nullopt если подходящее правило не найдено
     *
     * Логика:
     * 1. Получить все правила из репозитория
     * 2. Найти первое правило, которое подходит по языку И времени
     * 3. Вернуть redirect URL этого правила
     * 4. Если ничего не найдено, вернуть std::nullopt
     */
    virtual std::optional<std::string> Evaluate() = 0;
};

} // namespace smartlinks::domain
