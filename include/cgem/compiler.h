#ifndef CGEM_COMPILER_H
#define CGEM_COMPILER_H

#include "cgem/diagnostic.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

int cgem_compile(FILE *input, const char *include_path,
                 const char *source_path, const char *format_style,
                 const char *compiler, bool clean_output,
                 char *warning, size_t warning_size,
                 char *error, size_t error_size,
                 DiagnosticList *diagnostics_out);

#endif
