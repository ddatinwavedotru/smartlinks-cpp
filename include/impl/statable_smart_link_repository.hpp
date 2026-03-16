#pragma once

#include "domain/i_statable_smart_link_repository.hpp"
#include "domain/i_mongodb_repository.hpp"
#include "impl/statable_smart_link.hpp"
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/types.hpp>
#include <memory>
#include <optional>

namespace smartlinks::impl {

/**
 * @brief Реализация IStatableSmartLinkRepository
 *
 * Читает документ из MongoDB и извлекает поле "state".
 * Аналог ImplStatableSmartLinkRepository из C# версии.
 */
class StatableSmartLinkRepository : public domain::IStatableSmartLinkRepository {
private:
    std::shared_ptr<domain::IMongoDbRepository> repository_;

public:
    /**
     * @brief Конструктор с DI
     * @param repository MongoDB репозиторий (DI через конструктор)
     */
    explicit StatableSmartLinkRepository(
        std::shared_ptr<domain::IMongoDbRepository> repository
    ) : repository_(std::move(repository)) {}

    /**
     * @brief Прочитать SmartLink с состоянием
     * @return IStatableSmartLink или std::nullopt если документ не найден
     *
     * Логика:
     * 1. Прочитать BSON документ через repository
     * 2. Извлечь поле "state"
     * 3. Создать StatableSmartLink с этим состоянием
     */
    std::optional<std::shared_ptr<domain::IStatableSmartLink>> Read() override {
        // Прочитать документ
        auto doc_opt = repository_->Read();

        if (!doc_opt.has_value()) {
            return std::nullopt;
        }

        auto doc = doc_opt->view();

        // Извлечь поле "state"
        auto state_element = doc["state"];
        if (!state_element || state_element.type() != bsoncxx::type::k_string) {
            return std::nullopt;
        }

        std::string state(state_element.get_string().value);

        // Создать IStatableSmartLink
        return std::make_shared<StatableSmartLink>(state);
    }
};

} // namespace smartlinks::impl
