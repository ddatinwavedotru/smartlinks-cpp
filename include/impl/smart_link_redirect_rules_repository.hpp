#pragma once

#include "domain/i_smart_link_redirect_rules_repository.hpp"
#include "domain/i_mongodb_repository.hpp"
#include "impl/redirect_rule.hpp"
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/array/view.hpp>
#include <bsoncxx/types.hpp>
#include <memory>
#include <optional>
#include <vector>
#include <chrono>

namespace smartlinks::impl {

/**
 * @brief Реализация ISmartLinkRedirectRulesRepository
 *
 * Читает массив "rules" из MongoDB документа и преобразует в вектор IRedirectRule.
 * Аналог ImplSmartLinkRedirectRulesRepository из C# версии.
 */
class SmartLinkRedirectRulesRepository : public domain::ISmartLinkRedirectRulesRepository {
private:
    std::shared_ptr<domain::IMongoDbRepository> repository_;

    /**
     * @brief Преобразовать BSON timestamp в time_point
     * @param bson_date BSON дата (b_date)
     * @return std::chrono::system_clock::time_point
     */
    static std::chrono::system_clock::time_point BsonDateToTimePoint(
        const bsoncxx::types::b_date& bson_date
    ) {
        auto millis = bson_date.value;
        return std::chrono::system_clock::time_point(
            std::chrono::milliseconds(millis)
        );
    }

public:
    /**
     * @brief Конструктор с DI
     * @param repository MongoDB репозиторий (DI через конструктор)
     */
    explicit SmartLinkRedirectRulesRepository(
        std::shared_ptr<domain::IMongoDbRepository> repository
    ) : repository_(std::move(repository)) {}

    /**
     * @brief Прочитать массив правил редиректа
     * @return Вектор IRedirectRule или std::nullopt если документ не найден
     *
     * Логика:
     * 1. Прочитать BSON документ
     * 2. Извлечь массив "rules"
     * 3. Для каждого элемента массива:
     *    - Извлечь поля: language, redirect, start, end
     *    - Создать RedirectRule объект
     * 4. Вернуть вектор правил
     */
    std::optional<std::vector<std::shared_ptr<domain::IRedirectRule>>> Read() override {
        // Прочитать документ
        auto doc_opt = repository_->Read();

        if (!doc_opt.has_value()) {
            return std::nullopt;
        }

        auto doc = doc_opt->view();

        // Извлечь массив "rules"
        auto rules_element = doc["rules"];
        if (!rules_element || rules_element.type() != bsoncxx::type::k_array) {
            return std::nullopt;
        }

        bsoncxx::array::view rules_array = rules_element.get_array().value;

        std::vector<std::shared_ptr<domain::IRedirectRule>> result;

        // Обработать каждое правило в массиве
        for (const auto& rule_element : rules_array) {
            if (rule_element.type() != bsoncxx::type::k_document) {
                continue;
            }

            bsoncxx::document::view rule_doc = rule_element.get_document().value;

            // Извлечь поля
            auto language_elem = rule_doc["language"];
            auto redirect_elem = rule_doc["redirect"];
            auto start_elem = rule_doc["start"];
            auto end_elem = rule_doc["end"];

            // Проверить наличие всех полей
            if (!language_elem || language_elem.type() != bsoncxx::type::k_string ||
                !redirect_elem || redirect_elem.type() != bsoncxx::type::k_string ||
                !start_elem || start_elem.type() != bsoncxx::type::k_date ||
                !end_elem || end_elem.type() != bsoncxx::type::k_date) {
                continue;
            }

            // Извлечь значения
            std::string language(language_elem.get_string().value);
            std::string redirect(redirect_elem.get_string().value);
            auto start = BsonDateToTimePoint(start_elem.get_date());
            auto end = BsonDateToTimePoint(end_elem.get_date());

            // Извлечь поле "authorized" (опционально, по умолчанию false)
            bool authorized = false;  // Значение по умолчанию для обратной совместимости
            auto authorized_elem = rule_doc["authorized"];
            if (authorized_elem && authorized_elem.type() == bsoncxx::type::k_bool) {
                authorized = authorized_elem.get_bool().value;
            }

            // Создать RedirectRule
            result.push_back(std::make_shared<RedirectRule>(
                language, redirect, start, end, authorized
            ));
        }

        return result;
    }
};

} // namespace smartlinks::impl
