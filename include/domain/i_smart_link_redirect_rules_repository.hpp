#pragma once

#include "i_redirect_rule.hpp"
#include <optional>
#include <vector>
#include <memory>

namespace smartlinks::domain {

/**
 * @brief Интерфейс репозитория правил редиректа
 *
 * Аналог ISmartLinkRedirectRulesRepository из C# версии.
 * Читает массив правил из MongoDB документа и преобразует в IRedirectRule объекты.
 */
class ISmartLinkRedirectRulesRepository {
public:
    virtual ~ISmartLinkRedirectRulesRepository() = default;

    /**
     * @brief Прочитать список правил редиректа для SmartLink
     * @return Вектор правил или std::nullopt если SmartLink не найдена
     *
     * Логика:
     * 1. Получить BSON документ из IMongoDbRepository
     * 2. Извлечь массив "rules"
     * 3. Преобразовать каждый элемент в IRedirectRule
     * 4. Вернуть вектор правил
     */
    virtual std::optional<std::vector<std::shared_ptr<IRedirectRule>>> Read() = 0;
};

} // namespace smartlinks::domain
