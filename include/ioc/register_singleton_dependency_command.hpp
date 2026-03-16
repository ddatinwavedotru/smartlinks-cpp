#pragma once

#include "command.hpp"
#include "scope.hpp"
#include "ioc.hpp"
#include <mutex>

namespace smartlinks::ioc {

// Команда регистрации Singleton зависимости
// Singleton: один экземпляр на всё приложение
// Инициализируется при первом Resolve, thread-safe
class RegisterSingletonDependencyCommand : public ICommand {
private:
    std::string dependency_;
    DependencyFactory user_factory_;

public:
    RegisterSingletonDependencyCommand(
        const std::string& dependency,
        const DependencyFactory& user_factory
    ) : dependency_(dependency), user_factory_(user_factory) {}

    void Execute() override {
        auto current_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>(
            "IoC.Scope.Current"
        );

        // Для Singleton: оборачиваем пользовательскую фабрику в lambda с captured shared_ptr
        DependencyFactory user_factory_copy = user_factory_;  // Захватываем фабрику

        // Создаём shared_ptr для хранения экземпляра и once_flag (unique per dependency)
        auto instance_holder = std::make_shared<std::any>();
        auto init_flag_holder = std::make_shared<std::once_flag>();

        DependencyFactory singleton_factory = [user_factory_copy, instance_holder, init_flag_holder](const Args& args) -> std::any {
            // Thread-safe инициализация через std::call_once
            std::call_once(*init_flag_holder, [&args, user_factory_copy, instance_holder]() {
                *instance_holder = user_factory_copy(args);
            });

            return *instance_holder;
        };

        current_scope->set(dependency_, singleton_factory);
    }
};

} // namespace smartlinks::ioc
