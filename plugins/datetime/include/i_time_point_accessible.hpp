#pragma once

#include <ctime>

namespace smartlinks::dsl::plugins::datetime {

/**
 * @brief Интерфейс для доступа к текущему времени из контекста
 */
class ITimePointAccessible {
public:
    virtual ~ITimePointAccessible() = default;

    /**
     * @brief Получить текущее время как time_t
     * @return Текущее время в формате time_t
     */
    virtual time_t current_time() const = 0;
};

} // namespace smartlinks::dsl::plugins::datetime
