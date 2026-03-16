cmake_minimum_required(VERSION 3.20)
project(smartlinks-cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Code coverage
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# Зависимости
find_package(Drogon REQUIRED)
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)
# find_package(Boost REQUIRED COMPONENTS system)

# jwt-cpp может быть установлена в /usr/share/cmake
list(APPEND CMAKE_PREFIX_PATH "/usr/share/cmake")
find_package(jwt-cpp REQUIRED)

# Библиотека
add_library(smartlinks_lib SHARED
    src/ioc/ioc.cpp
    src/ioc/scope.cpp
    src/ioc/dependency_resolver.cpp
    src/ioc/init_command.cpp
    src/ioc/register_dependency_command.cpp
    src/ioc/update_ioc_resolve_dependency_strategy_command.cpp
    src/ioc/set_current_scope_command.cpp
    src/ioc/clear_current_scope_command.cpp
    src/impl/smart_link.cpp
    src/impl/redirect_rule.cpp
    src/impl/redirectable_smart_link.cpp
    src/impl/statable_smart_link.cpp
    src/impl/mongodb_repository.cpp
    src/impl/smart_link_redirect_service.cpp
    src/impl/smart_link_redirect_rules_repository.cpp
    src/impl/statable_smart_link_repository.cpp
    src/impl/freeze_smart_link_service.cpp
    src/impl/supported_http_request.cpp
    src/impl/jwt_validator.cpp
    src/middleware/method_not_allowed_middleware.cpp
    src/middleware/not_found_middleware.cpp
    src/middleware/unprocessable_content_middleware.cpp
    src/middleware/temporary_redirect_middleware.cpp
    src/middleware/drogon_middleware_adapter.cpp
    # DSL компоненты
    src/dsl/context.cpp
    src/dsl/parser/or_expression_parser.cpp
    src/dsl/parser/and_expression_parser.cpp
    src/dsl/parser/primary_expression_parser.cpp
    src/dsl/parser/combined_parser.cpp
    src/dsl/plugin/plugin_loader.cpp
    # DSL интеграция с MongoDB
    src/impl/smart_link_redirect_rules_repository_dsl.cpp
    src/impl/smart_link_redirect_service_dsl.cpp
)

target_include_directories(smartlinks_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/datetime/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/language/include
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/authorized/include
)

# Включить -fPIC для использования в shared libraries (плагинах)
set_target_properties(smartlinks_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(smartlinks_lib PUBLIC
    Drogon::Drogon
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
    jwt-cpp::jwt-cpp
    # Boost::system
    ${CMAKE_DL_LIBS}  # Для dlopen/dlsym (загрузка плагинов)
)

# Executable
add_executable(smartlinks src/main.cpp)
target_link_libraries(smartlinks PRIVATE smartlinks_lib)

# Копируем конфигурацию в build директорию
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Тесты
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Плагины DSL
add_subdirectory(plugins/datetime)
add_subdirectory(plugins/language)
add_subdirectory(plugins/authorized)
