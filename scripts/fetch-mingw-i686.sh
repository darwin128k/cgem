#!/bin/sh
# Download a local i686 MinGW toolchain into .toolchain/apt (no sudo).
# Debian/Ubuntu only; uses apt-get download + dpkg-deb -x.

set -e

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
apt_dir="$root/.toolchain/apt"
mingw_gcc="$apt_dir/root/usr/bin/i686-w64-mingw32-gcc-win32"

if [ -x "$mingw_gcc" ] && [ "${1:-}" != "--force" ]; then
    echo "MinGW i686 toolchain already present: $mingw_gcc"
    exit 0
fi

if ! command -v apt-get >/dev/null 2>&1; then
    echo "fetch-mingw-i686: apt-get is required (Debian/Ubuntu)." >&2
    exit 1
fi

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "fetch-mingw-i686: dpkg-deb is required." >&2
    exit 1
fi

packages="
    binutils-mingw-w64-i686
    gcc-mingw-w64-base
    gcc-mingw-w64-i686-win32
    gcc-mingw-w64-i686-win32-runtime
    mingw-w64-common
    mingw-w64-i686-dev
"

mkdir -p "$apt_dir"
cd "$apt_dir"

echo "Downloading MinGW i686 packages..."
apt-get download $packages

echo "Extracting into $apt_dir/root ..."
rm -rf root
mkdir root
for deb in *.deb; do
    dpkg-deb -x "$deb" root
done

if [ ! -x "$mingw_gcc" ]; then
    echo "fetch-mingw-i686: $mingw_gcc not found after extract" >&2
    exit 1
fi

echo "Ready: $mingw_gcc"
