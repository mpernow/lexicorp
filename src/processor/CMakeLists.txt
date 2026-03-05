add_library(processor
    TextProcessor.cpp
)

find_package(ICU REQUIRED COMPONENTS uc i18n data)

target_include_directories(processor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${ICU_INCLUDE_DIRS}
)

target_link_libraries(processor PUBLIC vsqlite::vsqlitepp ${ICU_LIBRARIES})


# Add test executable
add_executable(text_processor_test
    tests/TextProcessorTest.cpp
)
target_link_libraries(text_processor_test
    gtest_main
    processor
)

# Register tests
include(GoogleTest)
gtest_discover_tests(text_processor_test)
