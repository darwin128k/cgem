#include "cgem/core/config.h"

#include "cgem/core/allocator.h"
#include "cgem/core/attribute.h"
#include "cgem/core/file_stream.h"
#include "cgem/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(cgem_reader_t *reader, size_t *out_size)
{
    size_t capacity = 256;
    size_t size = 0;
    char *buffer = cgem_alloc(capacity);

    if (!buffer) {
        return NULL;
    }
    for (;;) {
        size_t n;

        if (size == capacity) {
            size_t new_capacity = capacity * 2;
            char *grown = cgem_realloc(buffer, new_capacity);

            if (!grown) {
                cgem_free(buffer);
                return NULL;
            }
            buffer = grown;
            capacity = new_capacity;
        }
        if (!cgem_reader_read(reader, buffer + size, capacity - size, &n)) {
            cgem_free(buffer);
            return NULL;
        }
        if (n == 0) {
            break;
        }
        size += n;
    }
    if (size == capacity) {
        char *grown = cgem_realloc(buffer, size + 1);

        if (!grown) {
            cgem_free(buffer);
            return NULL;
        }
        buffer = grown;
    }
    buffer[size] = '\0';
    *out_size = size;
    return buffer;
}

static char *trim(char *text)
{
    size_t length;

    while (*text && isspace((unsigned char) *text)) {
        text++;
    }
    length = strlen(text);
    while (length > 0 && isspace((unsigned char) text[length - 1])) {
        text[--length] = '\0';
    }
    return text;
}

static char *unquote(char *text)
{
    size_t length = strlen(text);

    if (length >= 2 && text[0] == '"' && text[length - 1] == '"') {
        text[length - 1] = '\0';
        return text + 1;
    }
    return text;
}

static bool parse_int(const char *text, long long *value)
{
    char *end;

    if (!*text) {
        return false;
    }
    *value = strtoll(text, &end, 10);
    return *end == '\0';
}

static cgem_attribute_value_t *classify_value(const char *raw)
{
    long long integer;

    if (strcmp(raw, "true") == 0) {
        return cgem_attribute_value_new_bool(true);
    }
    if (strcmp(raw, "false") == 0) {
        return cgem_attribute_value_new_bool(false);
    }
    if (parse_int(raw, &integer)) {
        return cgem_attribute_value_new_int(integer);
    }
    return cgem_attribute_value_new_string(raw);
}

static bool parse_key_value(cgem_attributes_t *config, char *text, char *error,
                            size_t error_size)
{
    char *equals;
    char *key;
    char *value_text;
    cgem_attribute_value_t *value;
    cgem_attribute_t *attribute;

    equals = strchr(text, '=');
    if (!equals) {
        snprintf(error, error_size, "invalid \"key=value\" entry (missing '='): %s",
                 text);
        return false;
    }
    *equals = '\0';
    key = trim(text);
    value_text = unquote(trim(equals + 1));
    if (!*key) {
        snprintf(error, error_size, "invalid \"key=value\" entry (empty key)");
        return false;
    }
    value = classify_value(value_text);
    if (!value) {
        snprintf(error, error_size, "out of memory");
        return false;
    }
    attribute = cgem_attribute_new(key, value);
    if (!attribute) {
        cgem_attribute_value_free(value);
        snprintf(error, error_size, "out of memory");
        return false;
    }
    if (!cgem_attributes_add(config, attribute)) {
        cgem_attribute_free(attribute);
        snprintf(error, error_size, "out of memory");
        return false;
    }
    return true;
}

static bool parse_line(cgem_attributes_t *config, char *line, char *error,
                       size_t error_size)
{
    char *trimmed = trim(line);

    if (!*trimmed || trimmed[0] == '#') {
        return true;
    }
    return parse_key_value(config, trimmed, error, error_size);
}

bool cgem_config_parse_arg(cgem_attributes_t *config, const char *arg,
                           char *error, size_t error_size)
{
    size_t length;
    char *copy;
    bool ok;

    if (!config || !arg) {
        snprintf(error, error_size, "no config or argument");
        return false;
    }
    length = strlen(arg);
    copy = cgem_alloc(length + 1);
    if (!copy) {
        snprintf(error, error_size, "out of memory");
        return false;
    }
    memcpy(copy, arg, length + 1);
    ok = parse_key_value(config, copy, error, error_size);
    cgem_free(copy);
    return ok;
}

bool cgem_config_parse_args(cgem_attributes_t *config, int argc, char **argv,
                            char *error, size_t error_size)
{
    int i;

    if (!config) {
        snprintf(error, error_size, "no config");
        return false;
    }
    for (i = 0; i < argc; i++) {
        if (!strchr(argv[i], '=')) {
            continue;
        }
        if (!cgem_config_parse_arg(config, argv[i], error, error_size)) {
            return false;
        }
    }
    return true;
}

cgem_attributes_t *cgem_config_parse(cgem_reader_t *reader, char *error,
                                     size_t error_size)
{
    size_t size;
    char *text;
    cgem_attributes_t *config;
    char *line_start;
    size_t i;
    bool ok = true;

    if (!reader) {
        snprintf(error, error_size, "no config reader");
        return NULL;
    }
    text = read_all(reader, &size);
    if (!text) {
        snprintf(error, error_size, "cannot read config");
        return NULL;
    }
    config = cgem_attributes_new();
    if (!config) {
        cgem_free(text);
        snprintf(error, error_size, "out of memory");
        return NULL;
    }
    line_start = text;
    for (i = 0; i <= size && ok; i++) {
        if (i == size || text[i] == '\n') {
            char saved = i < size ? text[i] : '\0';

            if (i < size) {
                text[i] = '\0';
            }
            ok = parse_line(config, line_start, error, error_size);
            if (i < size) {
                text[i] = saved;
            }
            line_start = text + i + 1;
        }
    }
    cgem_free(text);
    if (!ok) {
        cgem_attributes_free(config);
        return NULL;
    }
    return config;
}

cgem_attributes_t *cgem_config_find(char *error, size_t error_size)
{
    cgem_reader_t *reader;
    cgem_attributes_t *config;

    error[0] = '\0';
    if (!platform_path_exists(CGEM_CONFIG_FILENAME) ||
        !platform_path_is_regular_file(CGEM_CONFIG_FILENAME)) {
        return NULL;
    }
    reader = cgem_file_reader_new(CGEM_CONFIG_FILENAME);
    if (!reader) {
        snprintf(error, error_size, "%s: cannot open", CGEM_CONFIG_FILENAME);
        return NULL;
    }
    config = cgem_config_parse(reader, error, error_size);
    cgem_reader_free(reader);
    return config;
}
