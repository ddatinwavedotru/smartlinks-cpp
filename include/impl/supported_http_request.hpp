#pragma once

#include "domain/i_supported_http_request.hpp"
#include <drogon/HttpRequest.h>
#include <memory>

namespace smartlinks::impl {

/**
 * @brief Реализация ISupportedHttpRequest
 *
 * Проверяет, является ли HTTP метод допустимым для SmartLink.
 * SmartLink поддерживает только GET и HEAD.
 *
 * Аналог ImplSupportedHttpRequest из C# версии.
 */
class SupportedHttpRequest : public domain::ISupportedHttpRequest {
private:
    drogon::HttpRequestPtr request_;

public:
    /**
     * @brief Конструктор с DI
     * @param request HTTP запрос (DI через конструктор)
     */
    explicit SupportedHttpRequest(const drogon::HttpRequestPtr& request)
        : request_(request) {}

    /**
     * @brief Проверить, поддерживается ли HTTP метод
     * @return true для GET и HEAD, false для всех остальных
     */
    bool MethodIsSupported() override {
        auto method = request_->method();
        return method == drogon::Get || method == drogon::Head;
    }
};

} // namespace smartlinks::impl
