add_library(claude
    ClaudeClient.cpp
    CurlTransport.cpp
    GrammarReference.cpp
)

find_package(CURL REQUIRED)

# System prompts are compiled in, so the binary stays self-contained (and the
# prompts aren't served with resources/). Editing one re-runs CMake.
# embed_prompt(<file in prompts/> <header in claude/> <constant name>)
function(embed_prompt file header constant)
    set(path ${PROJECT_SOURCE_DIR}/prompts/${file})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${path})
    file(READ ${path} PROMPT_TEXT)
    string(STRIP "${PROMPT_TEXT}" PROMPT_TEXT)
    set(PROMPT_FILE_NAME ${file})
    set(PROMPT_CONSTANT ${constant})
    configure_file(Prompt.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/claude/${header} @ONLY)
endfunction()
embed_prompt(define_word.md DefineWordPrompt.h kDefineWordPrompt)
embed_prompt(explain_sentence.md ExplainSentencePrompt.h
    kExplainSentencePrompt)

target_include_directories(claude
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(claude PUBLIC Wt::Wt PRIVATE CURL::libcurl)

# Sends sentences through the app's explain request, for
# tools/explain_eval.py.
add_executable(explain_sentence ExplainCli.cpp)
target_link_libraries(explain_sentence claude)

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
