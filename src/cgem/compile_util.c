#define _POSIX_C_SOURCE 200809L

#include "cgem/compiler_internal.h"
#include "cgem/platform.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool cg_blocks_are_internal(const Block *blocks, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (blocks[i].internal) {
            return true;
        }
    }
    return false;
}

bool cg_should_emit_c(const Block *blocks, size_t block_count,
                          bool attribute_internal, bool attribute_public)
{
    if (attribute_public) {
        return true;
    }
    if (attribute_internal || cg_blocks_are_internal(blocks, block_count)) {
        return false;
    }
    return true;
}
bool cg_name_start(unsigned char ch)
{
    return isalpha(ch) || ch == '_';
}

bool cg_name_char(unsigned char ch)
{
    return isalnum(ch) || ch == '_';
}

char *cg_copy_text(const char *text, size_t length)
{
    char *copy = malloc(length + 1);

    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

char *cg_join_path(const char *left, const char *right)
{
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    bool slash = left_length > 0 && left[left_length - 1] != '/';
    char *path = malloc(left_length + (slash ? 1 : 0) + right_length + 1);

    if (!path) {
        return NULL;
    }
    memcpy(path, left, left_length);
    if (slash) {
        path[left_length++] = '/';
    }
    memcpy(path + left_length, right, right_length + 1);
    return path;
}
void cg_free_names(char **names, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
}

const char *cg_param_c_type(const char **params,
                                     const bool *param_variadic, size_t count,
                                     const char *name)
{
    ssize_t index = cg_param_index(params, count, name);

    if (index < 0) {
        return NULL;
    }
    return param_variadic && param_variadic[index] ? "__VA_ARGS__"
                                                   : params[index];
}

ssize_t cg_param_index(const char **params, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(params[i], name) == 0) {
            return (ssize_t) i;
        }
    }
    return -1;
}
