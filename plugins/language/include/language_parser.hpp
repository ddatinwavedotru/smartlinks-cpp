#pragma once

#include "dsl/parser/i_parser.hpp"
#include "dsl/ast/language_expressions.hpp"
#include "ioc/ioc.hpp"
#include <string>
#include <regex>

namespace smartlinks::dsl::plugins::language {

/**
 * @brief Парсер для языковых выражений
 *
 * Парсит:
 * - LANGUAGE = ru-RU
 * - LANGUAGE != en-US
 * - LANGUAGE = *
 * - LANGUAGE = any
 */
class LanguageParser : public parser::IParser {
public:
    parser::ParseResult TryParse(
        const std::string& input,
        std::shared_ptr<IContext> context
    ) const override {
        auto trimmed = Trim(input);

        // Попробовать распарсить "LANGUAGE = value" или "LANGUAGE != value"
        // Регулярное выражение: LANGUAGE\s*(=|!=)\s*(.+)
        std::regex pattern(R"(LANGUAGE\s*(=|!=)\s*(.+))", std::regex::icase);
        std::smatch match;

        if (!std::regex_match(trimmed, match, pattern)) {
            return parser::ParseResult{
                .success = false,
                .expression = nullptr,
                .error_message = "Not a LANGUAGE expression",
                .consumed_chars = 0
            };
        }

        // Извлечь оператор и значение
        std::string op = match[1].str();
        std::string value = Trim(match[2].str());

        // Проверить валидность языкового токена
        if (!IsValidLanguageToken(value)) {
            return parser::ParseResult{
                .success = false,
                .expression = nullptr,
                .error_message = "Invalid language token: " + value,
                .consumed_chars = 0
            };
        }

        // Создать AST узел через IoC
        ioc::Args args;
        args.push_back(context);  // Передать context как первый аргумент
        args.push_back(value);

        std::string factory_key = "Language.AST.Node." + op;

        std::shared_ptr<ast::IBoolExpression> expr;
        try {
            expr = ioc::IoC::Resolve<std::shared_ptr<ast::IBoolExpression>>(
                factory_key,
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

    std::string GetName() const override {
        return "Language";
    }

    int GetPriority() const override {
        return 50;  // Средний приоритет
    }

private:
    static std::string Trim(const std::string& str) {
        auto start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";

        auto end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    /**
     * @brief Проверить валидность языкового токена
     *
     * Допустимые значения:
     * - "any", "*" - специальные значения
     * - Accept-Language токены: "en", "en-US", "zh-Hant", "fr-CA", etc.
     */
    static bool IsValidLanguageToken(const std::string& token) {
        // Специальные значения
        if (token == "any" || token == "*") {
            return true;
        }

        // Accept-Language токен: основной язык + опциональные subtags
        // Формат: en, en-US, zh-Hant, sr-Cyrl-RS, etc.
        std::regex lang_pattern(R"([a-z]{2,3}(-[A-Z][a-z]{3})?(-[A-Z]{2})?)", std::regex::icase);

        return std::regex_match(token, lang_pattern);
    }
};

} // namespace smartlinks::dsl::plugins::language
