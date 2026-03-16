#pragma once

#include <string>

namespace smartlinks::domain {

/**
 * @brief Интерфейс для SmartLink с состоянием
 *
 * Аналог IStatableSmartLink из C# версии.
 * Предоставляет доступ к состоянию SmartLink (active, deleted, freezed).
 */
class IStatableSmartLink {
public:
    virtual ~IStatableSmartLink() = default;

    /**
     * @brief Получить состояние SmartLink
     * @return Состояние ("active", "deleted", "freezed")
     */
    virtual std::string GetState() const = 0;
};

} // namespace smartlinks::domain
