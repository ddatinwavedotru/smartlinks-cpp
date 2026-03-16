#pragma once

#include "domain/i_redirect_rule.hpp"
#include <string>
#include <chrono>

namespace smartlinks::impl {

/**
 * @brief Реализация IRedirectRule
 *
 * Хранит данные одного правила редиректа, извлечённые из BSON документа.
 * Аналог ImplRedirectRule из C# версии.
 */
class RedirectRule : public domain::IRedirectRule {
private:
    std::string language_;
    std::string redirect_;
    std::chrono::system_clock::time_point start_;
    std::chrono::system_clock::time_point end_;
    bool authorized_;

public:
    /**
     * @brief Конструктор
     * @param language Язык правила (например, "ru-RU", "any")
     * @param redirect URL редиректа
     * @param start Время начала действия
     * @param end Время окончания действия
     * @param authorized Требуется ли JWT авторизация (по умолчанию false)
     */
    RedirectRule(
        std::string language,
        std::string redirect,
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end,
        bool authorized = false
    ) : language_(std::move(language)),
        redirect_(std::move(redirect)),
        start_(start),
        end_(end),
        authorized_(authorized) {}

    std::string GetLanguage() const override {
        return language_;
    }

    std::string GetRedirect() const override {
        return redirect_;
    }

    std::chrono::system_clock::time_point GetStart() const override {
        return start_;
    }

    std::chrono::system_clock::time_point GetEnd() const override {
        return end_;
    }

    bool GetAuthorized() const override {
        return authorized_;
    }
};

} // namespace smartlinks::impl
