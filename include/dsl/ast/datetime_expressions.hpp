#pragma once

#include "expression.hpp"
#include "dsl/context.hpp"
#include "ioc/ioc.hpp"
#include "i_time_point_accessible.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace smartlinks::dsl::ast {

// Вспомогательная функция для форматирования datetime из time_t
inline std::string FormatTimeT(time_t time_t_value) {
    // Использовать потокобезопасную версию gmtime
    std::tm tm_buf = {};
    #ifdef _WIN32
        gmtime_s(&tm_buf, &time_t_value);
    #else
        gmtime_r(&time_t_value, &tm_buf);
    #endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

/**
 * @brief DATETIME = value (терминальное выражение)
 */
class DatetimeEqualExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter_;
    time_t value_;

public:
    DatetimeEqualExpression(
        std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter,
        time_t value
    ) : adapter_(std::move(adapter)), value_(value) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return adapter_->current_time() == value_;
    }

    std::string ToString() const override {
        return "DATETIME = " + FormatTimeT(value_);
    }
};

/**
 * @brief DATETIME != value (терминальное выражение)
 */
class DatetimeNotEqualExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter_;
    time_t value_;

public:
    DatetimeNotEqualExpression(
        std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter,
        time_t value
    ) : adapter_(std::move(adapter)), value_(value) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return adapter_->current_time() != value_;
    }

    std::string ToString() const override {
        return "DATETIME != " + FormatTimeT(value_);
    }
};

/**
 * @brief DATETIME < value (терминальное выражение)
 */
class DatetimeLessExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter_;
    time_t value_;

public:
    DatetimeLessExpression(
        std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter,
        time_t value
    ) : adapter_(std::move(adapter)), value_(value) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return adapter_->current_time() < value_;
    }

    std::string ToString() const override {
        return "DATETIME < " + FormatTimeT(value_);
    }
};

/**
 * @brief DATETIME <= value (терминальное выражение)
 */
class DatetimeLessEqualExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter_;
    time_t value_;

public:
    DatetimeLessEqualExpression(
        std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter,
        time_t value
    ) : adapter_(std::move(adapter)), value_(value) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return adapter_->current_time() <= value_;
    }

    std::string ToString() const override {
        return "DATETIME <= " + FormatTimeT(value_);
    }
};

/**
 * @brief DATETIME > value (терминальное выражение)
 */
class DatetimeGreaterExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter_;
    time_t value_;

public:
    DatetimeGreaterExpression(
        std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter,
        time_t value
    ) : adapter_(std::move(adapter)), value_(value) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return adapter_->current_time() > value_;
    }

    std::string ToString() const override {
        return "DATETIME > " + FormatTimeT(value_);
    }
};

/**
 * @brief DATETIME >= value (терминальное выражение)
 */
class DatetimeGreaterEqualExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter_;
    time_t value_;

public:
    DatetimeGreaterEqualExpression(
        std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter,
        time_t value
    ) : adapter_(std::move(adapter)), value_(value) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        return adapter_->current_time() >= value_;
    }

    std::string ToString() const override {
        return "DATETIME >= " + FormatTimeT(value_);
    }
};

/**
 * @brief DATETIME IN[start, end] (терминальное выражение)
 *
 * Проверяет: start <= current_time < end
 */
class DatetimeInExpression : public IBoolExpression {
private:
    std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter_;
    time_t start_;
    time_t end_;

public:
    DatetimeInExpression(
        std::shared_ptr<plugins::datetime::ITimePointAccessible> adapter,
        time_t start,
        time_t end
    ) : adapter_(std::move(adapter)), start_(start), end_(end) {}

    bool Evaluate(const std::shared_ptr<IContext>& ctx) const override {
        auto current_time = adapter_->current_time();
        return start_ <= current_time && current_time < end_;
    }

    std::string ToString() const override {
        return "DATETIME IN[" + FormatTimeT(start_) + ", " + FormatTimeT(end_) + "]";
    }
};

} // namespace smartlinks::dsl::ast
