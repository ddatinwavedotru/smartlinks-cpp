#pragma once

#include <string>

namespace smartlinks::dsl::plugins::language {

/**
 * @brief Интерфейс для проверки соответствия языка из контекста
 */
class ILanguageAccessible {
public:
    virtual ~ILanguageAccessible() = default;

    /**
     * @brief Проверить соответствие языка паттерну
     * @param pattern Паттерн языка (например "en-US", "any", "*")
     * @return true если язык соответствует
     */
    virtual bool language_matches(const std::string& pattern) const = 0;
};

} // namespace smartlinks::dsl::plugins::language
