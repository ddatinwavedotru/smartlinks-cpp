# Authorized плагин
add_library(authorized_plugin SHARED
    src/plugin_entry.cpp
    src/context_2_authorized_accessible.cpp
)

target_include_directories(authorized_plugin PRIVATE
    include
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(authorized_plugin PRIVATE
    smartlinks_lib
)

# Установить плагин в директорию plugins/
install(TARGETS authorized_plugin
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/plugins
)

# Копировать плагин в build/plugins/ сразу после сборки
add_custom_command(TARGET authorized_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:authorized_plugin> ${CMAKE_BINARY_DIR}/plugins/
)
