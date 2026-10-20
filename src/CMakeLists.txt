add_subdirectory(pages)
add_subdirectory(db)
add_subdirectory(processor)
add_subdirectory(utils)
add_subdirectory(claude)

add_executable(LexiCorp
    main.cpp
)

target_link_libraries(LexiCorp PRIVATE
    pages
    claude
    db
    processor
    utils
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

# Wt's own resources (Bootstrap theme CSS/JS etc.). The docroot's resources/
# folder only holds our style.css, so point Wt at its installed copy.
find_path(WT_RESOURCES_DIR
    NAMES themes/bootstrap/5/main.css
    PATHS ${Wt_DIR}/../../../share/Wt/resources
          /usr/local/share/Wt/resources
          /usr/share/Wt/resources
    NO_DEFAULT_PATH
)
if(NOT WT_RESOURCES_DIR)
    message(WARNING "Wt resources folder not found; the theme will not load. "
                    "Set WT_RESOURCES_DIR to <wt-prefix>/share/Wt/resources.")
endif()

# Only /resources is served as static files: everything else goes to the app,
# so Wt can use clean URLs (/list instead of /?_=/list) and the rest of the
# docroot (lexiCorp.db, sources) is not downloadable.
add_custom_target(run
    COMMAND $<TARGET_FILE:LexiCorp>
            --docroot "${PROJECT_SOURCE_DIR}$<SEMICOLON>/resources"
            --resources-dir ${WT_RESOURCES_DIR}
            --http-address 0.0.0.0
            --http-port ${RUN_PORT}
    DEPENDS main
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
)
