#pragma once

#include "command.hpp"
#include "scope.hpp"
#include "ioc.hpp"

namespace smartlinks::ioc {

// Команда регистрации Transient зависимости
// Transient: создается новый экземпляр при каждом Resolve
class RegisterTransientDependencyCommand : public ICommand {
private:
    std::string dependency_;
    DependencyFactory factory_;

public:
    RegisterTransientDependencyCommand(
        const std::string& dependency,
        const DependencyFactory& factory
    ) : dependency_(dependency), factory_(factory) {}

    void Execute() override {
        auto current_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>(
            "IoC.Scope.Current"
        );

        // Для Transient: пользовательская фабрика сохраняется напрямую
        // Каждый Resolve будет вызывать фабрику и создавать новый объект
        current_scope->set(dependency_, factory_);
    }
};

} // namespace smartlinks::ioc
