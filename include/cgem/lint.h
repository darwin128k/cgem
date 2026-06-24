#ifndef CGEM_LINT_H
#define CGEM_LINT_H

#include "cgem/diagnostic.h"

#include <stdio.h>

int cg_lint(FILE *input, DiagnosticList *diagnostics);

#endif
