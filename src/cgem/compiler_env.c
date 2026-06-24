#define _POSIX_C_SOURCE 200809L

#include "cgem/compiler_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define cg_popen _popen
#define cg_pclose _pclose
#else
#include <sys/wait.h>
#define cg_popen popen
#define cg_pclose pclose
#endif

static bool valid_compiler_name(const char *compiler)
{
    if (!compiler || !compiler[0]) return false;
    for (const char *at = compiler; *at; at++) {
        if (*at == '"' || *at == '\n' || *at == '\r' || *at == '$' ||
            *at == '`' || *at == '%' || *at == '!' || *at == '&' ||
            *at == '|' || *at == '<' || *at == '>' || *at == '^' ||
            *at == '(' || *at == ')') {
            return false;
        }
    }
    return true;
}

static int add_macro(Symbol **symbols, size_t *count, size_t *capacity,
                     char *line)
{
    const char prefix[] = "#define ";
    char *name;
    char *at;
    char *name_end;
    char *value = NULL;
    char *dsl_name;
    bool function_like;
    size_t dsl_length;

    if (strncmp(line, prefix, sizeof(prefix) - 1) != 0) return 0;
    name = line + sizeof(prefix) - 1;
    at = name;
    if (!(isalpha((unsigned char) *at) || *at == '_')) return 0;
    while (isalnum((unsigned char) *at) || *at == '_') at++;
    name_end = at;
    function_like = *at == '(';
    if (function_like) {
        char *close = strchr(at, ')');

        if (!close) return 0;
        at = close + 1;
    }
    if (*at && !isspace((unsigned char) *at)) return 0;
    if (*at) {
        *at++ = '\0';
        while (isspace((unsigned char) *at)) at++;
        if (*at && !function_like) {
            char *end = at + strlen(at);

            while (end > at && isspace((unsigned char) end[-1])) end--;
            *end = '\0';
            value = at;
        }
    }
    if (function_like) *name_end = '\0';
    dsl_length = strlen("c.compiler.") + strlen(name) + 1;
    dsl_name = malloc(dsl_length);
    if (!dsl_name) return -1;
    snprintf(dsl_name, dsl_length, "c.compiler.%s", name);
    if (cg_add_symbol(symbols, count, capacity, dsl_name, name, "", value,
                      true, true) != 0) {
        free(dsl_name);
        return -1;
    }
    return 0;
}

int cg_add_compiler_macros(Symbol **symbols, size_t *count, size_t *capacity,
                           const char *compiler, char *error,
                           size_t error_size)
{
    char command[4096];
    char *line = NULL;
    size_t line_capacity = 0;
    FILE *pipe;
    int status;

    if (!valid_compiler_name(compiler)) {
        snprintf(error, error_size, "invalid compiler name");
        return -1;
    }
#if defined(_WIN32)
    snprintf(command, sizeof(command),
             "\"%s\" -dM -E -x c - < NUL", compiler);
#else
    snprintf(command, sizeof(command),
             "\"%s\" -dM -E -x c - < /dev/null", compiler);
#endif
    pipe = cg_popen(command, "r");
    if (!pipe) {
        snprintf(error, error_size, "cannot run compiler: %s", compiler);
        return -1;
    }
    while (cg_read_input_line(pipe, &line, &line_capacity) >= 0) {
        if (add_macro(symbols, count, capacity, line) != 0) {
            free(line);
            cg_pclose(pipe);
            snprintf(error, error_size, "out of memory");
            return -1;
        }
    }
    free(line);
    status = cg_pclose(pipe);
#if !defined(_WIN32)
    if (status != -1 && WIFEXITED(status)) status = WEXITSTATUS(status);
#endif
    if (status != 0) {
        snprintf(error, error_size,
                 "compiler environment query failed: %s", compiler);
        return -1;
    }
    return 0;
}
