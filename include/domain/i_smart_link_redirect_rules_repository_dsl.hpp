#pragma once

#include "dsl/ast/rules_set.hpp"
#include <memory>
#include <optional>

namespace smartlinks::dsl {
    class IContext;
}

namespace smartlinks::domain {

/**
 * @brief Интерфейс репозитория для DSL правил редиректа
 *
 * Аналог ISmartLinkRedirectRulesRepository, но возвращает RulesSet вместо
 * вектора IRedirectRule. Отвечает за чтение и парсинг DSL правил из MongoDB.
 */
class ISmartLinkRedirectRulesRepositoryDsl {
public:
    virtual ~ISmartLinkRedirectRulesRepositoryDsl() = default;

    /**
     * @brief Прочитать и распарсить DSL правила
     *
     * Логика:
     * 1. Прочитать BSON документ из MongoDB
     * 2. Извлечь поле "rules_dsl" как строку
     * 3. Распарсить DSL через CombinedParser в RulesSet (передавая context для создания AST)
     *
     * @param context Контекст для создания AST узлов с инжектированными адаптерами
     * @return RulesSet если парсинг успешен, std::nullopt если документ не найден или ошибка парсинга
     */
    virtual std::optional<dsl::ast::RulesSet> Read(std::shared_ptr<dsl::IContext> context) = 0;
};

} // namespace smartlinks::domain
