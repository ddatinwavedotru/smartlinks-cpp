#pragma once

#include "dsl/ast/rules_set.hpp"
#include <memory>
#include <string>
#include <variant>

namespace smartlinks::dsl {
    class IContext;
}

namespace smartlinks::dsl::parser {

/**
 * @brief Интерфейс комбинированного парсера DSL
 */
class ICombinedParser {
public:
    virtual ~ICombinedParser() = default;

    /**
     * @brief Распарсить DSL строку в RulesSet
     *
     * @param dsl_string DSL строка из MongoDB (поле rules_dsl)
     * @param context Контекст для создания AST узлов с инжектированными адаптерами
     * @return RulesSet если успех, иначе строка с ошибкой
     */
    virtual std::variant<ast::RulesSet, std::string> Parse(
        const std::string& dsl_string,
        std::shared_ptr<IContext> context
    ) const = 0;
};

} // namespace smartlinks::dsl::parser
