add_library(db
    DatabaseHandler.cpp
)

include(FetchContent)
FetchContent_Declare(
  vsqlitepp
  GIT_REPOSITORY https://github.com/vinzenz/vsqlite--
  GIT_TAG v${VSQLITEPP_VERSION} # or a commit hash
)
set(VSQLITE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VSQLITE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(vsqlitepp)

target_link_libraries(db PRIVATE vsqlite::vsqlitepp)

target_include_directories(db
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add test executable
add_executable(db_utils_test
    tests/UtilsTest.cpp
)
target_link_libraries(db_utils_test
    gtest_main
    db
)

# Register tests
include(GoogleTest)
gtest_discover_tests(db_utils_test)

