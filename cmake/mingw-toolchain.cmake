# Minimal MinGW-w64 cross toolchain for building the Windows targets.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Prefer the -posix variants: on Debian/Ubuntu both win32 and posix mingw
# variants register update-alternatives, and the unsuffixed name may point at
# either one depending on what was installed first. The toolchain needs the
# posix (winpthread) model.
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# Package files: Qt is provided via CMAKE_PREFIX_PATH / Qt6_DIR at configure
# time. To stay relocatable (no hardcoded /home/... path) while keeping the
# sysroot isolation, BOTH lets packages resolve from the sysroot and from
# CMAKE_PREFIX_PATH; the GUI's explicit Qt6_DIR/CMAKE_PREFIX_PATH points at
# the cross Qt kit.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(CMAKE_CXX_FLAGS_INIT "-D__USE_MINGW_ANSI_STDIO=1 -fstack-protector-strong")

# Windows executables typically want the pthread model; MinGW-posix provides it.
set(THREADS_PREFER_PTHREAD_FLAG ON)