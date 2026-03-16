#pragma once

#include "domain/i_smart_link_redirect_service.hpp"
#include "domain/i_smart_link_redirect_rules_repository_dsl.hpp"
#include "dsl/context.hpp"
#include <memory>
#include <optional>
#include <string>

namespace smartlinks::impl {

/**
 * @brief Новая реализация ISmartLinkRedirectService для DSL правил
 *
 * Заменяет старый SmartLinkRedirectService. Работает с DSL правилами вместо JSON.
 * Использует ISmartLinkRedirectRulesRepositoryDsl для получения RulesSet,
 * создает Context из HTTP запроса, вычисляет через RulesSet::Evaluate.
 */
class SmartLinkRedirectServiceDsl : public domain::ISmartLinkRedirectService {
private:
    std::shared_ptr<dsl::IContext> context_;
    std::shared_ptr<domain::ISmartLinkRedirectRulesRepositoryDsl> repository_;

public:
    /**
     * @brief Конструктор с DI
     *
     * @param context Контекст для вычисления правил (содержит request и jwt_validator)
     * @param repository DSL repository для чтения и парсинга правил
     */
    SmartLinkRedirectServiceDsl(
        std::shared_ptr<dsl::IContext> context,
        std::shared_ptr<domain::ISmartLinkRedirectRulesRepositoryDsl> repository
    );

    /**
     * @brief Вычислить redirect URL
     *
     * Логика:
     * 1. Прочитать RulesSet через repository_->Read()
     * 2. Создать Context из HTTP запроса
     * 3. Вызвать RulesSet::Evaluate(context)
     * 4. Вернуть redirect URL первого совпавшего правила
     *
     * @return URL редиректа или std::nullopt если ничего не найдено
     */
    std::optional<std::string> Evaluate() override;
};

} // namespace smartlinks::impl
