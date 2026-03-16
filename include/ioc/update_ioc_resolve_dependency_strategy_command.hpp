#pragma once

#include "command.hpp"
#include "ioc.hpp"
#include <functional>

namespace smartlinks::ioc {

// Тип функции-обновителя стратегии: принимает старую стратегию, возвращает новую
using StrategyUpdater = std::function<ResolveStrategy(ResolveStrategy)>;

class UpdateIocResolveDependencyStrategyCommand : public ICommand {
private:
    StrategyUpdater updater_;

public:
    explicit UpdateIocResolveDependencyStrategyCommand(const StrategyUpdater& updater)
        : updater_(updater) {}

    void Execute() override {
        IoC::strategy_ = updater_(IoC::strategy_);
    }
};

} // namespace smartlinks::ioc
