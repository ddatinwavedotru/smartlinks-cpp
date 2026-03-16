#pragma once

#include "i_statable_smart_link.hpp"
#include <optional>
#include <memory>

namespace smartlinks::domain {

/**
 * @brief Интерфейс репозитория для SmartLink с состоянием
 *
 * Аналог IStatableSmartLinkRepository из C# версии.
 * Читает SmartLink из MongoDB и предоставляет доступ к состоянию.
 */
class IStatableSmartLinkRepository {
public:
    virtual ~IStatableSmartLinkRepository() = default;

    /**
     * @brief Прочитать SmartLink с состоянием из MongoDB
     * @return IStatableSmartLink объект или std::nullopt если не найдено
     *
     * Логика:
     * 1. Получить BSON документ из IMongoDbRepository
     * 2. Извлечь поле "state"
     * 3. Создать IStatableSmartLink с этим состоянием
     */
    virtual std::optional<std::shared_ptr<IStatableSmartLink>> Read() = 0;
};

} // namespace smartlinks::domain
