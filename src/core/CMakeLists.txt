# Core Library

set(CORE_SOURCES
    partition_table.cpp
    mbr.cpp
    gpt.cpp
    disk_io.cpp
    partition.cpp
    utils.cpp
    operation.cpp
    filesystem.cpp
    clone.cpp
    boot.cpp
    platform.cpp
    fat32_impl.cpp
    fat32_create.cpp
    fat32_fat.cpp
    fat32_root.cpp
    fat32_check.cpp
    fat32_resize.cpp
    ext4_impl.cpp
    ext4_boot.cpp
    ext4_root.cpp
    ext4_format.cpp
    ext4_journal.cpp
    ext4_check.cpp
    ext4_resize.cpp
    ntfs_impl.cpp
    ntfs_boot.cpp
    ntfs_format.cpp
    ntfs_check.cpp
    ntfs_resize.cpp
    exfat_impl.cpp
    exfat_boot.cpp
    exfat_format.cpp
    exfat_check.cpp
    exfat_resize.cpp
    encryption.cpp
    lvm.cpp
    raid.cpp
)

# Headers are automatically found, just add them for IDE support
set(CORE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/opm/partition_table.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/disk_io.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/partition.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/utils.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/types.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/exceptions.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/operation.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/filesystem.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/clone.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/boot.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/platform.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/raid.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/lvm.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/fat32_impl.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/ext4_impl.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/ntfs_impl.hpp
    ${CMAKE_SOURCE_DIR}/include/opm/exfat_impl.hpp
)

add_library(opm-core STATIC ${CORE_SOURCES})

target_include_directories(opm-core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(opm-core
    PUBLIC
        Threads::Threads
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(opm-core PUBLIC blkid)
endif()

# Set library properties
set_target_properties(opm-core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
