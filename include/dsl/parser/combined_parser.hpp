#pragma once

#include "i_combined_parser.hpp"
#include "parser_registry.hpp"
#include "expression_parser.hpp"
#include "dsl/ast/rules_set.hpp"
#include "dsl/ast/redirect_rule.hpp"
#include <memory>
#include <string>
#include <variant>

namespace smartlinks::dsl::parser {

/**
 * @brief Комбинированный парсер DSL
 *
 * Собирается из:
 * - Ядра (AND, OR, скобки) - обрабатываются рекурсивными парсерами
 * - Зарегистрированных leaf парсеров из плагинов
 *
 * Создается на каждый запрос через IoC.Resolve("ICombinedParser").
 */
class CombinedParser : public ICombinedParser {
private:
    std::shared_ptr<ParserRegistry> registry_;
    std::shared_ptr<IExpressionParser> or_parser_;  // Корневой парсер выражений

public:
    explicit CombinedParser(std::shared_ptr<ParserRegistry> registry);

    /**
     * @brief Распарсить DSL строку в RulesSet
     *
     * Грамматика (EBNF):
     * Rules    ::= Rule { ";" Rule } [ ";" ]
     * Rule     ::= Condition "->" Link
     * Condition ::= OrExpr
     * OrExpr   ::= AndExpr { "OR" AndExpr }
     * AndExpr  ::= Primary { "AND" Primary }
     * Primary  ::= "(" Condition ")" | LeafExpression
     *
     * @param dsl_string DSL строка из MongoDB (поле rules_dsl)
     * @param context Контекст для создания AST узлов с инжектированными адаптерами
     * @return RulesSet если успех, иначе строка с ошибкой
     */
    std::variant<ast::RulesSet, std::string> Parse(
        const std::string& dsl_string,
        std::shared_ptr<IContext> context
    ) const override;

private:
    /**
     * @brief Распарсить одно правило: Condition -> URL
     */
    std::variant<ast::RedirectRule, std::string> ParseRule(
        const std::string& rule_str,
        std::shared_ptr<IContext> context
    ) const;
};

} // namespace smartlinks::dsl::parser
