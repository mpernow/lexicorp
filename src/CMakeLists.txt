add_subdirectory(pages)
add_subdirectory(db)

add_executable(LexiCorp
    main.cpp
)

target_link_libraries(LexiCorp PRIVATE
    pages
    db
    vsqlite::vsqlitepp
    Wt::Wt
    Wt::HTTP
    Boost::system
    Boost::thread
    Boost::filesystem
    Boost::program_options
    Threads::Threads
)

# Copy resources
add_custom_command(TARGET LexiCorp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:LexiCorp>/resources
    COMMENT "Copying resources to output directory"
)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(RUN_PORT 9090)
else()
    set(RUN_PORT 8080)
endif()

add_custom_target(run
    COMMAND $<TARGET_FILE:LexiCorp>
            --docroot ${PROJECT_SOURCE_DIR}
            --http-address 0.0.0.0
            --http-port ${RUN_PORT}
    DEPENDS main
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
)
