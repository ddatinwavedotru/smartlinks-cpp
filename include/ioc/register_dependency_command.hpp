#pragma once

#include "command.hpp"
#include "scope.hpp"
#include "ioc.hpp"

namespace smartlinks::ioc {

class RegisterDependencyCommand : public ICommand {
private:
    std::string dependency_;
    DependencyFactory factory_;

public:
    RegisterDependencyCommand(
        const std::string& dependency,
        const DependencyFactory& factory
    ) : dependency_(dependency), factory_(factory) {}

    void Execute() override {
        auto current_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>(
            "IoC.Scope.Current"
        );
        current_scope->set(dependency_, factory_);
    }
};

} // namespace smartlinks::ioc
