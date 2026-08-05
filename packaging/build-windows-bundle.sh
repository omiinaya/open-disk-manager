#!/usr/bin/env bash
# Build the Windows GUI + CLI and produce a self-contained redistributable
# bundle using windeployqt. Run on Linux with the MinGW cross toolchain.
#
# Prerequisites:
#   x86_64-w64-mingw32-g++ (Debian: g++-mingw-w64-x86-64-posix)
#   Qt 6.x Windows kit (e.g. via aqtinstall: aqt install-qt windows desktop 6.5.3 win64_mingw)
#   Qt 6.x Linux kit for host tools (QT_HOST_PATH)
#   wine (only needed to run windeployqt)
#
# Usage:
#   ./packaging/build-windows-bundle.sh /path/to/qt-win /path/to/qt-host [out-dir]
set -euo pipefail

QT_WIN=${1:?usage: build-windows-bundle.sh <qt-win-prefix> <qt-host-prefix> [out-dir]}
QT_HOST=${2:?usage: build-windows-bundle.sh <qt-win-prefix> <qt-host-prefix> [out-dir]}
OUT_DIR=${3:-$(pwd)/dist-win}

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(mktemp -d /tmp/opm-winbuild.XXXXXX)"
trap 'rm -rf "$BUILD_DIR"' EXIT

NINJA="$(command -v ninja || echo /usr/bin/ninja)"
echo "==> Configuring (MinGW cross, GUI enabled) with ninja: $NINJA"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$NINJA" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_GUI=ON \
  -DBUILD_TESTS=OFF \
  -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/cmake/mingw-toolchain.cmake" \
  -DQT_HOST_PATH="$QT_HOST" \
  -DCMAKE_PREFIX_PATH="$QT_WIN"

echo "==> Building"
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Assembling bundle in $OUT_DIR"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/bin/opm-gui.exe" "$OUT_DIR/"
cp "$BUILD_DIR/src/cli/opm.exe" "$OUT_DIR/"

# MinGW runtime DLLs (stack-protector adds libssp)
GCC_DLL_DIR="$(dirname "$(command -v x86_64-w64-mingw32-g++)")/../lib/gcc/x86_64-w64-mingw32/"*/*posix
cp -u $GCC_DLL_DIR/*.dll "$OUT_DIR/" 2>/dev/null || true

echo "==> Qt runtime DLLs + plugins (manual deploy; windeployqt under Wine is
     unreliable on headless CI without wine32)"
QT_BIN="$QT_WIN/bin"
cp "$QT_BIN"/Qt6Core.dll "$QT_BIN"/Qt6Gui.dll "$QT_BIN"/Qt6Widgets.dll "$OUT_DIR/" 2>/dev/null || true
# MinGW runtime DLLs for the Qt binaries (libgcc, libstdc++, winpthread, libssp)
cp -u /usr/lib/gcc/x86_64-w64-mingw32/*/*posix/*.dll "$OUT_DIR/" 2>/dev/null || true
cp -u /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll "$OUT_DIR/" 2>/dev/null || true
# Platform plugin (Qt6 needs platforms/qwindows.dll)
mkdir -p "$OUT_DIR/platforms"
cp "$QT_BIN/../plugins/platforms/qwindows.dll" "$OUT_DIR/platforms/" 2>/dev/null || true
# Image format plugins (png/jpeg) used by the GUI
mkdir -p "$OUT_DIR/imageformats"
cp "$QT_BIN/../plugins/imageformats/qpng.dll" "$QT_BIN/../plugins/imageformats/qjpeg.dll" \
   "$QT_BIN/../plugins/imageformats/qgif.dll" "$OUT_DIR/imageformats/" 2>/dev/null || true

echo "==> Bundle contents ($(du -sh "$OUT_DIR" | cut -f1))"
ls "$OUT_DIR"
echo "DONE: Windows bundle at $OUT_DIR"
