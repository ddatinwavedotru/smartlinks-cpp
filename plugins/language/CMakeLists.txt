# Language плагин
add_library(language_plugin SHARED
    src/plugin_entry.cpp
    src/context_2_language_accessible.cpp
)

target_include_directories(language_plugin PRIVATE
    include
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(language_plugin PRIVATE
    smartlinks_lib
)

install(TARGETS language_plugin
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/plugins
)

add_custom_command(TARGET language_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:language_plugin> ${CMAKE_BINARY_DIR}/plugins/
)
