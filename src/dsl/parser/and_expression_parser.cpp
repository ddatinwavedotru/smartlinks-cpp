#include "dsl/parser/and_expression_parser.hpp"
#include "dsl/parser/parser_utils.hpp"
#include "dsl/ast/and_expression.hpp"

namespace smartlinks::dsl::parser {

std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
AndExpressionParser::Parse(const std::string& expr_str, std::shared_ptr<IContext> context) const {
    using namespace utils;

    auto trimmed = Trim(expr_str);

    // Разбить по "AND" с учетом скобок
    auto parts = SplitRespectingParentheses(trimmed, "AND");

    if (parts.size() == 1) {
        // Нет AND операторов - делегировать Primary парсеру
        return primary_parser_->Parse(parts[0], context);
    }

    // Есть AND операторы - построить дерево
    // Парсим первую часть
    auto left_result = primary_parser_->Parse(parts[0], context);
    if (std::holds_alternative<std::string>(left_result)) {
        return left_result;  // Ошибка
    }

    auto current = std::get<std::shared_ptr<ast::IBoolExpression>>(left_result);

    // Последовательно добавляем остальные части через AND
    for (size_t i = 1; i < parts.size(); ++i) {
        auto right_result = primary_parser_->Parse(parts[i], context);
        if (std::holds_alternative<std::string>(right_result)) {
            return right_result;  // Ошибка
        }

        auto right = std::get<std::shared_ptr<ast::IBoolExpression>>(right_result);

        // Создать AND узел
        current = std::make_shared<ast::AndExpression>(current, right);
    }

    return current;
}

} // namespace smartlinks::dsl::parser
