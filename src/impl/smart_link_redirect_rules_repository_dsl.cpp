#include "impl/smart_link_redirect_rules_repository_dsl.hpp"
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/types.hpp>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>

namespace smartlinks::impl {

SmartLinkRedirectRulesRepositoryDsl::SmartLinkRedirectRulesRepositoryDsl(
    std::shared_ptr<domain::IMongoDbRepository> repository,
    std::shared_ptr<dsl::parser::ICombinedParser> parser
) : repository_(std::move(repository))
  , parser_(std::move(parser)) {}

std::optional<dsl::ast::RulesSet> SmartLinkRedirectRulesRepositoryDsl::Read(
    std::shared_ptr<dsl::IContext> context
) {
    // 1. Прочитать BSON документ из MongoDB
    auto doc_opt = repository_->Read();

    if (!doc_opt.has_value()) {
        LOG_DEBUG << "Document not found in MongoDB";
        return std::nullopt;
    }

    auto doc = doc_opt->view();

    // 2. Извлечь поле "rules_dsl" как строку
    auto rules_dsl_element = doc["rules_dsl"];
    if (!rules_dsl_element || rules_dsl_element.type() != bsoncxx::type::k_string) {
        LOG_WARN << "Field 'rules_dsl' not found or not a string";
        return std::nullopt;
    }

    std::string rules_dsl_string(rules_dsl_element.get_string().value);

    // 3. Распарсить DSL через CombinedParser в RulesSet
    auto parse_result = parser_->Parse(rules_dsl_string, context);

    if (std::holds_alternative<std::string>(parse_result)) {
        // Ошибка парсинга
        std::string error_msg = std::get<std::string>(parse_result);
        LOG_ERROR << "DSL parsing error: " << error_msg;
        return std::nullopt;
    }

    // Успешный парсинг
    return std::get<dsl::ast::RulesSet>(parse_result);
}

} // namespace smartlinks::impl
