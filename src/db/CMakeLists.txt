add_library(db
    DatabaseHandler.cpp
    WordRepository.cpp
    TextRepository.cpp
    WordTextRepository.cpp
    SentenceRepository.cpp
    WordSentenceRepository.cpp
    DictionaryRepository.cpp
    WordFormRepository.cpp
    ExpressionWordRepository.cpp
    ClaudeUsageRepository.cpp
)

include(FetchContent)
FetchContent_Declare(
  vsqlitepp
  GIT_REPOSITORY https://github.com/vinzenz/vsqlite--
  GIT_TAG v${VSQLITEPP_VERSION}
)
set(VSQLITE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VSQLITE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(vsqlitepp)

find_package(SQLite3 REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc)

target_link_libraries(db PUBLIC utils vsqlite::vsqlitepp ${ICU_LIBRARIES})


target_include_directories(db
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${vsqlitepp_SOURCE_DIR}/include
        ${ICU_INCLUDE_DIRECTORIES}
)

# Add test executable
add_executable(db_utils_test
    tests/UtilsTest.cpp
)
target_link_libraries(db_utils_test
    gtest_main
    db
)

add_executable(db_handler_test
    tests/DatabaseHandlerTest.cpp
)
target_link_libraries(db_handler_test
    gtest_main
    db
    vsqlite::vsqlitepp
)

add_executable(word_repository_test
    tests/WordRepositoryTest.cpp
)
target_link_libraries(word_repository_test
    gtest_main
    db
    vsqlite::vsqlitepp
)

add_executable(text_repository_test
    tests/TextRepositoryTest.cpp
)
target_link_libraries(text_repository_test
    gtest_main
    db
    vsqlite::vsqlitepp
)

add_executable(word_text_repository_test
    tests/WordTextRepositoryTest.cpp
)
target_link_libraries(word_text_repository_test
    gtest_main
    db
    vsqlite::vsqlitepp
)

add_executable(savepoint_test
    tests/SavepointTest.cpp
)
target_link_libraries(savepoint_test
    gtest_main
    db
    vsqlite::vsqlitepp
)

add_executable(sentence_repository_test
    tests/SentenceRepositoryTest.cpp
)
target_link_libraries(sentence_repository_test
    gtest_main
    db
    vsqlite::vsqlitepp
)

add_executable(dictionary_repository_test
    tests/DictionaryRepositoryTest.cpp
)
target_link_libraries(dictionary_repository_test
    gtest_main
    db
    vsqlite::vsqlitepp
)

add_executable(claude_repositories_test
    tests/ClaudeRepositoriesTest.cpp
)
target_link_libraries(claude_repositories_test
    gtest_main
    db
    vsqlite::vsqlitepp
)

# Register tests
include(GoogleTest)
gtest_discover_tests(db_utils_test)
gtest_discover_tests(db_handler_test)
gtest_discover_tests(word_repository_test)
gtest_discover_tests(text_repository_test)
gtest_discover_tests(word_text_repository_test)
gtest_discover_tests(savepoint_test)
gtest_discover_tests(sentence_repository_test)
gtest_discover_tests(dictionary_repository_test)
gtest_discover_tests(claude_repositories_test)
