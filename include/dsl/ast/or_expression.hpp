#pragma once

#include "expression.hpp"
#include "dsl/context.hpp"
#include <memory>

namespace smartlinks::dsl::ast {

/**
 * @brief Логический оператор OR (нетерминальное выражение)
 *
 * Вычисляет: left OR right
 * Результат: true если хотя бы один операнд true
 */
class OrExpression : public IBoolExpression {
private:
    std::shared_ptr<IBoolExpression> left_;
    std::shared_ptr<IBoolExpression> right_;

public:
    OrExpression(std::shared_ptr<IBoolExpression> left,
                 std::shared_ptr<IBoolExpression> right)
        : left_(std::move(left))
        , right_(std::move(right)) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return left_->Evaluate(ctx) || right_->Evaluate(ctx);
    }

    std::string ToString() const override {
        return "(" + left_->ToString() + " OR " + right_->ToString() + ")";
    }
};

} // namespace smartlinks::dsl::ast
