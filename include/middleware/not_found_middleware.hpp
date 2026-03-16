#pragma once

#include "middleware/i_middleware.hpp"
#include "domain/i_statable_smart_link_repository.hpp"
#include <memory>

namespace smartlinks::middleware {

/**
 * @brief Middleware для проверки существования SmartLink (404 Not Found)
 *
 * Аналог The_App_Responses_404_Not_Found_Middleware из C# версии.
 *
 * Логика:
 * - Прочитать SmartLink из репозитория
 * - Если НЕ найдена ИЛИ state == "deleted" → 404 Not Found
 * - Иначе → вызвать next
 */
class NotFoundMiddleware : public IMiddleware {
private:
    std::shared_ptr<domain::IStatableSmartLinkRepository> repository_;

public:
    /**
     * @brief Конструктор с DI
     * @param repository IStatableSmartLinkRepository (DI через конструктор!)
     */
    explicit NotFoundMiddleware(
        std::shared_ptr<domain::IStatableSmartLinkRepository> repository
    ) : repository_(std::move(repository)) {}

    void Invoke(
        const drogon::HttpRequestPtr& request,
        drogon::HttpResponsePtr& response,
        RequestDelegate next
    ) override;
};

} // namespace smartlinks::middleware
