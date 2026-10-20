add_library(claude
    ClaudeClient.cpp
    CurlTransport.cpp
)

find_package(CURL REQUIRED)

# The system prompt is compiled in, so the binary stays self-contained (and the
# prompt isn't served with resources/). Editing it re-runs CMake.
set(DEFINE_WORD_PROMPT_FILE ${PROJECT_SOURCE_DIR}/prompts/define_word.md)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${DEFINE_WORD_PROMPT_FILE})
file(READ ${DEFINE_WORD_PROMPT_FILE} DEFINE_WORD_PROMPT)
string(STRIP "${DEFINE_WORD_PROMPT}" DEFINE_WORD_PROMPT)
configure_file(DefineWordPrompt.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/claude/DefineWordPrompt.h @ONLY)

target_include_directories(claude
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(claude PUBLIC Wt::Wt PRIVATE CURL::libcurl)

# Add test executable
add_executable(claude_client_test
    tests/ClaudeClientTest.cpp
)
target_include_directories(claude_client_test
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(claude_client_test
    gtest_main
    claude
)

# Register tests
include(GoogleTest)
gtest_discover_tests(claude_client_test)
