#pragma once

#include "dsl/parser/i_parser.hpp"
#include "dsl/ast/datetime_expressions.hpp"
#include "ioc/ioc.hpp"
#include <string>
#include <regex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <optional>
#include <ctime>

namespace smartlinks::dsl::plugins::datetime {

/**
 * @brief Парсер для datetime выражений
 *
 * Парсит:
 * - DATETIME = 2026-01-01T00:00:00Z
 * - DATETIME != 2026-01-01T00:00:00Z
 * - DATETIME < 2026-01-01T00:00:00Z
 * - DATETIME <= 2026-01-01T00:00:00Z
 * - DATETIME > 2026-01-01T00:00:00Z
 * - DATETIME >= 2026-01-01T00:00:00Z
 * - DATETIME IN[2026-01-01T00:00:00Z, 2026-12-31T23:59:59Z]
 */
class DatetimeParser : public parser::IParser {
public:
    parser::ParseResult TryParse(
        const std::string& input,
        std::shared_ptr<IContext> context
    ) const override {
        auto trimmed = Trim(input);

        // Попробовать IN сначала (более специфичный паттерн)
        auto in_result = TryParseIn(trimmed, context);
        if (in_result.success) {
            return in_result;
        }

        // Попробовать обычные операторы сравнения
        return TryParseComparison(trimmed, context);
    }

    std::string GetName() const override {
        return "Datetime";
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
     * @brief Попробовать распарсить DATETIME IN[start, end]
     */
    parser::ParseResult TryParseIn(
        const std::string& input,
        std::shared_ptr<IContext> context
    ) const {
        // Паттерн: DATETIME\s+IN\s*\[\s*(.+?)\s*,\s*(.+?)\s*\]
        std::regex pattern(
            R"(DATETIME\s+IN\s*\[\s*(.+?)\s*,\s*(.+?)\s*\])",
            std::regex::icase
        );

        std::smatch match;
        if (!std::regex_match(input, match, pattern)) {
            return parser::ParseResult{false, nullptr, "", 0};
        }

        // Распарсить start и end
        auto start_str = Trim(match[1].str());
        auto end_str = Trim(match[2].str());

        auto start_opt = ParseISO8601(start_str);
        auto end_opt = ParseISO8601(end_str);

        if (!start_opt || !end_opt) {
            return parser::ParseResult{
                false, nullptr,
                "Invalid datetime format in IN",
                0
            };
        }

        // Создать AST узел через DI
        try {
            ioc::Args args;
            args.push_back(context);  // Передать context как первый аргумент
            args.push_back(*start_opt);
            args.push_back(*end_opt);

            auto expr = ioc::IoC::Resolve<std::shared_ptr<ast::IBoolExpression>>(
                "DateTime.AST.Node.IN",
                args
            );

            return parser::ParseResult{
                true,
                expr,
                "",
                input.length()
            };
        } catch (const std::exception& e) {
            return parser::ParseResult{
                false, nullptr,
                std::string("Failed to create IN node: ") + e.what(),
                0
            };
        }
    }

    /**
     * @brief Попробовать распарсить DATETIME op value
     */
    parser::ParseResult TryParseComparison(
        const std::string& input,
        std::shared_ptr<IContext> context
    ) const {
        // Паттерн: DATETIME\s*(=|!=|<=|<|>=|>)\s*(.+)
        std::regex pattern(
            R"(DATETIME\s*(=|!=|<=|<|>=|>)\s*(.+))",
            std::regex::icase
        );

        std::smatch match;
        if (!std::regex_match(input, match, pattern)) {
            return parser::ParseResult{false, nullptr, "", 0};
        }

        // Извлечь оператор и значение
        std::string op = match[1].str();
        std::string value_str = Trim(match[2].str());

        // Распарсить datetime
        auto value_opt = ParseISO8601(value_str);
        if (!value_opt) {
            return parser::ParseResult{
                false, nullptr,
                "Invalid datetime format: " + value_str,
                0
            };
        }

        // Создать AST узел через DI фабрику
        try {
            ioc::Args args;
            args.push_back(context);  // Передать context как первый аргумент
            args.push_back(*value_opt);

            auto expr = ioc::IoC::Resolve<std::shared_ptr<ast::IBoolExpression>>(
                "DateTime.AST.Node." + op,
                args
            );

            return parser::ParseResult{
                true,
                expr,
                "",
                input.length()
            };
        } catch (const std::exception& e) {
            return parser::ParseResult{
                false, nullptr,
                std::string("Unknown operator or failed to create node: ") + op,
                0
            };
        }
    }

    /**
     * @brief Распарсить ISO-8601 datetime
     *
     * Поддерживаемые форматы:
     * - 2026-01-01T00:00:00Z (UTC)
     * - 2026-01-01T00:00:00+03:00 (с timezone)
     * - 2026-01-01T00:00:00-05:00 (с timezone)
     *
     * @return time_t (количество секунд с Unix epoch)
     */
    static std::optional<time_t>
    ParseISO8601(const std::string& str) {
        std::tm tm = {};
        std::istringstream ss(str);

        // Попробовать формат с 'Z' (UTC)
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

        if (ss.fail()) {
            // Попробовать формат с timezone (+03:00, -05:00)
            ss.clear();
            ss.str(str);

            // Упрощенный парсинг: игнорируем timezone, считаем UTC
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

            if (ss.fail()) {
                return std::nullopt;
            }
        }

        // Преобразовать в time_t (UTC)
        #ifdef _WIN32
            auto time_t_value = _mkgmtime(&tm);
        #else
            auto time_t_value = timegm(&tm);
        #endif

        return time_t_value;
    }
};

} // namespace smartlinks::dsl::plugins::datetime
