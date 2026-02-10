cmake_minimum_required(VERSION 3.16)
project(OpenPartitionManager VERSION 0.1.0 LANGUAGES CXX C)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_GUI "Build GUI application" OFF)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers in debug builds" ON)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-fstack-protector-strong -D_FORTIFY_SOURCE=2)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# Sanitizers for debug builds
if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Find packages
find_package(Threads REQUIRED)

# Core library
add_subdirectory(src/core)

# CLI application
add_subdirectory(src/cli)

# GUI application (optional)
if(BUILD_GUI)
    add_subdirectory(src/gui)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install targets
install(TARGETS opm opm-core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# CPack configuration
set(CPACK_PACKAGE_NAME "OpenPartitionManager")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open Source Partition Management Tool")
set(CPACK_PACKAGE_VENDOR "OpenPartitionManager Contributors")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)
