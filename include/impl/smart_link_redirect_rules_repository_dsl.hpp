#pragma once

#include "domain/i_smart_link_redirect_rules_repository_dsl.hpp"
#include "domain/i_mongodb_repository.hpp"
#include "dsl/parser/i_combined_parser.hpp"
#include <memory>

namespace smartlinks::impl {

/**
 * @brief Реализация репозитория DSL правил редиректа
 *
 * Читает поле "rules_dsl" из MongoDB документа и парсит его в RulesSet.
 */
class SmartLinkRedirectRulesRepositoryDsl : public domain::ISmartLinkRedirectRulesRepositoryDsl {
private:
    std::shared_ptr<domain::IMongoDbRepository> repository_;
    std::shared_ptr<dsl::parser::ICombinedParser> parser_;

public:
    /**
     * @brief Конструктор с DI
     *
     * @param repository MongoDB repository для чтения документа
     * @param parser DSL parser для парсинга rules_dsl
     */
    SmartLinkRedirectRulesRepositoryDsl(
        std::shared_ptr<domain::IMongoDbRepository> repository,
        std::shared_ptr<dsl::parser::ICombinedParser> parser
    );

    /**
     * @brief Прочитать и распарсить DSL правила
     *
     * @param context Контекст для создания AST узлов с инжектированными адаптерами
     * @return RulesSet или std::nullopt если ошибка
     */
    std::optional<dsl::ast::RulesSet> Read(std::shared_ptr<dsl::IContext> context) override;
};

} // namespace smartlinks::impl
