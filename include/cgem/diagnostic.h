#ifndef CGEM_DIAGNOSTIC_H
#define CGEM_DIAGNOSTIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE
} DiagnosticSeverity;

typedef struct {
    DiagnosticSeverity severity;
    size_t line;
    size_t column;
    char *code;
    char *message;
} Diagnostic;

typedef struct {
    Diagnostic *items;
    size_t count;
    size_t capacity;
} DiagnosticList;

void cg_diagnostic_init(DiagnosticList *list);
void cg_diagnostic_clear(DiagnosticList *list);
void cg_diagnostic_free(DiagnosticList *list);

void cg_diagnostic_set_active(DiagnosticList *list);

bool cg_diagnostic_push(DiagnosticList *list, DiagnosticSeverity severity,
                        size_t line, size_t column, const char *code,
                        const char *format, ...);
bool cg_diagnostic_push_v(DiagnosticList *list, DiagnosticSeverity severity,
                          size_t line, size_t column, const char *code,
                          const char *format, va_list args);

bool cg_diagnostic_has_errors(const DiagnosticList *list);
size_t cg_diagnostic_count(const DiagnosticList *list,
                           DiagnosticSeverity severity);

void cg_diagnostic_format_legacy(const DiagnosticList *list,
                                 char *warning, size_t warning_size,
                                 char *error, size_t error_size);

void cg_set_error(char *error, size_t error_size, const char *format, ...);
void cg_add_warning(char *warning, size_t warning_size, const char *format, ...);

#endif
