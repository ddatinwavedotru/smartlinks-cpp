#pragma once

#include "middleware/i_middleware.hpp"
#include "domain/i_freeze_smart_link_service.hpp"
#include <memory>

namespace smartlinks::middleware {

/**
 * @brief Middleware для проверки заморозки SmartLink (422 Unprocessable Entity)
 *
 * Аналог The_App_Responses_422_Unprocessable_Content_Middleware из C# версии.
 *
 * Логика:
 * - Проверить, заморожена ли SmartLink (state == "freezed")
 * - Если заморожена → 422 Unprocessable Entity
 * - Иначе → вызвать next
 */
class UnprocessableContentMiddleware : public IMiddleware {
private:
    std::shared_ptr<domain::IFreezeSmartLinkService> freeze_service_;

public:
    /**
     * @brief Конструктор с DI
     * @param freezableSmartLinkService IFreezeSmartLinkService (DI через конструктор!)
     */
    explicit UnprocessableContentMiddleware(
        std::shared_ptr<domain::IFreezeSmartLinkService> freezableSmartLinkService
    ) : freeze_service_(std::move(freezableSmartLinkService)) {}

    void Invoke(
        const drogon::HttpRequestPtr& request,
        drogon::HttpResponsePtr& response,
        RequestDelegate next
    ) override;
};

} // namespace smartlinks::middleware
