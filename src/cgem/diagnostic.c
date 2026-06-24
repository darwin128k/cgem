#define _POSIX_C_SOURCE 200809L

#include "cgem/diagnostic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DiagnosticList *active_diagnostics;

static bool push_message(DiagnosticList *list, DiagnosticSeverity severity,
                         size_t line, size_t column, const char *code,
                         const char *format, va_list args)
{
    Diagnostic *item;
    char message[1024];
    int length;

    if (!list) {
        return false;
    }
    length = vsnprintf(message, sizeof(message), format, args);
    if (length < 0) {
        return false;
    }
    if (list->count == list->capacity) {
        size_t next = list->capacity ? list->capacity * 2 : 16;
        Diagnostic *grown = realloc(list->items, next * sizeof(*list->items));

        if (!grown) {
            return false;
        }
        list->items = grown;
        list->capacity = next;
    }
    item = &list->items[list->count++];
    item->severity = severity;
    item->line = line;
    item->column = column;
    item->code = code ? strdup(code) : NULL;
    item->message = strdup(message);
    if ((code && !item->code) || !item->message) {
        free(item->code);
        free(item->message);
        item->code = NULL;
        item->message = NULL;
        list->count--;
        return false;
    }
    return true;
}

static size_t parse_line_number(const char *message)
{
    size_t line = 0;

    if (message && strncmp(message, "line ", 5) == 0) {
        sscanf(message + 5, "%zu", &line);
    }
    return line;
}

void cg_diagnostic_init(DiagnosticList *list)
{
    if (!list) {
        return;
    }
    *list = (DiagnosticList) {0};
}

void cg_diagnostic_clear(DiagnosticList *list)
{
    if (!list) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].code);
        free(list->items[i].message);
    }
    free(list->items);
    *list = (DiagnosticList) {0};
}

void cg_diagnostic_free(DiagnosticList *list)
{
    cg_diagnostic_clear(list);
}

void cg_diagnostic_set_active(DiagnosticList *list)
{
    active_diagnostics = list;
}

bool cg_diagnostic_push(DiagnosticList *list, DiagnosticSeverity severity,
                        size_t line, size_t column, const char *code,
                        const char *format, ...)
{
    bool ok;
    va_list args;

    va_start(args, format);
    ok = cg_diagnostic_push_v(list, severity, line, column, code, format, args);
    va_end(args);
    return ok;
}

bool cg_diagnostic_push_v(DiagnosticList *list, DiagnosticSeverity severity,
                          size_t line, size_t column, const char *code,
                          const char *format, va_list args)
{
    return push_message(list, severity, line, column, code, format, args);
}

bool cg_diagnostic_has_errors(const DiagnosticList *list)
{
    if (!list) {
        return false;
    }
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].severity == DIAG_ERROR) {
            return true;
        }
    }
    return false;
}

size_t cg_diagnostic_count(const DiagnosticList *list,
                           DiagnosticSeverity severity)
{
    size_t count = 0;

    if (!list) {
        return 0;
    }
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].severity == severity) {
            count++;
        }
    }
    return count;
}

static void append_legacy_line(char *buffer, size_t buffer_size,
                               const char *line)
{
    size_t length;

    if (!buffer || buffer_size == 0 || !line) {
        return;
    }
    length = strlen(buffer);
    if (length >= buffer_size - 1) {
        return;
    }
    if (length > 0) {
        buffer[length++] = '\n';
        buffer[length] = '\0';
    }
    strncat(buffer, line, buffer_size - length - 1);
}

void cg_diagnostic_format_legacy(const DiagnosticList *list,
                                 char *warning, size_t warning_size,
                                 char *error, size_t error_size)
{
    if (!list) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        const Diagnostic *item = &list->items[i];
        char formatted[1152];

        if (item->code && item->line > 0) {
            snprintf(formatted, sizeof(formatted), "line %zu: [%s] %s",
                     item->line, item->code, item->message);
        } else if (item->line > 0) {
            snprintf(formatted, sizeof(formatted), "line %zu: %s",
                     item->line, item->message);
        } else if (item->code) {
            snprintf(formatted, sizeof(formatted), "[%s] %s",
                     item->code, item->message);
        } else {
            snprintf(formatted, sizeof(formatted), "%s", item->message);
        }

        if (item->severity == DIAG_ERROR) {
            if (error && error_size > 0 && error[0] == '\0') {
                snprintf(error, error_size, "%s", formatted);
            }
        } else if (item->severity == DIAG_WARNING) {
            append_legacy_line(warning, warning_size, formatted);
        } else {
            append_legacy_line(warning, warning_size, formatted);
        }
    }
}

void cg_set_error(char *error, size_t error_size, const char *format, ...)
{
    va_list args;
    char message[1024];

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (error && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
    if (active_diagnostics) {
        cg_diagnostic_push(active_diagnostics, DIAG_ERROR,
                           parse_line_number(message), 0, NULL, "%s", message);
    }
}

void cg_add_warning(char *warning, size_t warning_size, const char *format, ...)
{
    size_t length;
    va_list args;
    char message[1024];

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (active_diagnostics) {
        cg_diagnostic_push(active_diagnostics, DIAG_WARNING,
                           parse_line_number(message), 0, NULL, "%s", message);
        return;
    }

    if (warning && warning_size > 0) {
        length = strlen(warning);
        if (length < warning_size - 1) {
            if (length > 0) {
                warning[length++] = '\n';
                warning[length] = '\0';
            }
            strncat(warning, message, warning_size - length - 1);
        }
    }
}
