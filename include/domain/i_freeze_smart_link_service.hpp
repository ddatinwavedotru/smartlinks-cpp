#pragma once

namespace smartlinks::domain {

/**
 * @brief Интерфейс сервиса для проверки "замороженности" SmartLink
 *
 * Аналог IFreezeSmartLinkService из C# версии.
 * Проверяет, должна ли SmartLink быть заморожена (статус 422).
 */
class IFreezeSmartLinkService {
public:
    virtual ~IFreezeSmartLinkService() = default;

    /**
     * @brief Проверить, должна ли SmartLink вернуть статус 422 Unprocessable Entity
     * @return true если SmartLink заморожена (state == "freezed")
     *
     * Примечание: В текущей реализации возвращает false,
     * так как проверка state="freezed" выполняется в другом месте.
     * Этот интерфейс сохранён для соответствия C# архитектуре.
     */
    virtual bool ShouldSmartLinkBeFreezed() = 0;
};

} // namespace smartlinks::domain
