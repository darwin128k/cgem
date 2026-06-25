#ifndef CGEM_TYPECHECK_H
#define CGEM_TYPECHECK_H

#include "cgem/diagnostic.h"
#include "cgem/ide_index.h"
#include "cgem/semantic.h"

void cgem_typecheck_rows(const CgemSemantic *semantic,
                         const IdeIndexRow *rows, size_t row_count,
                         DiagnosticList *diagnostics);

#endif
