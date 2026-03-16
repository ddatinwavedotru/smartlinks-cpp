#pragma once

#include "dsl/parser/i_parser.hpp"
#include "dsl/ast/authorized_expression.hpp"
#include "ioc/ioc.hpp"
#include <string>

namespace smartlinks::dsl::plugins::authorized {

/**
 * @brief Парсер для выражения AUTHORIZED
 *
 * Парсит строку "AUTHORIZED" и создает AuthorizedExpression.
 */
class AuthorizedParser : public parser::IParser {
public:
    parser::ParseResult TryParse(
        const std::string& input,
        std::shared_ptr<IContext> context
    ) const override {
        // Удалить пробелы
        auto trimmed = Trim(input);

        // Проверить точное совпадение (case-insensitive)
        if (ToUpper(trimmed) == "AUTHORIZED") {
            // Создать AST узел через IoC
            ioc::Args args;
            args.push_back(context);  // Передать context как первый аргумент

            std::shared_ptr<ast::IBoolExpression> expr;
            try {
                expr = ioc::IoC::Resolve<std::shared_ptr<ast::IBoolExpression>>(
                    "Authorized.AST.Node",
                    args
                );
            } catch (const std::exception& e) {
                return parser::ParseResult{
                    .success = false,
                    .expression = nullptr,
                    .error_message = "Failed to create AST node: " + std::string(e.what()),
                    .consumed_chars = 0
                };
            }

            return parser::ParseResult{
                .success = true,
                .expression = expr,
                .error_message = "",
                .consumed_chars = input.length()
            };
        }

        // Не подошло
        return parser::ParseResult{
            .success = false,
            .expression = nullptr,
            .error_message = "Not an AUTHORIZED expression",
            .consumed_chars = 0
        };
    }

    std::string GetName() const override {
        return "Authorized";
    }

    int GetPriority() const override {
        return 60;  // Высокий приоритет (простое выражение)
    }

private:
    static std::string Trim(const std::string& str) {
        auto start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";

        auto end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    static std::string ToUpper(const std::string& str) {
        std::string result = str;
        for (char& c : result) {
            c = std::toupper(static_cast<unsigned char>(c));
        }
        return result;
    }
};

} // namespace smartlinks::dsl::plugins::authorized
