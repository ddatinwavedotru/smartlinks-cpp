#pragma once

#include "domain/i_smart_link_redirect_service.hpp"
#include "domain/i_redirectable_smart_link.hpp"
#include "domain/i_smart_link_redirect_rules_repository.hpp"
#include <memory>
#include <optional>
#include <string>
#include <algorithm>

namespace smartlinks::impl {

/**
 * @brief Реализация ISmartLinkRedirectService
 *
 * Основная бизнес-логика поиска подходящего правила редиректа.
 * Аналог ImplSmartLinkRedirectService из C# версии.
 */
class SmartLinkRedirectService : public domain::ISmartLinkRedirectService {
private:
    std::shared_ptr<domain::IRedirectableSmartLink> redirectable_;
    std::shared_ptr<domain::ISmartLinkRedirectRulesRepository> repository_;

public:
    /**
     * @brief Конструктор с DI
     * @param redirectable IRedirectableSmartLink (DI через конструктор)
     * @param repository Repository правил (DI через конструктор)
     */
    SmartLinkRedirectService(
        std::shared_ptr<domain::IRedirectableSmartLink> redirectable,
        std::shared_ptr<domain::ISmartLinkRedirectRulesRepository> repository
    ) : redirectable_(std::move(redirectable)),
        repository_(std::move(repository)) {}

    /**
     * @brief Найти подходящее правило и вернуть URL редиректа
     * @return URL редиректа или std::nullopt если правило не найдено
     *
     * Логика (как в C# версии):
     * 1. Получить все правила из repository
     * 2. Найти ПЕРВОЕ правило, которое подходит по:
     *    - Языку (redirectable->IsLanguageAccepted)
     *    - Времени (redirectable->IsInTime)
     * 3. Вернуть redirect URL этого правила
     * 4. Если ничего не найдено → std::nullopt (вызовет 429)
     */
    std::optional<std::string> Evaluate() override {
        // Получить правила из репозитория
        auto rules_opt = repository_->Read();

        if (!rules_opt.has_value() || rules_opt->empty()) {
            return std::nullopt;
        }

        auto& rules = rules_opt.value();

        // Найти первое подходящее правило
        auto it = std::find_if(rules.begin(), rules.end(),
            [this](const std::shared_ptr<domain::IRedirectRule>& rule) {
                // Проверить язык И время
                bool language_matches = redirectable_->IsLanguageAccepted(
                    rule->GetLanguage()
                );
                bool time_matches = redirectable_->IsInTime(
                    rule->GetStart(),
                    rule->GetEnd()
                );

                // Проверить авторизацию (если требуется в правиле)
                bool auth_matches = true;  // По умолчанию не требуется
                if (rule->GetAuthorized()) {
                    auth_matches = redirectable_->IsAuthorized();
                }

                return language_matches && time_matches && auth_matches;
            }
        );

        // Если нашли подходящее правило
        if (it != rules.end()) {
            return (*it)->GetRedirect();
        }

        // Ничего не найдено
        return std::nullopt;
    }
};

} // namespace smartlinks::impl
