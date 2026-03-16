#pragma once

#include "redirect_rule.hpp"
#include "dsl/context.hpp"
#include <vector>
#include <optional>
#include <string>

namespace smartlinks::dsl::ast {

/**
 * @brief Набор правил редиректа
 *
 * Хранит список правил, проверяет их последовательно до первого совпадения.
 * Аналог массива rules в старом JSON формате, но с DSL условиями.
 */
class RulesSet {
private:
    std::vector<RedirectRule> rules_;

public:
    RulesSet() = default;

    /**
     * @brief Добавить правило в набор
     */
    void AddRule(RedirectRule rule) {
        rules_.push_back(std::move(rule));
    }

    /**
     * @brief Получить все правила
     */
    const std::vector<RedirectRule>& GetRules() const {
        return rules_;
    }

    /**
     * @brief Вычислить правила для контекста
     *
     * Проверяет правила последовательно до первого совпадения.
     * Логика аналогична SmartLinkRedirectService::Evaluate() - остановка на первом match.
     *
     * @param ctx Контекст с данными запроса
     * @return URL редиректа для первого совпавшего правила, или std::nullopt
     */
    std::optional<std::string> Evaluate(const std::shared_ptr<IContext>& ctx) const {
        for (const auto& rule : rules_) {
            if (rule.Matches(ctx)) {
                return rule.GetRedirectUrl();
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Преобразовать в строку (для отладки)
     */
    std::string ToString() const {
        std::string result;
        for (size_t i = 0; i < rules_.size(); ++i) {
            if (i > 0) {
                result += "; ";
            }
            result += rules_[i].ToString();
        }
        return result;
    }

    /**
     * @brief Проверить, есть ли правила
     */
    bool Empty() const {
        return rules_.empty();
    }

    /**
     * @brief Количество правил
     */
    size_t Size() const {
        return rules_.size();
    }
};

} // namespace smartlinks::dsl::ast
