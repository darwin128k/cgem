#!/usr/bin/env bash
# CI/local multi-target build helper.
# Usage:
#   ./scripts/ci-build.sh linux x86_64
#   ./scripts/ci-build.sh windows-xp i686 0x0501
#   ./scripts/ci-build.sh windows-10 x86_64 0x0A00

set -euo pipefail

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"

target="${1:?target (linux | windows-xp | windows-vista | windows-7 | windows-8 | windows-10)}"
arch="${2:?arch (x86_64 | i686)}"
winnt="${3:-}"

case "$target" in
    linux)
        if [[ "$arch" != "x86_64" ]]; then
            echo "Linux CI build supports x86_64 only." >&2
            exit 1
        fi
        build_dir="build/ci-linux-x86_64"
        artifact="dist/cgem-linux-x86_64"
        cmake -B "$build_dir" -G Ninja
        cmake --build "$build_dir"
        mkdir -p dist
        cp "$build_dir/cgem" "$artifact"
        ;;
    windows-*)
        if [[ -z "$winnt" ]]; then
            case "$target" in
                windows-xp) winnt=0x0501 ;;
                windows-vista) winnt=0x0600 ;;
                windows-7) winnt=0x0601 ;;
                windows-8) winnt=0x0602 ;;
                windows-10) winnt=0x0A00 ;;
                *)
                    echo "Unknown Windows target: $target" >&2
                    exit 1
                    ;;
            esac
        fi
        if [[ "$target" == "windows-xp" && "$arch" == "x86_64" ]]; then
            winnt=0x0502
        fi
        build_dir="build/ci-${target}-${arch}"
        artifact="dist/cgem-${target}-${arch}.exe"
        mingw_root="${CGEM_MINGW_ROOT:-/usr}"
        if [[ "$target" == "windows-xp" && "$arch" == "i686" &&
              -x "$root/.toolchain/apt/root/usr/bin/i686-w64-mingw32-gcc-win32" ]]; then
            mingw_root="$root/.toolchain/apt/root/usr"
        fi
        cmake -B "$build_dir" -G Ninja \
            -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-windows.cmake \
            -DCGEM_MINGW_ARCH="$arch" \
            -DCGEM_WIN32_WINNT="$winnt" \
            -DCGEM_MINGW_ROOT="$mingw_root"
        cmake --build "$build_dir"
        mkdir -p dist
        cp "$build_dir/cgem.exe" "$artifact"
        ;;
    *)
        echo "Unknown target: $target" >&2
        exit 1
        ;;
esac

echo "Built: $root/$artifact"
if command -v file >/dev/null 2>&1; then
    file "$artifact"
fi
