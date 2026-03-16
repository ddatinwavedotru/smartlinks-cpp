#include "impl/smart_link_redirect_service_dsl.hpp"
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>

namespace smartlinks::impl {

SmartLinkRedirectServiceDsl::SmartLinkRedirectServiceDsl(
    std::shared_ptr<dsl::IContext> context,
    std::shared_ptr<domain::ISmartLinkRedirectRulesRepositoryDsl> repository
) : context_(std::move(context))
  , repository_(std::move(repository)) {}

std::optional<std::string> SmartLinkRedirectServiceDsl::Evaluate() {
    // 1. Прочитать RulesSet через repository (передать context для парсинга)
    auto rules_set_opt = repository_->Read(context_);

    if (!rules_set_opt.has_value()) {
        LOG_DEBUG << "No rules found or parsing failed";
        return std::nullopt;
    }

    auto rules_set = rules_set_opt.value();

    // 2. Вычислить правила используя context (уже заполнен через DI)
    auto redirect_url = rules_set.Evaluate(context_);

    // 3. Вернуть результат
    return redirect_url;
}

} // namespace smartlinks::impl
