add_library(pages
    InsertTextPage.cpp
    HomePage.cpp
    LexiCorpMain.cpp
    DisplayWordsPage.cpp
    ViewSourcePage.cpp
    Sentences.cpp
    Migration.cpp
    Glosses.cpp
    ClaudeLookup.cpp
    Explanations.cpp
    GrammarSections.cpp
)

target_include_directories(pages
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pages PUBLIC claude db processor utils)

# Add test executable
add_executable(pages_test
    tests/PagesTest.cpp
)
target_link_libraries(pages_test
    gtest_main
    pages
    Wt::Wt
    Wt::Test
)

# Register tests
include(GoogleTest)
gtest_discover_tests(pages_test)
