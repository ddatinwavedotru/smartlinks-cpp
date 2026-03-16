#pragma once

#include <memory>
#include <string>

namespace smartlinks::dsl {
    class IContext;  // Forward declaration
}

namespace smartlinks::dsl::ast {

/**
 * @brief Базовый интерфейс всех выражений (паттерн Interpreter)
 *
 * Следует рекомендациям из паттерна Interpreter:
 * - Абстрактный интерфейс с методом interpret(Context)
 * - Терминальные и нетерминальные узлы наследуются от этого интерфейса
 */
class IExpression {
public:
    virtual ~IExpression() = default;

    /**
     * @brief Вычислить выражение в контексте
     * @param ctx Контекст с данными для вычисления
     * @return Результат вычисления
     */
    virtual bool Evaluate(const std::shared_ptr<IContext>& ctx) const = 0;

    /**
     * @brief Преобразовать в строку (для отладки)
     */
    virtual std::string ToString() const = 0;
};

/**
 * @brief Логические выражения (BoolExpr)
 *
 * Результат Evaluate() - булево значение.
 * Примеры: AND, OR, AUTHORIZED, сравнения datetime и language.
 */
class IBoolExpression : public IExpression {
public:
    virtual ~IBoolExpression() = default;
};

} // namespace smartlinks::dsl::ast
