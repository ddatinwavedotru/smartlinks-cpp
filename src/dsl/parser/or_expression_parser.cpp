#include "dsl/parser/or_expression_parser.hpp"
#include "dsl/parser/parser_utils.hpp"
#include "dsl/ast/or_expression.hpp"

namespace smartlinks::dsl::parser {

std::variant<std::shared_ptr<ast::IBoolExpression>, std::string>
OrExpressionParser::Parse(const std::string& expr_str, std::shared_ptr<IContext> context) const {
    using namespace utils;

    auto trimmed = Trim(expr_str);

    // Разбить по "OR" с учетом скобок
    auto parts = SplitRespectingParentheses(trimmed, "OR");

    if (parts.size() == 1) {
        // Нет OR операторов - делегировать AND парсеру
        return and_parser_->Parse(parts[0], context);
    }

    // Есть OR операторы - построить дерево
    // Парсим первую часть
    auto left_result = and_parser_->Parse(parts[0], context);
    if (std::holds_alternative<std::string>(left_result)) {
        return left_result;  // Ошибка
    }

    auto current = std::get<std::shared_ptr<ast::IBoolExpression>>(left_result);

    // Последовательно добавляем остальные части через OR
    for (size_t i = 1; i < parts.size(); ++i) {
        auto right_result = and_parser_->Parse(parts[i], context);
        if (std::holds_alternative<std::string>(right_result)) {
            return right_result;  // Ошибка
        }

        auto right = std::get<std::shared_ptr<ast::IBoolExpression>>(right_result);

        // Создать OR узел
        current = std::make_shared<ast::OrExpression>(current, right);
    }

    return current;
}

} // namespace smartlinks::dsl::parser
