#pragma once

#include "expression.hpp"
#include "dsl/context.hpp"
#include <memory>
#include <string>

namespace smartlinks::dsl::ast {

/**
 * @brief Правило редиректа: условие -> URL
 *
 * Представляет одно правило DSL формата:
 * LANGUAGE = ru-RU AND DATETIME BETWEEN 2026-01-01T00:00:00Z AND 2026-12-31T23:59:59Z -> https://example.ru
 */
class RedirectRule {
private:
    std::shared_ptr<IBoolExpression> condition_;
    std::string redirect_url_;

public:
    RedirectRule(std::shared_ptr<IBoolExpression> condition, std::string url)
        : condition_(std::move(condition))
        , redirect_url_(std::move(url)) {}

    /**
     * @brief Проверить соответствие правила контексту
     * @param ctx Контекст с данными запроса
     * @return true если правило подходит
     */
    bool Matches(const std::shared_ptr<IContext>& ctx) const {
        return condition_->Evaluate(ctx);
    }

    /**
     * @brief Получить URL редиректа
     */
    std::string GetRedirectUrl() const {
        return redirect_url_;
    }

    /**
     * @brief Преобразовать в строку (для отладки)
     */
    std::string ToString() const {
        return condition_->ToString() + " -> " + redirect_url_;
    }
};

} // namespace smartlinks::dsl::ast
