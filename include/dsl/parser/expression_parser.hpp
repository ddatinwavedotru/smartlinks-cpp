#pragma once

#include "dsl/ast/expression.hpp"
#include <memory>
#include <string>
#include <variant>

namespace smartlinks::dsl {
    class IContext;
}

namespace smartlinks::dsl::parser {

/**
 * @brief Интерфейс парсера выражений
 *
 * Каждый уровень грамматики (OrExpr, AndExpr, Primary) реализует этот интерфейс.
 */
class IExpressionParser {
public:
    virtual ~IExpressionParser() = default;

    /**
     * @brief Распарсить строку выражения в AST
     *
     * @param expr_str Строка с выражением
     * @param context Контекст для создания AST узлов с инжектированными адаптерами
     * @return AST узел если успех, иначе строка с ошибкой
     */
    virtual std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
    Parse(const std::string& expr_str, std::shared_ptr<IContext> context) const = 0;
};

} // namespace smartlinks::dsl::parser
