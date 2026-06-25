#define _POSIX_C_SOURCE 200809L

#include "cgem/semantic.h"

#include "cgem/compiler_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const DSL_KEYWORDS[] = {
    "package", "module", "scope", "type", "enum", "struct", "fn", "let",
    "case", "field", "param", "return", "if", "elif", "else", "self", NULL
};

void cgem_semantic_init(CgemSemantic *semantic)
{
    if (!semantic) {
        return;
    }
    *semantic = (CgemSemantic) {0};
    ide_index_init(&semantic->hints);
}

static void free_symbols(CgemSemantic *semantic)
{
    if (!semantic) {
        return;
    }
    for (size_t i = 0; i < semantic->symbol_count; i++) {
        free(semantic->symbols[i].dsl_name);
        free(semantic->symbols[i].type_dsl_name);
    }
    free(semantic->symbols);
    semantic->symbols = NULL;
    semantic->symbol_count = 0;
}

static void free_definitions(CgemSemantic *semantic)
{
    size_t i;

    if (!semantic) {
        return;
    }
    for (i = 0; i < semantic->definition_count; i++) {
        free(semantic->definitions[i].dsl_name);
        free(semantic->definitions[i].file_path);
    }
    free(semantic->definitions);
    semantic->definitions = NULL;
    semantic->definition_count = 0;
}

static int merge_adopted_symbol(CgemSemantic *semantic,
                                CgemSemanticSymbol *symbol)
{
    CgemSemanticSymbol *grown;
    size_t i;

    if (!symbol || !symbol->dsl_name) {
        return 0;
    }
    for (i = 0; i < semantic->symbol_count; i++) {
        if (semantic->symbols[i].dsl_name &&
            strcmp(semantic->symbols[i].dsl_name, symbol->dsl_name) == 0) {
            free(semantic->symbols[i].type_dsl_name);
            semantic->symbols[i].kind = symbol->kind;
            semantic->symbols[i].is_define = symbol->is_define;
            semantic->symbols[i].type_dsl_name = symbol->type_dsl_name;
            symbol->type_dsl_name = NULL;
            free(symbol->dsl_name);
            return 0;
        }
    }
    grown = realloc(semantic->symbols,
                    (semantic->symbol_count + 1) * sizeof(*semantic->symbols));
    if (!grown) {
        return -1;
    }
    semantic->symbols = grown;
    semantic->symbols[semantic->symbol_count++] = *symbol;
    return 0;
}

static CgemSymbolKind map_symbol_kind(SymbolKind kind)
{
    switch (kind) {
    case SYMBOL_KIND_TYPE:
        return CGEM_SYMBOL_KIND_TYPE;
    case SYMBOL_KIND_VALUE:
        return CGEM_SYMBOL_KIND_VALUE;
    case SYMBOL_KIND_MACRO:
        return CGEM_SYMBOL_KIND_MACRO;
    case SYMBOL_KIND_FN:
        return CGEM_SYMBOL_KIND_FN;
    default:
        return CGEM_SYMBOL_KIND_UNKNOWN;
    }
}

bool cgem_semantic_symbol_is_type(const CgemSemanticSymbol *symbol)
{
    return symbol && symbol->kind == CGEM_SYMBOL_KIND_TYPE;
}

const CgemSemanticSymbol *cgem_semantic_find(const CgemSemantic *semantic,
                                               const char *name)
{
    if (!semantic || !name) {
        return NULL;
    }
    for (size_t i = 0; i < semantic->symbol_count; i++) {
        if (semantic->symbols[i].dsl_name &&
            strcmp(semantic->symbols[i].dsl_name, name) == 0) {
            return &semantic->symbols[i];
        }
    }
    return NULL;
}

void cgem_semantic_clear(CgemSemantic *semantic)
{
    if (!semantic) {
        return;
    }
    free_symbols(semantic);
    free_definitions(semantic);
    ide_index_clear_hints(&semantic->hints);
    semantic->analyzed_revision = 0;
}

void cgem_semantic_free(CgemSemantic *semantic)
{
    if (!semantic) {
        return;
    }
    cgem_semantic_clear(semantic);
    ide_index_free(&semantic->hints);
    *semantic = (CgemSemantic) {0};
}

void cgem_semantic_adopt_symbols(CgemSemantic *semantic, Symbol *symbols,
                                 size_t symbol_count)
{
    size_t i;

    if (!semantic) {
        return;
    }
    for (i = 0; i < symbol_count; i++) {
        CgemSemanticSymbol entry;

        if (!symbols[i].dsl_name) {
            continue;
        }
        entry.dsl_name = symbols[i].dsl_name;
        entry.kind = map_symbol_kind(symbols[i].kind);
        entry.is_define = symbols[i].is_define;
        entry.type_dsl_name = symbols[i].type_dsl_name ?
                                  strdup(symbols[i].type_dsl_name) : NULL;
        symbols[i].dsl_name = NULL;
        symbols[i].type_dsl_name = NULL;
        if (merge_adopted_symbol(semantic, &entry) != 0) {
            free(entry.dsl_name);
            free(entry.type_dsl_name);
        }
    }
}

static bool symbol_name_known(const CgemSemantic *semantic, const char *name,
                              size_t length, bool allow_prefix)
{
    if (!semantic || !name || length == 0) {
        return false;
    }
    for (size_t i = 0; i < semantic->symbol_count; i++) {
        const char *entry = semantic->symbols[i].dsl_name;
        size_t entry_length;

        if (!entry) {
            continue;
        }
        entry_length = strlen(entry);
        if (entry_length == length && memcmp(entry, name, length) == 0) {
            return true;
        }
        if (allow_prefix && entry_length > length &&
            memcmp(entry, name, length) == 0) {
            return true;
        }
    }
    return false;
}

static bool hints_reference_known(const IdeIndex *hints, const char *reference,
                                  size_t length)
{
    return ide_index_reference_known(hints, reference, length, false);
}

bool cgem_semantic_reference_known(const CgemSemantic *semantic,
                                   const char *reference, size_t length,
                                   bool allow_prefix)
{
    if (!semantic || !reference || length == 0) {
        return false;
    }
    if (hints_reference_known(&semantic->hints, reference, length)) {
        return true;
    }
    return symbol_name_known(semantic, reference, length, allow_prefix);
}

static bool hints_has_children(const IdeIndex *hints, const char *parent,
                               size_t parent_length)
{
    for (size_t i = 0; i < hints->child_count; i++) {
        const char *edge_parent = hints->children[i].parent;

        if (strlen(edge_parent) == parent_length &&
            memcmp(edge_parent, parent, parent_length) == 0) {
            return true;
        }
    }
    return false;
}

static bool hints_is_terminal(const IdeIndex *hints, const char *path,
                              size_t path_length)
{
    for (size_t i = 0; i < hints->terminal_count; i++) {
        const char *terminal = hints->terminals[i];
        size_t length = strlen(terminal);

        if (length == path_length &&
            memcmp(terminal, path, path_length) == 0) {
            return true;
        }
    }
    return false;
}

static bool symbol_extends_terminal(const CgemSemantic *semantic,
                                    const char *entry)
{
    size_t entry_length = strlen(entry);

    for (size_t i = 0; i < semantic->hints.terminal_count; i++) {
        const char *terminal = semantic->hints.terminals[i];
        size_t length = strlen(terminal);

        if (entry_length <= length) {
            continue;
        }
        if (entry[length] == '.' && strncmp(entry, terminal, length) == 0) {
            if (hints_has_children(&semantic->hints, terminal, length)) {
                continue;
            }
            return true;
        }
    }
    return false;
}

static const char *ghost_from_children(const IdeIndex *hints,
                                       const char *parent_token,
                                       size_t parent_length,
                                       const char *partial,
                                       size_t partial_length)
{
    static char suffix[256];
    const char *best = NULL;
    size_t best_length = 0;

    for (size_t i = 0; i < hints->child_count; i++) {
        const IdeScopeChild *edge = &hints->children[i];
        size_t edge_parent_length = strlen(edge->parent);
        size_t edge_child_length = strlen(edge->child);

        if (edge_parent_length != parent_length ||
            memcmp(edge->parent, parent_token, parent_length) != 0) {
            continue;
        }
        if (partial_length > edge_child_length ||
            memcmp(edge->child, partial, partial_length) != 0) {
            continue;
        }
        if (!best || edge_child_length < best_length ||
            (edge_child_length == best_length &&
             strcmp(edge->child, best) < 0)) {
            best = edge->child;
            best_length = edge_child_length;
        }
    }
    if (!best || best_length <= partial_length) {
        return NULL;
    }
    if (best_length - partial_length >= sizeof(suffix)) {
        return NULL;
    }
    memcpy(suffix, best + partial_length, best_length - partial_length + 1);
    return suffix;
}

const char *cgem_semantic_ghost_suffix(const CgemSemantic *semantic,
                                       const char *scope,
                                       const char *token,
                                       size_t token_length)
{
    static char suffix[256];
    const char *best = NULL;
    size_t suffix_length;
    size_t parent_length = 0;
    const char *last_dot = NULL;
    char qualified[256];
    char scope_copy[256];
    const char *scoped_suffix;

    if (!semantic || !token || token_length == 0) {
        return NULL;
    }

    for (size_t i = 0; i < token_length; i++) {
        if (token[i] == '.') {
            last_dot = token + i;
        }
    }

    if (last_dot != NULL) {
        parent_length = (size_t) (last_dot - token);
        if (hints_is_terminal(&semantic->hints, token, parent_length) &&
            !hints_has_children(&semantic->hints, token, parent_length)) {
            return NULL;
        }
        if (last_dot + 1 == token + token_length) {
            return ghost_from_children(&semantic->hints, token, parent_length,
                                       "", 0);
        }
        {
            const char *partial = last_dot + 1;
            size_t partial_length = token_length - parent_length - 1;
            const char *child_suffix =
                ghost_from_children(&semantic->hints, token, parent_length,
                                    partial, partial_length);

            if (child_suffix) {
                return child_suffix;
            }
        }
    }

    for (size_t i = 0; i < semantic->symbol_count; i++) {
        const char *entry = semantic->symbols[i].dsl_name;
        size_t entry_length;

        if (!entry) {
            continue;
        }
        entry_length = strlen(entry);
        if (symbol_extends_terminal(semantic, entry)) {
            continue;
        }
        if (entry_length <= token_length ||
            memcmp(entry, token, token_length) != 0) {
            continue;
        }
        if (!best || strcmp(entry, best) < 0) {
            best = entry;
        }
    }

    for (size_t i = 0; DSL_KEYWORDS[i]; i++) {
        const char *keyword = DSL_KEYWORDS[i];
        size_t keyword_length = strlen(keyword);

        if (keyword_length <= token_length ||
            memcmp(keyword, token, token_length) != 0) {
            continue;
        }
        if (!best || strcmp(keyword, best) < 0) {
            best = keyword;
        }
    }

    if (!best) {
        if (!scope || !scope[0]) {
            return NULL;
        }
        snprintf(scope_copy, sizeof(scope_copy), "%s", scope);
        for (;;) {
            size_t scope_length = strlen(scope_copy);

            if (scope_length + 1 + token_length + 1 > sizeof(qualified)) {
                return NULL;
            }
            memcpy(qualified, scope_copy, scope_length);
            qualified[scope_length] = '.';
            memcpy(qualified + scope_length + 1, token, token_length);
            qualified[scope_length + 1 + token_length] = '\0';
            scoped_suffix =
                cgem_semantic_ghost_suffix(semantic, NULL, qualified,
                                           strlen(qualified));
            if (scoped_suffix) {
                return scoped_suffix;
            }
            if (!scope_length) {
                break;
            }
            {
                char *last_dot_scope = strrchr(scope_copy, '.');

                if (!last_dot_scope) {
                    break;
                }
                *last_dot_scope = '\0';
            }
        }
        return NULL;
    }
    suffix_length = strlen(best + token_length);
    if (suffix_length == 0 || suffix_length >= sizeof(suffix)) {
        return NULL;
    }
    memcpy(suffix, best + token_length, suffix_length + 1);
    return suffix;
}

const char *cgem_semantic_fn_hint(const CgemSemantic *semantic,
                                  const char *scope, const char *callee)
{
    const char *hint;
    char qualified[256];
    char scope_copy[256];

    if (!semantic || !callee || !callee[0]) {
        return NULL;
    }
    hint = ide_index_fn_hint(&semantic->hints, callee);
    if (hint) {
        return hint;
    }
    if (!scope || !scope[0] || strchr(callee, '.')) {
        return NULL;
    }
    snprintf(scope_copy, sizeof(scope_copy), "%s", scope);
    for (;;) {
        size_t scope_length = strlen(scope_copy);
        size_t callee_length = strlen(callee);

        if (scope_length + 1 + callee_length + 1 > sizeof(qualified)) {
            return NULL;
        }
        memcpy(qualified, scope_copy, scope_length);
        qualified[scope_length] = '.';
        memcpy(qualified + scope_length + 1, callee, callee_length);
        qualified[scope_length + 1 + callee_length] = '\0';
        hint = ide_index_fn_hint(&semantic->hints, qualified);
        if (hint) {
            return hint;
        }
        if (!scope_length) {
            break;
        }
        {
            char *last_dot = strrchr(scope_copy, '.');

            if (!last_dot) {
                break;
            }
            *last_dot = '\0';
        }
    }
    return NULL;
}

bool cgem_semantic_scope_path(const IdeIndexRow *rows, size_t row_count,
                              size_t line_index, char *out, size_t out_size)
{
    return ide_index_scope_path(rows, row_count, line_index, out, out_size);
}

int cgem_semantic_analyze_rows(const IdeIndexRow *rows, size_t row_count,
                               const char *compiler,
                               const char *include_path,
                               const char *source_path,
                               const char *workspace_root,
                               const char *current_file,
                               DiagnosticList *diagnostics,
                               CgemSemantic *semantic)
{
    FILE *input;
    int result;
    const char *include_dir =
        include_path && include_path[0] ? include_path : ".";
    const char *source_dir =
        source_path && source_path[0] ? source_path : ".";

    if (!rows || !compiler || !diagnostics || !semantic) {
        return -1;
    }
    cgem_semantic_clear(semantic);
    cg_diagnostic_clear(diagnostics);
    if (workspace_root && workspace_root[0]) {
        cgem_semantic_load_workspace(workspace_root, current_file, compiler,
                                     include_dir, source_dir, semantic);
    }

    input = tmpfile();
    if (!input) {
        return -1;
    }
    for (size_t i = 0; i < row_count; i++) {
        if (fwrite(rows[i].data, 1, rows[i].length, input) != rows[i].length ||
            (i + 1 < row_count && fputc('\n', input) == EOF)) {
            fclose(input);
            return -1;
        }
    }
    if (fflush(input) != 0 || fseek(input, 0, SEEK_SET) != 0) {
        fclose(input);
        return -1;
    }
    result = cgem_analyze(input, compiler, include_dir, source_dir, diagnostics,
                          semantic);
    fclose(input);
    if (result != 0) {
        return result;
    }
    if (cg_diagnostic_has_errors(diagnostics)) {
        return 0;
    }
    cgem_semantic_index_definitions(rows, row_count, current_file, semantic);
    ide_index_collect_hints(&semantic->hints, rows, row_count);
    return 0;
}
