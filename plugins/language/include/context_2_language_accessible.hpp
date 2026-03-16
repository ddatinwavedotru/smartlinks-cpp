#pragma once

#include "i_language_accessible.hpp"
#include "dsl/context.hpp"
#include <memory>
#include <string>

namespace smartlinks::dsl::plugins::language {

/**
 * @brief Адаптер преобразующий IContext в ILanguageAccessible
 *
 * Извлекает Accept-Language header из request в IContext.
 * Реализует логику сопоставления языков из Context::LanguageMatches.
 */
class Context2LanguageAccessible : public ILanguageAccessible {
private:
    std::shared_ptr<dsl::IContext> context_;

public:
    explicit Context2LanguageAccessible(std::shared_ptr<dsl::IContext> context);

    bool language_matches(const std::string& pattern) const override;

private:
    std::string GetAcceptLanguage() const;
    static std::string ToLower(const std::string& str);
};

} // namespace smartlinks::dsl::plugins::language
