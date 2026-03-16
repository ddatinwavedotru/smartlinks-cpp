#pragma once

#include "expression_parser.hpp"
#include "dsl/ast/or_expression.hpp"
#include <memory>

namespace smartlinks::dsl::parser {

// Forward declarations
class IExpressionParser;

/**
 * @brief Парсер OR выражений
 *
 * Грамматика: OrExpr ::= AndExpr { "OR" AndExpr }
 *
 * Примеры:
 * - "AUTHORIZED OR LANGUAGE = ru-RU"
 * - "AUTHORIZED"  (одно AND выражение без OR)
 */
class OrExpressionParser : public IExpressionParser {
private:
    std::shared_ptr<IExpressionParser> and_parser_;  // Парсер для AND выражений

public:
    explicit OrExpressionParser(std::shared_ptr<IExpressionParser> and_parser)
        : and_parser_(std::move(and_parser)) {}

    std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
    Parse(const std::string& expr_str, std::shared_ptr<IContext> context) const override;
};

} // namespace smartlinks::dsl::parser
