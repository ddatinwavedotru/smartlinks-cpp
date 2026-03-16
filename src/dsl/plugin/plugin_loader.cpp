#include "dsl/plugin/plugin_loader.hpp"
#include <dlfcn.h>
#include <stdexcept>
#include <filesystem>
#include <iostream>

namespace smartlinks::dsl::plugin {

void PluginLoader::LoadPluginsFromDirectory(const std::string& plugin_dir) {
    namespace fs = std::filesystem;

    if (!fs::exists(plugin_dir) || !fs::is_directory(plugin_dir)) {
        std::cerr << "Plugin directory not found: " << plugin_dir << std::endl;
        return;
    }

    // Поиск всех .so файлов в директории
    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        if (entry.is_regular_file()) {
            auto path = entry.path();

            if (path.extension() == ".so") {
                try {
                    LoadPlugin(path.string());
                    std::cout << "Loaded plugin: " << path.filename() << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Failed to load plugin " << path.filename()
                              << ": " << e.what() << std::endl;
                }
            }
        }
    }
}

void PluginLoader::LoadPlugin(const std::string& plugin_path) {
    // Загрузить библиотеку
    // RTLD_NOW - разрешить все символы сразу
    // RTLD_GLOBAL - сделать символы доступными для других библиотек
    void* handle = dlopen(plugin_path.c_str(), RTLD_NOW | RTLD_GLOBAL);

    if (!handle) {
        throw std::runtime_error(
            std::string("Failed to load plugin: ") + dlerror()
        );
    }

    loaded_handles_.push_back(handle);

    // Конструкторы с __attribute__((constructor)) вызовутся автоматически
    // Они зарегистрируют парсеры через IoC.Resolve("Parser.Register", ...)
}

void PluginLoader::UnloadAll() {
    for (auto handle : loaded_handles_) {
        // Деструкторы с __attribute__((destructor)) вызовутся автоматически
        // Они дерегистрируют парсеры через IoC.Resolve("Parser.Unregister", ...)
        dlclose(handle);
    }

    loaded_handles_.clear();
}

PluginLoader::~PluginLoader() {
    UnloadAll();
}

} // namespace smartlinks::dsl::plugin
