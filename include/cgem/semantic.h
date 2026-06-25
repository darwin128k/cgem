#ifndef CGEM_SEMANTIC_H
#define CGEM_SEMANTIC_H

#include "cgem/diagnostic.h"
#include "cgem/ide_index.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    CGEM_SYMBOL_KIND_UNKNOWN,
    CGEM_SYMBOL_KIND_TYPE,
    CGEM_SYMBOL_KIND_VALUE,
    CGEM_SYMBOL_KIND_MACRO,
    CGEM_SYMBOL_KIND_FN
} CgemSymbolKind;

typedef struct {
    char *dsl_name;
    CgemSymbolKind kind;
    char *type_dsl_name;
    bool is_define;
} CgemSemanticSymbol;

typedef struct {
    char *dsl_name;
    size_t line;
    size_t column;
    char *file_path;
} CgemSemanticDefinition;

typedef struct {
    CgemSemanticSymbol *symbols;
    size_t symbol_count;
    CgemSemanticDefinition *definitions;
    size_t definition_count;
    IdeIndex hints;
    uint64_t analyzed_revision;
} CgemSemantic;

void cgem_semantic_init(CgemSemantic *semantic);
void cgem_semantic_free(CgemSemantic *semantic);
void cgem_semantic_clear(CgemSemantic *semantic);

int cgem_semantic_analyze_rows(const IdeIndexRow *rows, size_t row_count,
                               const char *compiler,
                               const char *workspace_root,
                               const char *current_file,
                               DiagnosticList *diagnostics,
                               CgemSemantic *semantic);

int cgem_analyze(FILE *input, const char *compiler,
                 DiagnosticList *diagnostics, CgemSemantic *semantic);

bool cgem_semantic_scope_path(const IdeIndexRow *rows, size_t row_count,
                              size_t line_index, char *out, size_t out_size);

const char *cgem_semantic_ghost_suffix(const CgemSemantic *semantic,
                                       const char *scope,
                                       const char *token,
                                       size_t token_length);

const char *cgem_semantic_fn_hint(const CgemSemantic *semantic,
                                  const char *scope, const char *callee);

bool cgem_semantic_reference_known(const CgemSemantic *semantic,
                                   const char *reference, size_t length,
                                   bool allow_prefix);

bool cgem_semantic_symbol_is_type(const CgemSemanticSymbol *symbol);

const CgemSemanticSymbol *cgem_semantic_find(const CgemSemantic *semantic,
                                               const char *name);

bool cgem_semantic_reference_at(const char *line, size_t length,
                                size_t cursor, char *reference,
                                size_t reference_size, size_t *start_column);

bool cgem_semantic_qualify_reference(const char *reference,
                                     const char *scope,
                                     const CgemSemantic *semantic, char *out,
                                     size_t out_size);

bool cgem_semantic_find_definition(const CgemSemantic *semantic,
                                   const char *qualified_reference,
                                   size_t *line, size_t *column,
                                   char *file_path, size_t file_path_size);

void cgem_semantic_index_definitions(const IdeIndexRow *rows,
                                     size_t row_count,
                                     const char *current_file,
                                     CgemSemantic *semantic);

void cgem_semantic_load_workspace(const char *workspace_root,
                                  const char *skip_file, const char *compiler,
                                  CgemSemantic *semantic);

#endif
