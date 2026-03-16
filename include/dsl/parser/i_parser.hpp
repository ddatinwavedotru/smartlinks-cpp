#pragma once

#include "dsl/ast/expression.hpp"
#include <memory>
#include <string>

namespace smartlinks::dsl {
    class IContext;
}

namespace smartlinks::dsl::parser {

/**
 * @brief Результат парсинга
 */
struct ParseResult {
    bool success;
    std::shared_ptr<ast::IBoolExpression> expression;  // Если success == true
    std::string error_message;                          // Если success == false
    size_t consumed_chars;                              // Сколько символов распарсили

    // Конструктор для успешного парсинга
    static ParseResult Success(std::shared_ptr<ast::IBoolExpression> expr, size_t consumed) {
        return ParseResult{true, std::move(expr), "", consumed};
    }

    // Конструктор для ошибки парсинга
    static ParseResult Error(std::string message) {
        return ParseResult{false, nullptr, std::move(message), 0};
    }
};

/**
 * @brief Интерфейс плагинного парсера
 *
 * Каждый плагин (datetime, language, authorized) реализует этот интерфейс.
 * Парсер пытается распарсить входную строку и вернуть AST узел.
 */
class IParser {
public:
    virtual ~IParser() = default;

    /**
     * @brief Попытка распарсить выражение с начала строки
     *
     * @param input Входная строка DSL
     * @param context Контекст для создания AST узлов с инжектированными адаптерами
     * @return Результат парсинга (успех или ошибка)
     */
    virtual ParseResult TryParse(
        const std::string& input,
        std::shared_ptr<IContext> context
    ) const = 0;

    /**
     * @brief Имя парсера (для отладки и регистрации)
     *
     * Примеры: "Datetime", "Language", "Authorized"
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Приоритет парсера (больше = раньше пытаемся)
     *
     * Используется для определения порядка применения парсеров.
     * Например, AUTHORIZED может иметь приоритет выше, чем LANGUAGE.
     */
    virtual int GetPriority() const = 0;
};

} // namespace smartlinks::dsl::parser
