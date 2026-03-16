#pragma once

#include "i_authorized_accessible.hpp"
#include "dsl/context.hpp"
#include <memory>

namespace smartlinks::dsl::plugins::authorized {

/**
 * @brief Адаптер преобразующий IContext в IAuthorizedAccessible
 *
 * Извлекает JWT validation логику из IContext:
 * - Получает "request" и "jwt_validator" из контекста
 * - Проверяет Bearer токен из Authorization header
 */
class Context2AuthorizedAccessible : public IAuthorizedAccessible {
private:
    std::shared_ptr<dsl::IContext> context_;

public:
    explicit Context2AuthorizedAccessible(std::shared_ptr<dsl::IContext> context);

    bool is_authorized() const override;
};

} // namespace smartlinks::dsl::plugins::authorized
