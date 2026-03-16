#pragma once

#include "domain/i_mongodb_repository.hpp"
#include "domain/i_smart_link.hpp"
#include <mongocxx/collection.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <memory>
#include <optional>

namespace smartlinks::impl {

/**
 * @brief Реализация IMongoDbRepository
 *
 * Низкоуровневый доступ к MongoDB коллекции.
 * Ищет документ по полю "slug" (значение из ISmartLink).
 *
 * Аналог ImplMongoDbRepository из C# версии.
 */
class MongoDbRepository : public domain::IMongoDbRepository {
private:
    mongocxx::collection collection_;
    std::shared_ptr<domain::ISmartLink> smart_link_;

public:
    /**
     * @brief Конструктор с DI
     * @param collection MongoDB коллекция (DI через конструктор)
     * @param smart_link ISmartLink для получения slug (DI через конструктор)
     */
    MongoDbRepository(
        mongocxx::collection collection,
        std::shared_ptr<domain::ISmartLink> smart_link
    ) : collection_(std::move(collection)),
        smart_link_(std::move(smart_link)) {}

    /**
     * @brief Прочитать документ из MongoDB по slug
     * @return BSON документ или std::nullopt если не найдено
     *
     * Фильтр: { "slug": smart_link->GetValue() }
     */
    std::optional<bsoncxx::document::value> Read() override {
        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;

        // Получить slug из ISmartLink
        auto slug = smart_link_->GetValue();

        // Построить фильтр для поиска
        auto filter = document{}
            << "slug" << slug
            << finalize;

        // Выполнить поиск
        auto result = collection_.find_one(filter.view());

        if (result) {
            return bsoncxx::document::value(result->view());
        }

        return std::nullopt;
    }
};

} // namespace smartlinks::impl
