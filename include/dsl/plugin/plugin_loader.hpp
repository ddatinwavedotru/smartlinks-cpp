#pragma once

#include <vector>
#include <string>

namespace smartlinks::dsl::plugin {

/**
 * @brief Загрузчик плагинов (.so библиотек)
 *
 * Использует dlopen/dlsym для динамической загрузки плагинов.
 * При загрузке вызываются конструкторы с __attribute__((constructor)),
 * которые регистрируют парсеры через IoC команды.
 */
class PluginLoader {
private:
    std::vector<void*> loaded_handles_;  // dlopen handles

public:
    PluginLoader() = default;

    /**
     * @brief Загрузить все плагины из директории
     *
     * Ищет файлы *.so в директории и загружает их.
     *
     * @param plugin_dir Путь к директории с плагинами
     */
    void LoadPluginsFromDirectory(const std::string& plugin_dir);

    /**
     * @brief Загрузить конкретный плагин
     *
     * Вызывает dlopen() для загрузки .so библиотеки.
     * Конструкторы с __attribute__((constructor)) вызовутся автоматически.
     *
     * @param plugin_path Полный путь к .so файлу
     * @throws std::runtime_error если загрузка не удалась
     */
    void LoadPlugin(const std::string& plugin_path);

    /**
     * @brief Выгрузить все плагины
     *
     * Вызывает dlclose() для каждого загруженного плагина.
     * Деструкторы с __attribute__((destructor)) вызовутся автоматически.
     */
    void UnloadAll();

    /**
     * @brief Деструктор - выгружает все плагины
     */
    ~PluginLoader();
};

} // namespace smartlinks::dsl::plugin
