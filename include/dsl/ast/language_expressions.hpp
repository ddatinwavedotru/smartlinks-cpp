#pragma once

#include "expression.hpp"
#include "dsl/context.hpp"
#include "ioc/ioc.hpp"
#include "i_language_accessible.hpp"
#include <string>

namespace smartlinks::dsl::ast {

/**
 * @brief LANGUAGE = value (терминальное выражение)
 *
 * Поддерживаемые значения:
 * - Accept-Language токены (en-US, ru-RU, fr-FR, etc.)
 * - Специальные значения: "any", "*"
 */
class LanguageEqualExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::language::ILanguageAccessible> adapter_;
    std::string value_;

public:
    LanguageEqualExpression(
        std::shared_ptr<plugins::language::ILanguageAccessible> adapter,
        std::string value
    ) : adapter_(std::move(adapter)), value_(std::move(value)) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return adapter_->language_matches(value_);
    }

    std::string ToString() const override {
        return "LANGUAGE = " + value_;
    }
};

/**
 * @brief LANGUAGE != value (терминальное выражение)
 */
class LanguageNotEqualExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::language::ILanguageAccessible> adapter_;
    std::string value_;

public:
    LanguageNotEqualExpression(
        std::shared_ptr<plugins::language::ILanguageAccessible> adapter,
        std::string value
    ) : adapter_(std::move(adapter)), value_(std::move(value)) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return !adapter_->language_matches(value_);
    }

    std::string ToString() const override {
        return "LANGUAGE != " + value_;
    }
};

} // namespace smartlinks::dsl::ast
