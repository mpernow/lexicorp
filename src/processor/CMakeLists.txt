add_library(processor
    TextProcessor.cpp
)

target_include_directories(processor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(processor PRIVATE db)


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
