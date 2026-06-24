#define _POSIX_C_SOURCE 200809L

#include "cgem/format.h"

#include "cgem/compiler_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t leading_indent_width(const char *line, size_t length, size_t *end)
{
    size_t width = 0;
    size_t at = 0;

    while (at < length && (line[at] == ' ' || line[at] == '\t')) {
        width += line[at] == '\t' ? 4 : 1;
        at++;
    }
    *end = at;
    return width;
}

static size_t trim_line_end(const char *line, size_t length)
{
    while (length > 0 &&
           (line[length - 1] == ' ' || line[length - 1] == '\t' ||
            line[length - 1] == '\n' || line[length - 1] == '\r')) {
        length--;
    }
    return length;
}

ssize_t cg_format_text_line(char *line, size_t length, size_t capacity)
{
    size_t leading_end;
    size_t indent_width;
    size_t content_end;
    size_t content_length;
    size_t normalized_indent;
    char *formatted;
    ssize_t formatted_length;

    content_end = trim_line_end(line, length);
    indent_width = leading_indent_width(line, content_end, &leading_end);
    if (leading_end == content_end) {
        return 0;
    }
    normalized_indent = indent_width - (indent_width % 4);
    content_length = content_end - leading_end;
    formatted_length = (ssize_t) (normalized_indent + content_length);
    if ((size_t) formatted_length + 1 > capacity) {
        return -1;
    }
    formatted = line;
    if (content_length > 0) {
        if (normalized_indent > leading_end) {
            size_t i = content_length;

            while (i > 0) {
                i--;
                formatted[normalized_indent + i] = formatted[leading_end + i];
            }
        } else {
            memmove(formatted + normalized_indent, line + leading_end,
                    content_length);
        }
    }
    if (normalized_indent > 0) {
        memset(formatted, ' ', normalized_indent);
    }
    formatted[formatted_length] = '\0';
    return formatted_length;
}

int cg_format_stream(FILE *input, FILE *output)
{
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;

    if (!input || !output) {
        return -1;
    }
    while ((line_length = cg_read_input_line(input, &line, &line_capacity)) != -1) {
        ssize_t formatted_length =
            cg_format_text_line(line, (size_t) line_length, line_capacity);

        if (formatted_length < 0) {
            free(line);
            return -1;
        }
        if (formatted_length > 0) {
            if (fwrite(line, 1, (size_t) formatted_length, output) !=
                (size_t) formatted_length) {
                free(line);
                return -1;
            }
        }
        if (fputc('\n', output) == EOF) {
            free(line);
            return -1;
        }
    }
    free(line);
    if (ferror(input)) {
        return -1;
    }
    return 0;
}

int cg_format_path(const char *path, char *error, size_t error_size)
{
    char temp_path[512];
    FILE *input;
    FILE *output;
    int result;

    if (!path) {
        cg_set_error(error, error_size, "format path is required");
        return -1;
    }
    if (snprintf(temp_path, sizeof(temp_path), "%s.cgemfmt", path) >=
        (int) sizeof(temp_path)) {
        cg_set_error(error, error_size, "path too long: %s", path);
        return -1;
    }
    input = fopen(path, "r");
    if (!input) {
        cg_set_error(error, error_size, "%s: %s", path, strerror(errno));
        return -1;
    }
    output = fopen(temp_path, "wb");
    if (!output) {
        fclose(input);
        cg_set_error(error, error_size, "%s: %s", temp_path, strerror(errno));
        return -1;
    }
    result = cg_format_stream(input, output);
    fclose(input);
    if (fclose(output) != 0) {
        result = -1;
    }
    if (result != 0) {
        remove(temp_path);
        cg_set_error(error, error_size, "failed to format %s", path);
        return -1;
    }
    if (remove(path) != 0) {
        remove(temp_path);
        cg_set_error(error, error_size, "failed to replace %s", path);
        return -1;
    }
    if (rename(temp_path, path) != 0) {
        cg_set_error(error, error_size, "failed to replace %s", path);
        return -1;
    }
    return 0;
}
