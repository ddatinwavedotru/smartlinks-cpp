#pragma once

#include "command.hpp"
#include "scope.hpp"
#include "ioc.hpp"
#include <stdexcept>

namespace smartlinks::ioc {

/**
 * @brief Команда дерегистрации зависимости из IoC контейнера
 *
 * Заменяет фабрику на "пустую", которая бросает исключение при попытке resolve.
 * Используется для выгрузки плагинов и очистки зависимостей.
 */
class UnregisterDependencyCommand : public ICommand {
private:
    std::string dependency_;

public:
    explicit UnregisterDependencyCommand(const std::string& dependency)
        : dependency_(dependency) {}

    void Execute() override {
        auto current_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>(
            "IoC.Scope.Current"
        );

        // 1. Удалить кэшированный экземпляр (если есть)
        current_scope->uncache(dependency_);

        // 2. Установить "пустую" фабрику, которая бросает исключение
        auto empty_factory = [this](const Args&) -> std::any {
            throw std::runtime_error(
                "Dependency '" + dependency_ + "' has been unregistered"
            );
        };

        current_scope->set(dependency_, empty_factory);
    }
};

} // namespace smartlinks::ioc
