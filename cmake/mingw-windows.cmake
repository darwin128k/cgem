# Cross-compile cgem for Windows with MinGW-w64.
#
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-windows.cmake \
#     -DCGEM_MINGW_ARCH=i686 -DCGEM_WIN32_WINNT=0x0501
#
# CGEM_MINGW_ARCH: i686 | x86_64
# CGEM_WIN32_WINNT: 0x0501 (XP), 0x0600 (Vista), 0x0601 (7), 0x0602 (8), 0x0A00 (10)
# CGEM_MINGW_ROOT: prefix with bin/i686-w64-mingw32-gcc (default /usr)

set(CMAKE_SYSTEM_NAME Windows)

# Keep custom toolchain settings when CMake runs try_compile (ABI detection).
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    CGEM_MINGW_ARCH
    CGEM_MINGW_ROOT
    CGEM_WIN32_WINNT)

if(NOT DEFINED CGEM_MINGW_ARCH)
    set(CGEM_MINGW_ARCH i686)
endif()

if(CGEM_MINGW_ARCH STREQUAL "i686")
    set(CMAKE_SYSTEM_PROCESSOR i686)
    set(CGEM_MINGW_TRIPLET i686-w64-mingw32)
elseif(CGEM_MINGW_ARCH STREQUAL "x86_64")
    set(CMAKE_SYSTEM_PROCESSOR AMD64)
    set(CGEM_MINGW_TRIPLET x86_64-w64-mingw32)
else()
    message(FATAL_ERROR "Unsupported CGEM_MINGW_ARCH: ${CGEM_MINGW_ARCH}")
endif()

if(NOT DEFINED CGEM_MINGW_ROOT)
    set(CGEM_MINGW_ROOT /usr)
endif()

if(NOT DEFINED CGEM_WIN32_WINNT)
    set(CGEM_WIN32_WINNT 0x0501)
endif()

set(_gcc_win32 ${CGEM_MINGW_ROOT}/bin/${CGEM_MINGW_TRIPLET}-gcc-win32)
set(_gcc_plain ${CGEM_MINGW_ROOT}/bin/${CGEM_MINGW_TRIPLET}-gcc)

if(EXISTS ${_gcc_win32})
    set(CMAKE_C_COMPILER ${_gcc_win32})
elseif(EXISTS ${_gcc_plain})
    set(CMAKE_C_COMPILER ${_gcc_plain})
else()
    message(FATAL_ERROR
        "MinGW compiler not found for ${CGEM_MINGW_TRIPLET} under ${CGEM_MINGW_ROOT}/bin")
endif()

set(_windres ${CGEM_MINGW_ROOT}/bin/${CGEM_MINGW_TRIPLET}-windres)
if(EXISTS ${_windres})
    set(CMAKE_RC_COMPILER ${_windres})
endif()

set(CMAKE_FIND_ROOT_PATH ${CGEM_MINGW_ROOT}/${CGEM_MINGW_TRIPLET} ${CGEM_MINGW_ROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(_winnt_flag -D_WIN32_WINNT=${CGEM_WIN32_WINNT})

if(CGEM_MINGW_ARCH STREQUAL "i686")
    set(_arch_compile_flag -m32)
    set(_arch_link_flag -m32)
else()
    set(_arch_compile_flag -m64)
    set(_arch_link_flag -m64)
endif()

set(CMAKE_C_FLAGS_INIT "${_arch_compile_flag} ${_winnt_flag}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_arch_link_flag} -static-libgcc")

add_compile_options(${_arch_compile_flag} ${_winnt_flag})
add_link_options(${_arch_link_flag} -static-libgcc)
