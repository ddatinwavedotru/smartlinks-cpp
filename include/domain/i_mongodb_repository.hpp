#pragma once

#include <optional>
#include <bsoncxx/document/value.hpp>

namespace smartlinks::domain {

/**
 * @brief Интерфейс репозитория MongoDB
 *
 * Аналог IMongoDbRepository из C# версии.
 * Предоставляет низкоуровневый доступ к MongoDB коллекции.
 */
class IMongoDbRepository {
public:
    virtual ~IMongoDbRepository() = default;

    /**
     * @brief Прочитать документ SmartLink из MongoDB по slug
     * @return BSON документ или std::nullopt если не найдено
     *
     * Ищет документ по полю "slug" в коллекции "links".
     */
    virtual std::optional<bsoncxx::document::value> Read() = 0;
};

} // namespace smartlinks::domain
