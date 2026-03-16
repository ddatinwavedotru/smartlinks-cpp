#pragma once

#include "command.hpp"
#include "scope.hpp"

namespace smartlinks::ioc {

class SetCurrentScopeCommand : public ICommand {
private:
    std::shared_ptr<IScopeDict> scope_;

public:
    explicit SetCurrentScopeCommand(std::shared_ptr<IScopeDict> scope)
        : scope_(scope) {}

    void Execute() override {
        Scope::SetCurrent(scope_);
    }
};

} // namespace smartlinks::ioc
