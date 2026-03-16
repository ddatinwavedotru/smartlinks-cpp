# GoogleTest для всех тестов
find_package(GTest REQUIRED)

# Unit тесты для IoC
add_executable(ioc_tests
    unit/ioc_tests.cpp
)
target_link_libraries(ioc_tests PRIVATE
    smartlinks_lib
    GTest::gtest
    GTest::gtest_main
)
add_test(NAME IoC_Tests COMMAND ioc_tests)

# Интеграционные тесты SmartLinks (BDD-style с GTest)
add_executable(smartlinks_tests
    bdd/smartlinks_tests.cpp
)

target_include_directories(smartlinks_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(smartlinks_tests PRIVATE
    smartlinks_lib
    GTest::gtest
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
    Drogon::Drogon
)

add_test(NAME SmartLinks_Integration_Tests COMMAND smartlinks_tests)

# Интеграционные DSL тесты SmartLinks (отдельный набор для DSL режима)
add_executable(smartlinks_dsl_tests
    bdd/smartlinks_dsl_tests.cpp
)

target_include_directories(smartlinks_dsl_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(smartlinks_dsl_tests PRIVATE
    smartlinks_lib
    GTest::gtest
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
    Drogon::Drogon
)

add_test(NAME SmartLinks_DSL_Integration_Tests COMMAND smartlinks_dsl_tests)
