#include "ioc/scope.hpp"

namespace smartlinks::ioc {

// Инициализация статических переменных
thread_local std::shared_ptr<IScopeDict> Scope::current_scope_ = nullptr;
std::shared_ptr<IScopeDict> Scope::root_scope_ = nullptr;

void Scope::InitRootScope() {
    if (!root_scope_) {
        root_scope_ = std::make_shared<ConcurrentScopeDict>();
    }
}

std::shared_ptr<IScopeDict> Scope::GetCurrent() {
    return current_scope_;
}

void Scope::SetCurrent(std::shared_ptr<IScopeDict> scope) {
    current_scope_ = scope;
}

void Scope::ClearCurrent() {
    current_scope_ = nullptr;
}

std::shared_ptr<IScopeDict> Scope::CreateScope(std::shared_ptr<IScopeDict> parent) {
    std::shared_ptr<IScopeDict> new_scope = std::make_shared<ScopeDict>();

    // Установить родительский скоуп
    if (parent) {
        new_scope->set("IoC.Scope.Parent", [parent](const Args&) -> std::any {
            return parent;
        });
    } else {
        // Если родитель не указан, используем текущий скоуп
        auto current = IoC::Resolve<std::shared_ptr<IScopeDict>>(
            "IoC.Scope.Current"
            );
        if (current) {
            new_scope->set("IoC.Scope.Parent", [current](const Args&) -> std::any {
                return current;
            });
        }
    }

    return new_scope;
}

std::shared_ptr<IScopeDict> Scope::GetRoot() {
    return root_scope_;
}

} // namespace smartlinks::ioc
