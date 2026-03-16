#pragma once

#include <string>
#include <functional>
#include <vector>
#include <any>
#include <memory>

namespace smartlinks::ioc {

// Аналог object[] в C#
using Args = std::vector<std::any>;

// Аналог Func<string, object[], object>
using ResolveStrategy = std::function<std::any(const std::string&, const Args&)>;

class IoC {
private:
    static ResolveStrategy strategy_;

public:
    // Разрешение зависимости
    template<typename T>
    static T Resolve(const std::string& dependency, const Args& args = {}) {
        std::any result = strategy_(dependency, args);
        return std::any_cast<T>(result);
    }

    // Для внутреннего использования
    friend class UpdateIocResolveDependencyStrategyCommand;
    friend class InitCommand;
};

} // namespace smartlinks::ioc
