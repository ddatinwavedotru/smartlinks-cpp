#pragma once

#include "scope.hpp"
#include <stdexcept>

namespace smartlinks::ioc {

class DependencyResolver {
private:
    std::shared_ptr<IScopeDict> scope_;

public:
    explicit DependencyResolver(std::shared_ptr<IScopeDict> scope)
        : scope_(scope) {}

    std::any Resolve(const std::string& dependency, const Args& args) {
        auto current = scope_;

        while (current) {
            auto factory_opt = current->get(dependency);
            if (factory_opt != std::nullopt) {
                return factory_opt.value()(args);
            }

            // Поиск родительского скоупа
            auto parent_factory_opt = current->get("IoC.Scope.Parent");
            if (parent_factory_opt != std::nullopt) {
                current = std::any_cast<std::shared_ptr<IScopeDict>>(
                    parent_factory_opt.value()({})
                );
            } else {
                break;
            }
        }

        throw std::runtime_error("Dependency not found: " + dependency);
    }
};

} // namespace smartlinks::ioc
