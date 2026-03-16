#pragma once

#include <string>

namespace smartlinks::domain {

/**
 * @brief Интерфейс SmartLink - представляет умную ссылку
 *
 * Аналог ISmartLink из C# версии.
 * Предоставляет доступ к значению (slug) SmartLink из HTTP запроса.
 */
class ISmartLink {
public:
    virtual ~ISmartLink() = default;

    /**
     * @brief Получить значение SmartLink (slug из URL path)
     * @return Значение slug (например, "/my-link")
     */
    virtual std::string GetValue() const = 0;
};

} // namespace smartlinks::domain
