#include "dsl/parser/primary_expression_parser.hpp"
#include "dsl/parser/parser_utils.hpp"
#include <iostream>

namespace smartlinks::dsl::parser {

std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
PrimaryExpressionParser::Parse(const std::string& expr_str, std::shared_ptr<IContext> context) const {
    using namespace utils;

    auto trimmed = Trim(expr_str);

    // Проверить скобки
    if (StartsWithParenthesis(trimmed)) {
        // Извлечь содержимое скобок
        auto content = ExtractParenthesesContent(trimmed);
        if (content.empty()) {
            return "Empty parentheses or mismatched parentheses";
        }

        // Рекурсивно распарсить содержимое через OR парсер (верхний уровень)
        return or_parser_->Parse(content, context);
    }

    // Не скобки - попробовать leaf парсеры
    return TryLeafParsers(trimmed, context);
}

std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
PrimaryExpressionParser::TryLeafParsers(const std::string& expr_str, std::shared_ptr<IContext> context) const {
    std::cout << "[PrimaryExpressionParser] TryLeafParsers: '" << expr_str << "'" << std::endl;

    // Получить все leaf парсеры из реестра (отсортированные по приоритету)
    auto leaf_parsers = registry_->GetLeafParsers();

    std::cout << "[PrimaryExpressionParser] Got " << leaf_parsers.size() << " leaf parsers" << std::endl;

    // Попробовать каждый парсер
    for (const auto& parser : leaf_parsers) {
        std::cout << "[PrimaryExpressionParser] Trying parser: " << parser->GetName() << std::endl;
        auto result = parser->TryParse(expr_str, context);

        if (result.success) {
            std::cout << "[PrimaryExpressionParser] Parser " << parser->GetName() << " succeeded!" << std::endl;
            return result.expression;
        } else {
            std::cout << "[PrimaryExpressionParser] Parser " << parser->GetName() << " failed: " << result.error_message << std::endl;
        }
    }

    // Ни один парсер не подошел
    std::cout << "[PrimaryExpressionParser] No parser matched for: '" << expr_str << "'" << std::endl;
    return "Unknown expression: " + expr_str;
}

} // namespace smartlinks::dsl::parser
