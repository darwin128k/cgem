#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "cgem/common.h"
#include "cgem/platform.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <unistd.h>
#else
#include <direct.h>
#endif

typedef struct {
    char *error;
    size_t error_size;
    int result;
} CleanContext;

static bool clean_remove_entry(const char *path, void *context)
{
    CleanContext *ctx = context;

    if (ctx->result != 0) {
        return false;
    }
    if (platform_path_is_directory(path)) {
        if (!platform_scan_directory(path, clean_remove_entry, context)) {
            if (ctx->result == 0) {
                snprintf(ctx->error, ctx->error_size,
                         "cannot clean directory: %s", path);
                ctx->result = -1;
            }
            return false;
        }
#if defined(_WIN32)
        if (_rmdir(path) != 0) {
#else
        if (rmdir(path) != 0) {
#endif
            snprintf(ctx->error, ctx->error_size, "%s: %s",
                     path, strerror(errno));
            ctx->result = -1;
            return false;
        }
        return true;
    }
    if (remove(path) != 0) {
        snprintf(ctx->error, ctx->error_size, "%s: %s", path, strerror(errno));
        ctx->result = -1;
        return false;
    }
    return true;
}

static int clean_package(const char *base_path, const char *package_name,
                         char *error, size_t error_size)
{
    size_t base_length = strlen(base_path);
    size_t name_length = strlen(package_name);
    bool slash = base_length > 0 && base_path[base_length - 1] != '/';
    char *path = malloc(base_length + (slash ? 1 : 0) + name_length + 1);
    CleanContext ctx = {error, error_size, 0};

    if (!path) {
        snprintf(error, error_size, "out of memory");
        return -1;
    }
    memcpy(path, base_path, base_length);
    if (slash) {
        path[base_length++] = '/';
    }
    memcpy(path + base_length, package_name, name_length + 1);

    if (platform_path_exists(path) && !clean_remove_entry(path, &ctx)) {
        free(path);
        return -1;
    }
    free(path);
    return 0;
}

int cgem_clean_output_package(const char *include_path,
                              const char *source_path,
                              const char *package_name,
                              char *error, size_t error_size)
{
    if (clean_package(include_path, package_name, error, error_size) != 0 ||
        clean_package(source_path, package_name, error, error_size) != 0) {
        return -1;
    }
    return 0;
}

static bool resolve_directory(const char *path, char *resolved, size_t size)
{
    char combined[PATH_MAX];

    if (!path || !path[0]) {
        return false;
    }
#if !defined(_WIN32)
    if (path[0] == '/') {
        if (platform_path_is_directory(path) && realpath(path, resolved)) {
            return true;
        }
        return snprintf(resolved, size, "%s", path) < (int) size;
    }
    if (!getcwd(combined, sizeof(combined))) {
        return false;
    }
    if (snprintf(resolved, size, "%s/%s", combined, path) >= (int) size) {
        return false;
    }
    if (platform_path_is_directory(resolved)) {
        char absolute[PATH_MAX];

        if (realpath(resolved, absolute)) {
            return snprintf(resolved, size, "%s", absolute) < (int) size;
        }
    }
    return true;
#else
    return snprintf(resolved, size, "%s", path) < (int) size;
#endif
}

static bool style_file_in_directory(const char *directory, const char *name,
                                    char *resolved, size_t resolved_size)
{
    char candidate[PATH_MAX];

    if (!directory || !directory[0]) {
        return false;
    }
    if (snprintf(candidate, sizeof(candidate), "%s/%s", directory, name) >=
        (int) sizeof(candidate)) {
        return false;
    }
    if (!platform_path_is_regular_file(candidate)) {
        return false;
    }
#if !defined(_WIN32)
    if (realpath(candidate, resolved)) {
        return true;
    }
#endif
    return snprintf(resolved, resolved_size, "%s", candidate) < (int) resolved_size;
}

static bool find_clang_format_from(const char *start, char *resolved,
                                   size_t resolved_size)
{
    char directory[PATH_MAX];

    if (!resolve_directory(start, directory, sizeof(directory))) {
        return false;
    }
    for (;;) {
        if (style_file_in_directory(directory, ".clang-format", resolved,
                                    resolved_size) ||
            style_file_in_directory(directory, "_clang-format", resolved,
                                    resolved_size)) {
            return true;
        }
        if (strcmp(directory, "/") == 0) {
            return false;
        }
        {
            char *slash = strrchr(directory, '/');

            if (!slash) {
                return false;
            }
            if (slash == directory) {
                directory[1] = '\0';
            } else {
                *slash = '\0';
            }
        }
    }
}

const char *cgem_generated_output_style(const char *include_path,
                                        const char *source_path)
{
    static char style_path[PATH_MAX];
    const char *roots[] = {".", include_path, source_path};

    if (!platform_clang_format_available()) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        if (find_clang_format_from(roots[i], style_path, sizeof(style_path))) {
            return style_path;
        }
    }
    return NULL;
}
