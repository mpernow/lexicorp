add_library(processor
    TextProcessor.cpp
)

target_include_directories(processor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(processor PRIVATE db)
