cmake_minimum_required(VERSION 3.21)
project(LexiCorp LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Global C++ settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(Boost REQUIRED COMPONENTS
    system
    thread
    filesystem
    program_options
)
find_package(Wt REQUIRED)
find_package(Threads REQUIRED)

set(VSQLITEPP_VERSION 0.4.0)

# Enable testing and download gtest
enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

FetchContent_MakeAvailable(googletest)

# Build src/
add_subdirectory(src)
