#pragma once

#include "domain/i_statable_smart_link.hpp"
#include <string>

namespace smartlinks::impl {

/**
 * @brief Реализация IStatableSmartLink
 *
 * Хранит состояние SmartLink, извлечённое из BSON документа.
 * Аналог ImplStatableSmartLink из C# версии.
 */
class StatableSmartLink : public domain::IStatableSmartLink {
private:
    std::string state_;

public:
    /**
     * @brief Конструктор
     * @param state Состояние SmartLink ("active", "deleted", "freezed")
     */
    explicit StatableSmartLink(std::string state)
        : state_(std::move(state)) {}

    std::string GetState() const override {
        return state_;
    }
};

} // namespace smartlinks::impl
