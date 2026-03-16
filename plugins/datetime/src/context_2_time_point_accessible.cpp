#include "context_2_time_point_accessible.hpp"
#include <chrono>

namespace smartlinks::dsl::plugins::datetime {

Context2TimePointAccessible::Context2TimePointAccessible(std::shared_ptr<dsl::IContext> context)
    : context_(std::move(context)) {}

time_t Context2TimePointAccessible::current_time() const {
    // Используем текущее системное время
    // (аналогично логике из Context::FromHttpRequest)
    auto now = std::chrono::system_clock::now();
    return std::chrono::system_clock::to_time_t(now);
}

} // namespace smartlinks::dsl::plugins::datetime
