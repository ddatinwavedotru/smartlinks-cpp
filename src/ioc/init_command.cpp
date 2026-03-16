#include "ioc/init_command.hpp"
#include "ioc/ioc.hpp"
#include "ioc/update_ioc_resolve_dependency_strategy_command.hpp"
#include "ioc/unregister_dependency_command.hpp"
#include "ioc/register_transient_dependency_command.hpp"
#include "ioc/register_scoped_dependency_command.hpp"
#include "ioc/register_singleton_dependency_command.hpp"

namespace smartlinks::ioc {

bool InitCommand::initialized_ = false;
std::mutex InitCommand::init_mutex_;

void InitCommand::Execute() {
    if (initialized_) {
        return;
    }

    std::lock_guard<std::mutex> lock(init_mutex_);
    if (initialized_) {
        return;
    }

    // Инициализировать root scope
    Scope::InitRootScope();
    auto root = Scope::GetRoot();

    // Регистрация базовых зависимостей в root scope

    // IoC.Scope.Current.Set
    root->set("IoC.Scope.Current.Set", [](const Args& args) -> std::any {
        auto scope = std::any_cast<std::shared_ptr<IScopeDict>>(args[0]);
        std::shared_ptr<ICommand> cmd = std::make_shared<SetCurrentScopeCommand>(scope);
        return cmd;
    });

    // IoC.Scope.Current.Clear
    root->set("IoC.Scope.Current.Clear", [](const Args&) -> std::any {
        std::shared_ptr<ICommand> cmd = std::make_shared<ClearCurrentScopeCommand>();
        return cmd;
    });

    // IoC.Scope.Current
    root->set("IoC.Scope.Current", [root](const Args&) -> std::any {
        std::shared_ptr<IScopeDict> current = Scope::GetCurrent();
        return current ? current : root;
    });

    // IoC.Scope.Create.Empty
    root->set("IoC.Scope.Create.Empty", [](const Args&) -> std::any {
        std::shared_ptr<IScopeDict> scope = std::make_shared<ScopeDict>();
        return scope;
    });

    // IoC.Scope.Create
    root->set("IoC.Scope.Create", [](const Args& args) -> std::any {
        if (!args.empty()) {
            auto parent = std::any_cast<std::shared_ptr<IScopeDict>>(args[0]);
            return Scope::CreateScope(parent);
        }
        return Scope::CreateScope();
    });

    // IoC.Register.Transient - создает новый объект при каждом Resolve
    root->set("IoC.Register.Transient", [](const Args& args) -> std::any {
        if (args.size() < 2) {
            throw std::runtime_error("IoC.Register.Transient requires 2 arguments");
        }

        auto dependency = std::any_cast<std::string>(args[0]);

        if (!args[1].has_value()) {
            throw std::runtime_error("IoC.Register.Transient: args[1] is empty");
        }

        DependencyFactory factory;
        try {
            factory = std::any_cast<const DependencyFactory&>(args[1]);
        } catch (const std::bad_any_cast& e) {
            try {
                factory = std::any_cast<DependencyFactory>(args[1]);
            } catch (const std::bad_any_cast& e2) {
                throw std::runtime_error(
                    std::string("Cannot extract DependencyFactory. Got type: ") +
                    args[1].type().name()
                );
            }
        }

        std::shared_ptr<ICommand> cmd = std::make_shared<RegisterTransientDependencyCommand>(dependency, factory);
        return cmd;
    });

    // IoC.Register.Scoped - кэширует в текущем scope
    root->set("IoC.Register.Scoped", [](const Args& args) -> std::any {
        if (args.size() < 2) {
            throw std::runtime_error("IoC.Register.Scoped requires 2 arguments");
        }

        auto dependency = std::any_cast<std::string>(args[0]);

        if (!args[1].has_value()) {
            throw std::runtime_error("IoC.Register.Scoped: args[1] is empty");
        }

        DependencyFactory factory;
        try {
            factory = std::any_cast<const DependencyFactory&>(args[1]);
        } catch (const std::bad_any_cast& e) {
            try {
                factory = std::any_cast<DependencyFactory>(args[1]);
            } catch (const std::bad_any_cast& e2) {
                throw std::runtime_error(
                    std::string("Cannot extract DependencyFactory. Got type: ") +
                    args[1].type().name()
                );
            }
        }

        std::shared_ptr<ICommand> cmd = std::make_shared<RegisterScopedDependencyCommand>(dependency, factory);
        return cmd;
    });

    // IoC.Register.Singleton - создает один раз, кэширует глобально
    root->set("IoC.Register.Singleton", [](const Args& args) -> std::any {
        if (args.size() < 2) {
            throw std::runtime_error("IoC.Register.Singleton requires 2 arguments");
        }

        auto dependency = std::any_cast<std::string>(args[0]);

        if (!args[1].has_value()) {
            throw std::runtime_error("IoC.Register.Singleton: args[1] is empty");
        }

        DependencyFactory factory;
        try {
            factory = std::any_cast<const DependencyFactory&>(args[1]);
        } catch (const std::bad_any_cast& e) {
            try {
                factory = std::any_cast<DependencyFactory>(args[1]);
            } catch (const std::bad_any_cast& e2) {
                throw std::runtime_error(
                    std::string("Cannot extract DependencyFactory. Got type: ") +
                    args[1].type().name()
                );
            }
        }

        std::shared_ptr<ICommand> cmd = std::make_shared<RegisterSingletonDependencyCommand>(dependency, factory);
        return cmd;
    });

    // IoC.Unregister
    root->set("IoC.Unregister", [](const Args& args) -> std::any {
        if (args.empty()) {
            throw std::runtime_error("IoC.Unregister requires 1 argument");
        }

        auto dependency = std::any_cast<std::string>(args[0]);
        std::shared_ptr<ICommand> cmd = std::make_shared<UnregisterDependencyCommand>(dependency);
        return cmd;
    });

    // Обновление глобальной стратегии разрешения зависимостей через команду
    IoC::Resolve<std::shared_ptr<ICommand>>(
        "Update Ioc Resolve Dependency Strategy",
        Args{
            StrategyUpdater([root](ResolveStrategy oldStrategy) -> ResolveStrategy {
                return [root](const std::string& dependency, const Args& args) -> std::any {
                    auto current = Scope::GetCurrent();
                    if (!current) {
                        current = root;
                    }
                    DependencyResolver resolver(current);
                    return resolver.Resolve(dependency, args);
                };
            })
        }
    )->Execute();

    initialized_ = true;
}

} // namespace smartlinks::ioc
