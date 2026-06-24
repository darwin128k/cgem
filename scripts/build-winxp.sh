#!/bin/sh
set -e

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"

mingw_root="$root/.toolchain/apt/root/usr"
if [ -x "$mingw_root/bin/i686-w64-mingw32-gcc-win32" ]; then
    CGEM_MINGW_ROOT="$mingw_root"
elif command -v i686-w64-mingw32-gcc-win32 >/dev/null 2>&1; then
    CGEM_MINGW_ROOT=/usr
elif command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
    CGEM_MINGW_ROOT=/usr
else
    "$root/scripts/fetch-mingw-i686.sh"
    mingw_root="$root/.toolchain/apt/root/usr"
    if [ ! -x "$mingw_root/bin/i686-w64-mingw32-gcc-win32" ]; then
        echo "Need MinGW cross-compiler. Install system packages:" >&2
        echo "  sudo apt install gcc-mingw-w64-i686" >&2
        exit 1
    fi
    CGEM_MINGW_ROOT="$mingw_root"
fi

if [ -f build/CMakeCache.txt ] &&
    ! grep -q 'i686-w64-mingw32-gcc' build/CMakeCache.txt 2>/dev/null; then
    echo "Removing native CMake cache for Windows XP cross-build..."
    rm -f build/CMakeCache.txt
    rm -rf build/CMakeFiles
fi

cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-xp-i686.cmake \
    -DCGEM_MINGW_ROOT="$CGEM_MINGW_ROOT"
cmake --build build
echo
echo "Built: $root/build/cgem.exe"
file build/cgem.exe 2>/dev/null || true
