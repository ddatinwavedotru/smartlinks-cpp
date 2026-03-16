#include "dsl/context.hpp"

namespace smartlinks::dsl {

std::any Context::get(const std::string& key) const {
    auto it = storage_.find(key);
    if (it != storage_.end()) {
        return it->second;
    }
    return std::any{};
}

void Context::set(const std::string& key, std::any value) {
    storage_[key] = std::move(value);
}

} // namespace smartlinks::dsl
