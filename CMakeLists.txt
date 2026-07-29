cmake_minimum_required(VERSION 3.12)
project(cgem VERSION 0.1.0 LANGUAGES C)

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
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(WIN32)
    set(CGEM_PLATFORM_SOURCE src/cgem/platform_win32.c)
else()
    set(CGEM_PLATFORM_SOURCE src/cgem/platform_linux.c)
endif()

# cgem_core: the DSL data model (node/attribute/tree/reader/writer/...) and
# the platform layer. Built as a shared library so both the cgem executable
# and every dynamically loaded generator link against the same definitions.
set(CGEM_CORE_SOURCES
    src/cgem/core/allocator.c
    src/cgem/core/array.c
    src/cgem/core/string.c
    src/cgem/core/attribute.c
    src/cgem/core/attributes.c
    src/cgem/core/node.c
    src/cgem/core/symbol.c
    src/cgem/core/object.c
    src/cgem/core/field.c
    src/cgem/core/struct.c
    src/cgem/core/scope.c
    src/cgem/core/module.c
    src/cgem/core/package.c
    src/cgem/core/project.c
    src/cgem/core/reader.c
    src/cgem/core/writer.c
    src/cgem/core/stream.c
    src/cgem/core/memory_stream.c
    src/cgem/core/file_stream.c
    src/cgem/core/config.c
    src/cgem/core/cursor.c
    src/cgem/core/tree.c
    src/cgem/core/generator.c
    src/cgem/core/generator_sink.c
    ${CGEM_PLATFORM_SOURCE}
)

# cgem_core's primitive types alias "lh" (Light Helper) typedefs, generated
# from main.cgem into include/lh/ and already on the include path below.
add_library(cgem_core SHARED ${CGEM_CORE_SOURCES})
target_include_directories(cgem_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
set_target_properties(cgem_core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(UNIX AND NOT APPLE)
    find_library(CGEM_DL_LIBRARY dl)
    if(CGEM_DL_LIBRARY)
        target_link_libraries(cgem_core PRIVATE ${CGEM_DL_LIBRARY})
    endif()
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
    src/cgem/semantic.c
    src/cgem/semantic_nav.c
    src/cgem/typecheck.c
)

set(CGEM_SOURCES
    src/cgem/cgem.c
    src/cgem/common.c
    ${CGEM_COMPILER_SOURCES}
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
target_link_libraries(cgem PRIVATE cgem_core)

if(NOT CGEM_IDE)
    target_compile_definitions(cgem PRIVATE CGEM_NO_IDE)
endif()

# Example out-of-tree generator, loaded by cgem_core's dlopen-based loader
# at runtime (not linked into the cgem executable). Proves the plugin ABI.
add_library(cgem_generator_example MODULE generators/example/generator_example.c)
target_include_directories(cgem_generator_example PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(cgem_generator_example PRIVATE cgem_core)
set_target_properties(cgem_generator_example PROPERTIES PREFIX ""
    WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(cgem_generator_c MODULE generators/c/generator_c.c)
target_include_directories(cgem_generator_c PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(cgem_generator_c PRIVATE cgem_core)
set_target_properties(cgem_generator_c PROPERTIES PREFIX ""
    WINDOWS_EXPORT_ALL_SYMBOLS ON)

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
