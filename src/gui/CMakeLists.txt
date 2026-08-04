# GUI Application

# Find Qt6 or Qt5
find_package(Qt6 COMPONENTS Widgets Core Gui)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 COMPONENTS Widgets Core Gui REQUIRED)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# GUI source files
set(GUI_SOURCES
main.cpp
main_window.cpp
disk_tree_widget.cpp
partition_view_widget.cpp
operation_panel_widget.cpp
status_bar_widget.cpp
dialogs/create_partition_dialog.cpp
dialogs/delete_partition_dialog.cpp
dialogs/resize_partition_dialog.cpp
dialogs/format_dialog.cpp
dialogs/clone_dialog.cpp
dialogs/secure_erase_dialog.cpp
dialogs/benchmark_dialog.cpp
dialogs/partition_properties_dialog.cpp
dialogs/preferences_dialog.cpp
dialogs/schedules_dialog.cpp
wizards/wizards.cpp
)

set(GUI_HEADERS
main_window.hpp
disk_tree_widget.hpp
partition_view_widget.hpp
operation_panel_widget.hpp
status_bar_widget.hpp
dialogs/create_partition_dialog.hpp
dialogs/delete_partition_dialog.hpp
dialogs/resize_partition_dialog.hpp
dialogs/format_dialog.hpp
dialogs/clone_dialog.hpp
dialogs/secure_erase_dialog.hpp
dialogs/benchmark_dialog.hpp
dialogs/partition_properties_dialog.hpp
dialogs/preferences_dialog.hpp
dialogs/schedules_dialog.hpp
wizards/wizards.hpp
)

# Create executable
add_executable(opm-gui ${GUI_SOURCES} ${GUI_HEADERS})

# Link libraries
target_link_libraries(opm-gui
    PRIVATE
    opm-core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
)

# Include directories
target_include_directories(opm-gui
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set target properties
set_target_properties(opm-gui PROPERTIES
    VERSION ${PROJECT_VERSION}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Platform-specific settings
if(WIN32)
    set_target_properties(opm-gui PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Install
install(TARGETS opm-gui
    RUNTIME DESTINATION bin
)
