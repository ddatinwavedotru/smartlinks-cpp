#pragma once

namespace smartlinks::dsl::plugins::authorized {

/**
 * @brief Интерфейс для проверки авторизации из контекста
 */
class IAuthorizedAccessible {
public:
    virtual ~IAuthorizedAccessible() = default;

    /**
     * @brief Проверить авторизован ли запрос
     * @return true если запрос авторизован (валидный JWT токен)
     */
    virtual bool is_authorized() const = 0;
};

} // namespace smartlinks::dsl::plugins::authorized
