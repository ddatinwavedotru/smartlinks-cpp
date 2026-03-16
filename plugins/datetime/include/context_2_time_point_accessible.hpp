#pragma once

#include "i_time_point_accessible.hpp"
#include "dsl/context.hpp"
#include <memory>

namespace smartlinks::dsl::plugins::datetime {

/**
 * @brief Адаптер преобразующий IContext в ITimePointAccessible
 *
 * Извлекает данные из IContext:
 * - Использует текущее системное время
 */
class Context2TimePointAccessible : public ITimePointAccessible {
private:
    std::shared_ptr<dsl::IContext> context_;

public:
    explicit Context2TimePointAccessible(std::shared_ptr<dsl::IContext> context);

    time_t current_time() const override;
};

} // namespace smartlinks::dsl::plugins::datetime
