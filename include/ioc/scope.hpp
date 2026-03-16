#pragma once

#include "ioc.hpp"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace smartlinks::ioc {

using DependencyFactory = std::function<std::any(const Args&)>;

class IScopeDict {
public:
    virtual ~IScopeDict() = default;
    virtual std::optional<DependencyFactory> get(const std::string& key) const = 0;
    virtual void set(const std::string& key, DependencyFactory value) = 0;
    virtual bool contains(const std::string& key) const = 0;
    virtual bool empty() const = 0;

    // Кэш для scoped экземпляров
    virtual std::optional<std::any> get_cached(const std::string& key) const = 0;
    virtual void cache(const std::string& key, std::any value) = 0;
    virtual void uncache(const std::string& key) = 0;
};

class ScopeDict : public IScopeDict {
protected:
    std::map<std::string, DependencyFactory> data_;
    std::map<std::string, std::any> cache_;  // Кэш scoped экземпляров

public:
    std::optional<DependencyFactory> get(const std::string& key) const override {
        auto it = data_.find(key);
        if (it == data_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void set(const std::string& key, DependencyFactory value) override {
        data_[key] = std::move(value);
    }

    bool contains(const std::string& key) const override {
        return data_.find(key) != data_.end();
    }

    bool empty() const override {
        return data_.empty();
    }

    std::optional<std::any> get_cached(const std::string& key) const override {
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void cache(const std::string& key, std::any value) override {
        cache_[key] = std::move(value);
    }

    void uncache(const std::string& key) override {
        cache_.erase(key);
    }
};

class ConcurrentScopeDict : public ScopeDict {
private:
    mutable std::mutex mutex_;

public:
    std::optional<DependencyFactory> get(const std::string& key) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = data_.find(key);
        if (it == data_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void set(const std::string& key, DependencyFactory value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[key] = std::move(value);
    }

    bool contains(const std::string& key) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.find(key) != data_.end();
    }

    bool empty() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return data_.empty();
    }

    std::optional<std::any> get_cached(const std::string& key) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void cache(const std::string& key, std::any value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[key] = std::move(value);
    }

    void uncache(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.erase(key);
    }
};

class Scope {
private:
    // ThreadLocal для текущего скоупа (C++11: thread_local)
    static thread_local std::shared_ptr<IScopeDict> current_scope_;
    static std::shared_ptr<IScopeDict> root_scope_;

public:
    static void InitRootScope();
    static std::shared_ptr<IScopeDict> GetCurrent();
    static void SetCurrent(std::shared_ptr<IScopeDict> scope);
    static void ClearCurrent();
    static std::shared_ptr<IScopeDict> CreateScope(
        std::shared_ptr<IScopeDict> parent = nullptr
    );
    static std::shared_ptr<IScopeDict> GetRoot();
};

} // namespace smartlinks::ioc
