# CLI Application

set(CLI_SOURCES
    main.cpp
    commands.cpp
)

add_executable(opm ${CLI_SOURCES})

target_include_directories(opm
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(opm
    PRIVATE
        opm-core
)

# Install
install(TARGETS opm
    RUNTIME DESTINATION bin
)
