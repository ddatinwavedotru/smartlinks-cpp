#pragma once

#include "command.hpp"
#include "scope.hpp"
#include "ioc.hpp"

namespace smartlinks::ioc {

// Команда регистрации Scoped зависимости
// Scoped: один экземпляр на scope (HTTP request)
// Кэшируется в текущем scope при первом Resolve
class RegisterScopedDependencyCommand : public ICommand {
private:
    std::string dependency_;
    DependencyFactory user_factory_;

public:
    RegisterScopedDependencyCommand(
        const std::string& dependency,
        const DependencyFactory& user_factory
    ) : dependency_(dependency), user_factory_(user_factory) {}

    void Execute() override {
        auto current_scope = IoC::Resolve<std::shared_ptr<IScopeDict>>(
            "IoC.Scope.Current"
        );

        // Для Scoped: оборачиваем пользовательскую фабрику в lambda с кэшированием
        std::string cache_key = dependency_;  // Захватываем по значению
        DependencyFactory user_factory_copy = user_factory_;  // Захватываем фабрику

        DependencyFactory scoped_factory = [cache_key, user_factory_copy](const Args& args) -> std::any {
            // 1. Получить текущий scope
            auto scope = IoC::Resolve<std::shared_ptr<IScopeDict>>("IoC.Scope.Current");

            // 2. Проверить кэш в текущем scope
            auto cached = scope->get_cached(cache_key);
            if (cached.has_value()) {
                return cached.value();
            }

            // 3. Создать новый экземпляр через пользовательскую фабрику
            std::any instance = user_factory_copy(args);

            // 4. Сохранить в кэше текущего scope
            scope->cache(cache_key, instance);

            return instance;
        };

        current_scope->set(dependency_, scoped_factory);
    }
};

} // namespace smartlinks::ioc
