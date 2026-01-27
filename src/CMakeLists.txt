add_executable(main
    main.cpp
)

target_link_libraries(main PRIVATE
    Wt::Wt
    Wt::HTTP
    Boost::system
    Boost::thread
    Boost::filesystem
    Boost::program_options
    Threads::Threads
)

target_include_directories(main PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(RUN_PORT 9090)
else()
    set(RUN_PORT 8080)
endif()

add_custom_target(run
    COMMAND $<TARGET_FILE:main>
            --docroot ${PROJECT_SOURCE_DIR}
            --http-address 0.0.0.0
            --http-port ${RUN_PORT}
    DEPENDS main
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
)
