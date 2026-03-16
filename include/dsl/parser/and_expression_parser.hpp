#pragma once

#include "expression_parser.hpp"
#include "dsl/ast/and_expression.hpp"
#include <memory>

namespace smartlinks::dsl::parser {

class IExpressionParser;

/**
 * @brief Парсер AND выражений
 *
 * Грамматика: AndExpr ::= Primary { "AND" Primary }
 *
 * Примеры:
 * - "AUTHORIZED AND LANGUAGE = ru-RU"
 * - "AUTHORIZED"  (одно Primary выражение без AND)
 */
class AndExpressionParser : public IExpressionParser {
private:
    std::shared_ptr<IExpressionParser> primary_parser_;  // Парсер для Primary выражений

public:
    explicit AndExpressionParser(std::shared_ptr<IExpressionParser> primary_parser)
        : primary_parser_(std::move(primary_parser)) {}

    std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
    Parse(const std::string& expr_str, std::shared_ptr<IContext> context) const override;
};

} // namespace smartlinks::dsl::parser
