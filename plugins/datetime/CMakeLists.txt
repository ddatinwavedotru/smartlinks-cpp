# Datetime плагин
add_library(datetime_plugin SHARED
    src/plugin_entry.cpp
    src/context_2_time_point_accessible.cpp
)

target_include_directories(datetime_plugin PRIVATE
    include
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(datetime_plugin PRIVATE
    smartlinks_lib
)

install(TARGETS datetime_plugin
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/plugins
)

add_custom_command(TARGET datetime_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/plugins
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:datetime_plugin> ${CMAKE_BINARY_DIR}/plugins/
)
