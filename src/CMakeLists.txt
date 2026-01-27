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
