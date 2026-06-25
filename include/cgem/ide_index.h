#ifndef CGEM_IDE_INDEX_H
#define CGEM_IDE_INDEX_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *data;
    size_t length;
} IdeIndexRow;

typedef struct {
    char *parent;
    char *child;
} IdeScopeChild;

typedef struct {
    char *key;
    char *hint;
} IdeFnHint;

typedef struct {
    char **entries;
    size_t count;
    size_t capacity;
    char **terminals;
    size_t terminal_count;
    size_t terminal_capacity;
    IdeScopeChild *children;
    size_t child_count;
    size_t child_capacity;
    IdeFnHint *fn_hints;
    size_t fn_hint_count;
    size_t fn_hint_capacity;
} IdeIndex;

void ide_index_init(IdeIndex *index);
void ide_index_free(IdeIndex *index);
void ide_index_clear_hints(IdeIndex *index);
void ide_index_collect_hints(IdeIndex *index, const IdeIndexRow *rows,
                             size_t row_count);
void ide_index_rebuild(IdeIndex *index, const IdeIndexRow *rows, size_t row_count);

bool ide_index_completion_token_at(const char *line, size_t cursor,
                                   const char **token, size_t *token_length);

bool ide_index_completion_token(const char *line, size_t length,
                                const char **token, size_t *token_length);

const char *ide_index_ghost_suffix(const IdeIndex *index, const char *token,
                                   size_t token_length);

bool ide_index_scope_path(const IdeIndexRow *rows, size_t row_count,
                          size_t line_index, char *out, size_t out_size);

const char *ide_index_scoped_ghost_suffix(const IdeIndex *index,
                                          const char *scope, const char *token,
                                          size_t token_length);

bool ide_index_reference_known(const IdeIndex *index, const char *reference,
                               size_t length, bool allow_prefix);

const char *ide_index_fn_hint(const IdeIndex *index, const char *callee);

const char *ide_index_fn_hint_scoped(const IdeIndex *index, const char *scope,
                                     const char *callee);

#endif
