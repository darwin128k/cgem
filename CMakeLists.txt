cmake_minimum_required(VERSION 3.12)
project(cgem C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(CGEM_IDE "Build CGEM with interactive IDE support" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(WIN32)
    set(CGEM_PLATFORM_SOURCE src/cgem/platform_win32.c)
else()
    set(CGEM_PLATFORM_SOURCE src/cgem/platform_linux.c)
endif()

set(CGEM_COMPILER_SOURCES
    src/cgem/compiler.c
    src/cgem/compile_util.c
    src/cgem/diagnostic.c
    src/cgem/lint.c
    src/cgem/format.c
    src/cgem/symbols.c
    src/cgem/compiler_env.c
    src/cgem/parse.c
    src/cgem/codegen.c
)

set(CGEM_SOURCES
    src/cgem/cgem.c
    src/cgem/common.c
    ${CGEM_COMPILER_SOURCES}
    ${CGEM_PLATFORM_SOURCE}
)

if(CGEM_IDE)
    list(APPEND CGEM_SOURCES
        src/cgem/bmp.c
        src/cgem/ide.c
        src/cgem/ide_index.c
        src/cgem/ide_menu.c
        src/cgem/ide_keymap.c
        themes/palette.c
        themes/monokai-light.c
        themes/monokai.c
        themes/dracula.c
        themes/onedark.c
        themes/gruvbox-light.c
        themes/gruvbox-dark.c
        themes/nord.c
        themes/catppuccin.c
        themes/tokyo-night.c
        themes/solarized-light.c
        themes/solarized-dark.c
        themes/one-light.c
        themes/github-light.c
        themes/android-holo.c
        themes/xcode.c
        themes/xcode-dark.c
        themes/cmd.c
        themes/cmd-dark.c
        themes/windows-xp.c
        themes/windows-xp-dark.c
        themes/windows-aero.c
        themes/windows-aero-dark.c
        themes/windows-modern.c
        themes/windows-modern-dark.c
        themes/ubuntu-light.c
        themes/ubuntu.c
        themes/far.c
        themes/far-dark.c
        themes/atom.c
        themes/registry.c
    )
endif()

add_executable(cgem ${CGEM_SOURCES})
target_include_directories(cgem PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(NOT CGEM_IDE)
    target_compile_definitions(cgem PRIVATE CGEM_NO_IDE)
endif()

if(CGEM_IDE)
    add_custom_target(run
        COMMAND $<TARGET_FILE:cgem> -g -c ${CMAKE_C_COMPILER} -i . -I include -s source --ide
        DEPENDS cgem
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
    add_custom_target(run
        COMMAND $<TARGET_FILE:cgem> -g -c ${CMAKE_C_COMPILER} -i main.cgem -I include -s src
        DEPENDS cgem
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
