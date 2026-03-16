#pragma once

#include "command.hpp"
#include "scope.hpp"
#include "dependency_resolver.hpp"
#include "register_dependency_command.hpp"
#include "set_current_scope_command.hpp"
#include "clear_current_scope_command.hpp"
#include <mutex>

namespace smartlinks::ioc {

class InitCommand : public ICommand {
private:
    static bool initialized_;
    static std::mutex init_mutex_;

public:
    void Execute() override;
};

} // namespace smartlinks::ioc
