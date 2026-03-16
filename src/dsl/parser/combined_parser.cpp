#include "dsl/parser/combined_parser.hpp"
#include "dsl/parser/or_expression_parser.hpp"
#include "dsl/parser/and_expression_parser.hpp"
#include "dsl/parser/primary_expression_parser.hpp"
#include "dsl/parser/parser_utils.hpp"
#include <iostream>

namespace smartlinks::dsl::parser {

CombinedParser::CombinedParser(std::shared_ptr<ParserRegistry> registry)
    : registry_(std::move(registry)) {

    std::cout << "[CombinedParser] Constructor called with registry=" << registry_.get() << std::endl;

    // Проверим, сколько парсеров в registry
    auto leaf_parsers = registry_->GetLeafParsers();
    std::cout << "[CombinedParser] Registry contains " << leaf_parsers.size() << " leaf parsers:" << std::endl;
    for (const auto& parser : leaf_parsers) {
        std::cout << "[CombinedParser]   - " << parser->GetName()
                  << " (priority=" << parser->GetPriority() << ")" << std::endl;
    }

    // Построить цепочку парсеров (рекурсивный descent parser)
    // Primary -> And -> Or

    // Используем shared_ptr с forward declaration для разрешения циклической зависимости
    // (PrimaryParser использует OrParser для скобок)

    auto or_parser = std::make_shared<OrExpressionParser>(nullptr);  // Заполним позже
    auto primary_parser = std::make_shared<PrimaryExpressionParser>(or_parser, registry_);
    auto and_parser = std::make_shared<AndExpressionParser>(primary_parser);

    // Установить and_parser в or_parser
    or_parser = std::make_shared<OrExpressionParser>(and_parser);

    // Обновить or_parser в primary_parser (нужен для скобок)
    primary_parser = std::make_shared<PrimaryExpressionParser>(or_parser, registry_);

    // Обновить всю цепочку заново
    and_parser = std::make_shared<AndExpressionParser>(primary_parser);
    or_parser_ = std::make_shared<OrExpressionParser>(and_parser);

    std::cout << "[CombinedParser] Parser chain built successfully" << std::endl;
}

std::variant<ast::RulesSet, std::string> CombinedParser::Parse(
    const std::string& dsl_string,
    std::shared_ptr<IContext> context
) const {
    using namespace utils;

    auto trimmed = Trim(dsl_string);

    if (trimmed.empty()) {
        return "Empty DSL string";
    }

    // Разбить по ";" (разделитель правил)
    auto rule_strings = SplitRespectingParentheses(trimmed, ";");

    ast::RulesSet rules_set;

    for (const auto& rule_str : rule_strings) {
        auto trimmed_rule = Trim(rule_str);

        if (trimmed_rule.empty()) {
            continue;  // Пропустить пустые правила (например, "rule1; rule2;")
        }

        // Парсить правило
        auto rule_result = ParseRule(trimmed_rule, context);

        if (std::holds_alternative<std::string>(rule_result)) {
            // Ошибка парсинга
            return "Error parsing rule '" + trimmed_rule + "': " +
                   std::get<std::string>(rule_result);
        }

        rules_set.AddRule(std::get<ast::RedirectRule>(rule_result));
    }

    if (rules_set.Empty()) {
        return "No valid rules found";
    }

    return rules_set;
}

std::variant<ast::RedirectRule, std::string> CombinedParser::ParseRule(
    const std::string& rule_str,
    std::shared_ptr<IContext> context
) const {
    using namespace utils;

    // Найти "->" разделитель
    auto arrow_pos = rule_str.find("->");

    if (arrow_pos == std::string::npos) {
        return "Missing '->' in rule";
    }

    // Разбить на condition и url
    auto condition_str = Trim(rule_str.substr(0, arrow_pos));
    auto url_str = Trim(rule_str.substr(arrow_pos + 2));

    if (condition_str.empty()) {
        return "Empty condition in rule";
    }

    if (url_str.empty()) {
        return "Empty URL in rule";
    }

    // Парсить условие через OR parser (верхний уровень грамматики)
    auto condition_result = or_parser_->Parse(condition_str, context);

    if (std::holds_alternative<std::string>(condition_result)) {
        return std::get<std::string>(condition_result);  // Ошибка
    }

    auto condition = std::get<std::shared_ptr<ast::IBoolExpression>>(condition_result);

    // Создать RedirectRule
    return ast::RedirectRule(condition, url_str);
}

} // namespace smartlinks::dsl::parser
