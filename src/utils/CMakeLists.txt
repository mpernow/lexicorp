add_library(utils
    INTERFACE
)

target_include_directories(utils
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add test executable
add_executable(language_test
    tests/LanguageTest.cpp
)
target_link_libraries(language_test
    gtest_main
    utils
)

# Register tests
include(GoogleTest)
gtest_discover_tests(language_test)
