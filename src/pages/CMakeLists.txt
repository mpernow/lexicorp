add_library(pages
    HelloApplication.cpp
)

target_include_directories(pages
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pages PUBLIC db processor)
