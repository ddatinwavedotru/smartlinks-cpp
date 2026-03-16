#pragma once

#include <string>
#include <any>
#include <map>
#include <memory>

namespace smartlinks::dsl {

/**
 * @brief Интерфейс контекста для интерпретации DSL выражений
 *
 * Хранит произвольные данные в формате ключ-значение.
 * Плагины используют адаптеры для извлечения типизированных данных.
 *
 * Паттерн Interpreter: Context хранит глобальные переменные для интерпретации.
 */
class IContext {
public:
    virtual ~IContext() = default;

    /**
     * @brief Получить значение по ключу
     * @param key Ключ
     * @return std::any с значением, или пустой std::any если ключ не найден
     */
    virtual std::any get(const std::string& key) const = 0;

    /**
     * @brief Установить значение по ключу
     * @param key Ключ
     * @param value Значение
     */
    virtual void set(const std::string& key, std::any value) = 0;
};

/**
 * @brief Реализация контекста с хранилищем на основе std::map
 */
class Context : public IContext {
private:
    std::map<std::string, std::any> storage_;

public:
    Context() = default;

    std::any get(const std::string& key) const override;
    void set(const std::string& key, std::any value) override;
};

} // namespace smartlinks::dsl
