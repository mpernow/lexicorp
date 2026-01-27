cmake_minimum_required(VERSION 3.20)
project(LexiCorp LANGUAGES CXX)

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

# Build src/
add_subdirectory(src)
