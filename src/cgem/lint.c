#define _POSIX_C_SOURCE 200809L

#include "cgem/lint.h"

#include "cgem/compiler_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool line_has_trailing_space(const char *line, size_t length)
{
    while (length > 0 &&
           (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        length--;
    }
    return length > 0 && line[length - 1] == ' ';
}

static void lint_line(DiagnosticList *diagnostics, size_t line_number,
                      const char *line, size_t length)
{
    size_t indent = 0;
    size_t content_end = length;

    while (indent < length && line[indent] == ' ') {
        indent++;
    }
    while (content_end > indent &&
           (line[content_end - 1] == '\n' || line[content_end - 1] == '\r')) {
        content_end--;
    }
    if (content_end == indent) {
        return;
    }

    for (size_t i = 0; i < content_end; i++) {
        if (line[i] == '\t') {
            cg_diagnostic_push(diagnostics, DIAG_WARNING, line_number, i + 1,
                               "W001",
                               "indentation must use spaces, not tabs");
            break;
        }
    }

    if (indent % 4 != 0) {
        cg_diagnostic_push(diagnostics, DIAG_WARNING, line_number, 1, "W002",
                           "indentation should use groups of 4 spaces");
    }

    if (line_has_trailing_space(line, length)) {
        cg_diagnostic_push(diagnostics, DIAG_NOTE, line_number, content_end,
                           "W003", "trailing whitespace");
    }
}

int cg_lint(FILE *input, DiagnosticList *diagnostics)
{
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    size_t line_number = 0;

    if (!input || !diagnostics) {
        return -1;
    }
    if (fseek(input, 0, SEEK_SET) != 0) {
        return -1;
    }

    while ((line_length = cg_read_input_line(input, &line, &line_capacity)) != -1) {
        line_number++;
        lint_line(diagnostics, line_number, line, (size_t) line_length);
    }
    free(line);

    if (ferror(input)) {
        return -1;
    }
    if (fseek(input, 0, SEEK_SET) != 0) {
        return -1;
    }
    return 0;
}
