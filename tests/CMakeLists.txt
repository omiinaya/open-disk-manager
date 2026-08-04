# Tests

# Find Google Test
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # Fetch Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Test files
set(TEST_SOURCES
    test_main.cpp
    test_mbr.cpp
    test_gpt.cpp
    test_exfat.cpp
    test_clone.cpp
    test_boot.cpp
    test_utils.cpp
    test_filesystem.cpp
    test_operation.cpp
    test_partition_table.cpp
    test_checksums.cpp
    test_gpt_ops.cpp
    test_iso.cpp
    test_encryption.cpp
    test_i18n.cpp
    test_conversion.cpp
    test_swap.cpp
    test_labels.cpp
    test_recovery.cpp
    test_undelete.cpp
    test_security.cpp
)

add_executable(opm_tests ${TEST_SOURCES})

target_include_directories(opm_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(opm_tests PRIVATE
    OPM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(opm_tests
    PRIVATE
        opm-core
        GTest::gtest
        GTest::gtest_main
)

# Add test
add_test(NAME opm_unit_tests COMMAND opm_tests)

# Integration tests
add_subdirectory(integration)
