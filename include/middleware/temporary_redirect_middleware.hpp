#pragma once

#include "middleware/i_middleware.hpp"
#include "domain/i_smart_link_redirect_service.hpp"
#include <memory>

namespace smartlinks::middleware {

/**
 * @brief Middleware для редиректа (307 Temporary Redirect)
 *
 * Аналог The_App_Responses_307_Temporary_Redirect_Middleware из C# версии.
 *
 * Логика:
 * - Найти подходящее правило редиректа
 * - Если найдено → 307 Temporary Redirect с Location header
 * - Иначе → вызвать next (что приведёт к 429, т.к. это последний middleware)
 */
class TemporaryRedirectMiddleware : public IMiddleware {
private:
    std::shared_ptr<domain::ISmartLinkRedirectService> smart_link_redirect_service_;

public:
    /**
     * @brief Конструктор с DI
     * @param smartLinkRedirectService ISmartLinkRedirectService (DI через конструктор!)
     */
    explicit TemporaryRedirectMiddleware(
        std::shared_ptr<domain::ISmartLinkRedirectService> smartLinkRedirectService
    ) : smart_link_redirect_service_(std::move(smartLinkRedirectService)) {}

    void Invoke(
        const drogon::HttpRequestPtr& request,
        drogon::HttpResponsePtr& response,
        RequestDelegate next
    ) override;
};

} // namespace smartlinks::middleware
