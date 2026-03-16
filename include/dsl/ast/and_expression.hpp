#pragma once

#include "expression.hpp"
#include "dsl/context.hpp"
#include <memory>

namespace smartlinks::dsl::ast {

/**
 * @brief Логический оператор AND (нетерминальное выражение)
 *
 * Вычисляет: left AND right
 * Результат: true если оба операнда true
 */
class AndExpression : public IBoolExpression {
private:
    std::shared_ptr<IBoolExpression> left_;
    std::shared_ptr<IBoolExpression> right_;

public:
    AndExpression(std::shared_ptr<IBoolExpression> left,
                  std::shared_ptr<IBoolExpression> right)
        : left_(std::move(left))
        , right_(std::move(right)) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return left_->Evaluate(ctx) && right_->Evaluate(ctx);
    }

    std::string ToString() const override {
        return "(" + left_->ToString() + " AND " + right_->ToString() + ")";
    }
};

} // namespace smartlinks::dsl::ast
