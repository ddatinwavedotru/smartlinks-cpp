#pragma once

#include "domain/i_smart_link.hpp"
#include <drogon/HttpRequest.h>
#include <memory>

namespace smartlinks::impl {

/**
 * @brief Реализация ISmartLink
 *
 * Извлекает slug из HTTP запроса (path).
 * Аналог ImplSmartLink из C# версии.
 */
class SmartLink : public domain::ISmartLink {
private:
    drogon::HttpRequestPtr request_;

public:
    /**
     * @brief Конструктор с DI
     * @param request HTTP запрос (DI через конструктор)
     */
    explicit SmartLink(const drogon::HttpRequestPtr& request)
        : request_(request) {}

    /**
     * @brief Получить slug из path запроса
     * @return Путь запроса (например, "/my-link")
     */
    std::string GetValue() const override {
        return request_->path();
    }
};

} // namespace smartlinks::impl
