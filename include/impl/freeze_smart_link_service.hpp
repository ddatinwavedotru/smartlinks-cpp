#pragma once

#include "domain/i_freeze_smart_link_service.hpp"
#include "domain/i_statable_smart_link_repository.hpp"
#include <memory>

namespace smartlinks::impl {

/**
 * @brief Реализация IFreezeSmartLinkService
 *
 * Проверяет, должна ли SmartLink вернуть статус 422 Unprocessable Entity.
 * Аналог ImplFreezeSmartLinkService из C# версии.
 */
class FreezeSmartLinkService : public domain::IFreezeSmartLinkService {
private:
    std::shared_ptr<domain::IStatableSmartLinkRepository> repository_;

public:
    /**
     * @brief Конструктор с DI
     * @param repository Repository состояния SmartLink (DI через конструктор)
     */
    explicit FreezeSmartLinkService(
        std::shared_ptr<domain::IStatableSmartLinkRepository> repository
    ) : repository_(std::move(repository)) {}

    /**
     * @brief Проверить, заморожена ли SmartLink
     * @return true если state == "freezed"
     *
     * Логика:
     * 1. Прочитать SmartLink из репозитория
     * 2. Проверить, что state == "freezed"
     * 3. Вернуть true если заморожена, false иначе
     */
    bool ShouldSmartLinkBeFreezed() override {
        auto smart_link_opt = repository_->Read();

        if (!smart_link_opt.has_value()) {
            return false;
        }

        auto smart_link = smart_link_opt.value();
        return smart_link->GetState() == "freezed";
    }
};

} // namespace smartlinks::impl
