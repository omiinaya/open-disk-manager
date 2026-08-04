# Integration Tests

# These tests require actual disk access
# Run with care - they may modify disk structures

add_executable(test_disk_detection
    test_disk_detection.cpp
)

target_include_directories(test_disk_detection
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_disk_detection
    PRIVATE
        opm-core
)

# Add as test with special label
add_test(NAME integration_disk_detection 
    COMMAND test_disk_detection
)
set_tests_properties(integration_disk_detection PROPERTIES
    LABELS "integration"
)

# CLI end-to-end test on a virtual disk image (no root required)
find_program(BASH_EXECUTABLE bash REQUIRED)
add_test(NAME integration_cli_e2e
    COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cli_e2e.sh
            $<TARGET_FILE:opm>
)
set_tests_properties(integration_cli_e2e PROPERTIES
    LABELS "integration"
)
