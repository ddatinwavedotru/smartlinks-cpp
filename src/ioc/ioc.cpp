#include "ioc/ioc.hpp"
#include "ioc/update_ioc_resolve_dependency_strategy_command.hpp"
#include <stdexcept>
#include <memory>

namespace smartlinks::ioc {

// Инициализация статической переменной стратегии
ResolveStrategy IoC::strategy_ = [](const std::string& dependency, const Args& args) -> std::any {
    if (dependency == "Update Ioc Resolve Dependency Strategy") {
        if (args.empty()) {
            throw std::runtime_error("Update Ioc Resolve Dependency Strategy requires updater function");
        }

        auto updater = std::any_cast<StrategyUpdater>(args[0]);
        std::shared_ptr<ICommand> cmd = std::make_shared<UpdateIocResolveDependencyStrategyCommand>(updater);
        return cmd;
    }

    throw std::runtime_error("Dependency not found: " + dependency);
};

} // namespace smartlinks::ioc
