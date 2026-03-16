#pragma once

#include "i_parser.hpp"
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace smartlinks::dsl::parser {

/**
 * @brief Реестр парсеров (хранится в IoC Scope)
 *
 * Хранит два типа парсеров:
 * 1. Core парсеры (AND, OR, скобки) - встроенные, всегда присутствуют
 * 2. Leaf парсеры (datetime, language, authorized) - из плагинов, регистрируются динамически
 */
class ParserRegistry {
private:
    // Ядро парсеров (встроенные, не используются напрямую в текущей архитектуре,
    // т.к. AND/OR обрабатываются Boost.Spirit грамматикой)
    std::vector<std::shared_ptr<IParser>> core_parsers_;

    // Листовые парсеры (плагины) - регистрируются динамически
    std::map<std::string, std::shared_ptr<IParser>> leaf_parsers_;

    // Thread-safety для регистрации/дерегистрации
    mutable std::mutex mutex_;

public:
    ParserRegistry() = default;

    /**
     * @brief Зарегистрировать core парсер
     *
     * Core парсеры встроены в приложение (AND, OR, скобки).
     * На практике они обрабатываются Boost.Spirit грамматикой напрямую.
     */
    void RegisterCoreParser(std::shared_ptr<IParser> parser) {
        std::lock_guard<std::mutex> lock(mutex_);
        core_parsers_.push_back(std::move(parser));
    }

    /**
     * @brief Зарегистрировать leaf парсер (из плагина)
     *
     * Вызывается через команду Parser.Register при загрузке плагина.
     *
     * @param name Имя парсера (совпадает с parser->GetName())
     * @param parser Парсер из плагина
     */
    void RegisterLeafParser(const std::string& name, std::shared_ptr<IParser> parser) {
        std::lock_guard<std::mutex> lock(mutex_);
        leaf_parsers_[name] = std::move(parser);

        // Debug logging
        std::cout << "[ParserRegistry] Registered leaf parser '" << name
                  << "' (total: " << leaf_parsers_.size() << ", this=" << this << ")" << std::endl;
    }

    /**
     * @brief Дерегистрировать leaf парсер
     *
     * Вызывается через команду Parser.Unregister при выгрузке плагина.
     *
     * @param name Имя парсера
     */
    void UnregisterLeafParser(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        leaf_parsers_.erase(name);
    }

    /**
     * @brief Получить все парсеры (core + leaf)
     */
    std::vector<std::shared_ptr<IParser>> GetAllParsers() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::shared_ptr<IParser>> all_parsers;
        all_parsers.reserve(core_parsers_.size() + leaf_parsers_.size());

        // Сначала core
        for (const auto& parser : core_parsers_) {
            all_parsers.push_back(parser);
        }

        // Затем leaf (отсортированные по приоритету)
        std::vector<std::shared_ptr<IParser>> leaf_vec;
        for (const auto& [name, parser] : leaf_parsers_) {
            leaf_vec.push_back(parser);
        }

        // Сортировка по приоритету (убывание)
        std::sort(leaf_vec.begin(), leaf_vec.end(),
            [](const auto& a, const auto& b) {
                return a->GetPriority() > b->GetPriority();
            });

        for (const auto& parser : leaf_vec) {
            all_parsers.push_back(parser);
        }

        return all_parsers;
    }

    /**
     * @brief Получить только core парсеры
     */
    std::vector<std::shared_ptr<IParser>> GetCoreParsers() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return core_parsers_;
    }

    /**
     * @brief Получить только leaf парсеры (отсортированные по приоритету)
     */
    std::vector<std::shared_ptr<IParser>> GetLeafParsers() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::shared_ptr<IParser>> result;
        for (const auto& [name, parser] : leaf_parsers_) {
            result.push_back(parser);
        }

        // Сортировка по приоритету (убывание)
        std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) {
                return a->GetPriority() > b->GetPriority();
            });

        // Debug logging
        std::cout << "[ParserRegistry] GetLeafParsers() returning " << result.size()
                  << " parsers (this=" << this << ")" << std::endl;

        return result;
    }

    /**
     * @brief Проверить, зарегистрирован ли парсер с данным именем
     */
    bool HasParser(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return leaf_parsers_.find(name) != leaf_parsers_.end();
    }
};

} // namespace smartlinks::dsl::parser
