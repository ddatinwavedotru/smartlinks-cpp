#pragma once

#include "expression_parser.hpp"
#include "parser_registry.hpp"
#include <memory>

namespace smartlinks::dsl::parser {

class IExpressionParser;

/**
 * @brief Парсер Primary выражений
 *
 * Грамматика: Primary ::= "(" Condition ")" | LeafExpression
 *
 * Примеры:
 * - "(AUTHORIZED OR LANGUAGE = ru-RU)"  - скобки
 * - "AUTHORIZED"                         - листовое выражение
 * - "LANGUAGE = ru-RU"                   - листовое выражение
 * - "DATETIME BETWEEN 2026-01-01T00:00:00Z AND 2026-12-31T23:59:59Z"  - листовое выражение
 */
class PrimaryExpressionParser : public IExpressionParser {
private:
    std::shared_ptr<IExpressionParser> or_parser_;  // Парсер для рекурсии (скобки)
    std::shared_ptr<ParserRegistry> registry_;      // Реестр leaf парсеров

public:
    PrimaryExpressionParser(
        std::shared_ptr<IExpressionParser> or_parser,
        std::shared_ptr<ParserRegistry> registry
    ) : or_parser_(std::move(or_parser))
      , registry_(std::move(registry)) {}

    std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
    Parse(const std::string& expr_str, std::shared_ptr<IContext> context) const override;

private:
    /**
     * @brief Попытка распарсить через leaf парсеры из плагинов
     */
    std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
    TryLeafParsers(const std::string& expr_str, std::shared_ptr<IContext> context) const;
};

} // namespace smartlinks::dsl::parser
