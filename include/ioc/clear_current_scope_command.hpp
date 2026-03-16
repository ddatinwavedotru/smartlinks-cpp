#pragma once

#include "command.hpp"
#include "scope.hpp"

namespace smartlinks::ioc {

class ClearCurrentScopeCommand : public ICommand {
public:
    void Execute() override {
        Scope::ClearCurrent();
    }
};

} // namespace smartlinks::ioc
