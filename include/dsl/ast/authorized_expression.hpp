#pragma once

#include "expression.hpp"
#include "dsl/context.hpp"
#include "ioc/ioc.hpp"
#include "i_authorized_accessible.hpp"

namespace smartlinks::dsl::ast {

/**
 * @brief Листовое выражение AUTHORIZED (терминальное выражение)
 *
 * Проверяет наличие валидного JWT токена в запросе.
 * Результат: true если запрос авторизован
 */
class AuthorizedExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::authorized::IAuthorizedAccessible> adapter_;

public:
    explicit AuthorizedExpression(
        std::shared_ptr<plugins::authorized::IAuthorizedAccessible> adapter
    ) : adapter_(std::move(adapter)) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return adapter_->is_authorized();
    }

    std::string ToString() const override {
        return "AUTHORIZED";
    }
};

} // namespace smartlinks::dsl::ast
