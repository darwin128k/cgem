#define _POSIX_C_SOURCE 200809L

#include "cgem/typecheck.h"

#include "cgem/compiler_internal.h"

#include <stdlib.h>
#include <string.h>

static size_t leading_spaces(const char *line, size_t length)
{
    size_t count = 0;

    while (count < length && line[count] == ' ') {
        count++;
    }
    return count;
}

static void pop_blocks(Block *blocks, size_t *count, size_t indent)
{
    while (*count > 0 && blocks[*count - 1].indent >= indent) {
        free(blocks[(*count) - 1].name);
        (*count)--;
    }
}

static const CgemSemanticSymbol *find_exact(const CgemSemantic *semantic,
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

static const CgemSemanticSymbol *resolve_type_symbol(
    const CgemSemantic *semantic, Block *blocks, size_t block_count,
    const char *name)
{
    const CgemSemanticSymbol *symbol;
    char *qualified;

    if (!name || !name[0]) {
        return NULL;
    }
    symbol = find_exact(semantic, name);
    if (symbol) {
        return symbol;
    }
    if (block_count == 0) {
        return NULL;
    }
    qualified = cg_build_dsl_name(blocks, block_count, name);
    if (!qualified) {
        return NULL;
    }
    symbol = find_exact(semantic, qualified);
    free(qualified);
    return symbol;
}

static void report_not_type(DiagnosticList *diagnostics, size_t line,
                            size_t column, const char *name)
{
    cg_diagnostic_push(diagnostics, DIAG_ERROR, line, column, "E101",
                       "not a type: %s", name);
}

static void check_type_ref(const CgemSemantic *semantic, Block *blocks,
                           size_t block_count, const FieldType *type,
                           size_t line_number, size_t column,
                           DiagnosticList *diagnostics)
{
    const CgemSemanticSymbol *symbol;

    if (!type || !type->name || type->is_param_ref) {
        return;
    }
    symbol = resolve_type_symbol(semantic, blocks, block_count, type->name);
    if (!symbol || !cgem_semantic_symbol_is_type(symbol)) {
        report_not_type(diagnostics, line_number, column, type->name);
    }
}

typedef struct {
    char **names;
    size_t count;
    size_t capacity;
} ParamNames;

static void param_names_free(ParamNames *params)
{
    if (!params) {
        return;
    }
    for (size_t i = 0; i < params->count; i++) {
        free(params->names[i]);
    }
    free(params->names);
    *params = (ParamNames) {0};
}

static bool param_names_add(ParamNames *params, const char *name)
{
    char *copy;

    for (size_t i = 0; i < params->count; i++) {
        if (strcmp(params->names[i], name) == 0) {
            return true;
        }
    }
    if (params->count == params->capacity) {
        size_t next = params->capacity ? params->capacity * 2 : 8;
        char **grown = realloc(params->names, next * sizeof(*params->names));

        if (!grown) {
            return false;
        }
        params->names = grown;
        params->capacity = next;
    }
    copy = strdup(name);
    if (!copy) {
        return false;
    }
    params->names[params->count++] = copy;
    return true;
}

static void check_field_type(const CgemSemantic *semantic, Block *blocks,
                             size_t block_count, const FieldType *type,
                             size_t line_number, DiagnosticList *diagnostics)
{
    if (!type || !type->name) {
        return;
    }
    if (type->is_param_ref) {
        return;
    }
    check_type_ref(semantic, blocks, block_count, type, line_number, 1,
                   diagnostics);
}

void cgem_typecheck_rows(const CgemSemantic *semantic,
                         const IdeIndexRow *rows, size_t row_count,
                         DiagnosticList *diagnostics)
{
    Block blocks[48];
    size_t block_count = 0;
    ParamNames struct_params = {0};
    size_t struct_indent = 0;
    bool in_struct = false;
    size_t fn_indent = 0;
    bool in_fn = false;
    FieldType fn_return_type = {0};
    bool attribute_noscope = false;

    if (!semantic || !rows || !diagnostics) {
        return;
    }

    for (size_t y = 0; y < row_count; y++) {
        const char *line = rows[y].data;
        size_t length = rows[y].length;
        size_t indent = leading_spaces(line, length);
        const char *text;
        char *name = NULL;
        BlockKind kind;

        if (length == indent) {
            continue;
        }
        if (in_struct && indent <= struct_indent) {
            in_struct = false;
            param_names_free(&struct_params);
        }
        if (in_fn && indent <= fn_indent) {
            in_fn = false;
            cg_free_field_type(&fn_return_type);
        }
        pop_blocks(blocks, &block_count, indent);
        text = line + indent;

        if (text[0] == '@') {
            const char *attribute_name;
            size_t attribute_name_length;

            if (cg_parse_attribute(text, &attribute_name,
                                   &attribute_name_length)) {
                if (attribute_name_length == strlen("noscope") &&
                    memcmp(attribute_name, "noscope",
                           attribute_name_length) == 0) {
                    attribute_noscope = true;
                }
            }
            continue;
        }

        if (cg_parse_named_block(text, "package", &name)) {
            kind = BLOCK_PACKAGE;
        } else if (cg_parse_named_block(text, "module", &name)) {
            kind = BLOCK_MODULE;
        } else if (cg_parse_named_block(text, "scope", &name)) {
            kind = BLOCK_SCOPE;
        } else if (cg_parse_named_block(text, "struct", &name)) {
            free(name);
            if (block_count > 0 &&
                blocks[block_count - 1].kind == BLOCK_MODULE) {
                struct_indent = indent;
                in_struct = true;
                param_names_free(&struct_params);
            }
            continue;
        } else {
            char *reference = NULL;
            char *base = NULL;
            FieldType field_type = {0};
            bool is_meta = false;
            bool is_variadic = false;
            char *let_value = NULL;
            char *expression = NULL;
            FieldType cast_type = {0};
            char *param_name = NULL;

            if (cg_parse_type(text, &name, NULL, NULL, &reference, NULL, NULL)) {
                check_type_ref(semantic, blocks, block_count, &(FieldType) {
                    .name = reference
                }, y + 1, 1, diagnostics);
                free(name);
                free(reference);
                continue;
            }
            if (cg_parse_enum(text, &name, &base)) {
                check_type_ref(semantic, blocks, block_count,
                               &(FieldType) {.name = base}, y + 1, 1,
                               diagnostics);
                free(name);
                free(base);
                continue;
            }
            if (cg_parse_fn(text, &name, &field_type)) {
                if (in_fn) {
                    cg_free_field_type(&fn_return_type);
                }
                if (field_type.name) {
                    check_type_ref(semantic, blocks, block_count, &field_type,
                                   y + 1, 1, diagnostics);
                }
                in_fn = true;
                fn_indent = indent;
                fn_return_type = field_type;
                free(name);
                continue;
            }
            if (in_fn && cg_parse_return(text, &expression, &cast_type)) {
                if (cast_type.name) {
                    check_type_ref(semantic, blocks, block_count, &cast_type,
                                   y + 1, 1, diagnostics);
                }
                free(expression);
                cg_free_field_type(&cast_type);
                continue;
            }
            if (cg_parse_let(text, &name, &field_type, &let_value)) {
                if (field_type.name) {
                    check_type_ref(semantic, blocks, block_count, &field_type,
                                   y + 1, 1, diagnostics);
                }
                free(name);
                cg_free_field_type(&field_type);
                free(let_value);
                continue;
            }
            if (in_struct &&
                cg_parse_param(text, &param_name, &field_type, &is_meta,
                               &is_variadic)) {
                if (!is_meta && field_type.name && !is_variadic) {
                    check_type_ref(semantic, blocks, block_count, &field_type,
                                   y + 1, 1, diagnostics);
                }
                param_names_add(&struct_params, param_name);
                free(param_name);
                cg_free_field_type(&field_type);
                continue;
            }
            if (in_struct && cg_parse_field(text, &name, &field_type)) {
                check_field_type(semantic, blocks, block_count, &field_type,
                                 y + 1, diagnostics);
                free(name);
                cg_free_field_type(&field_type);
                continue;
            }
            if (in_fn &&
                cg_parse_param(text, &param_name, &field_type, &is_meta,
                               &is_variadic)) {
                if (!is_meta && field_type.name && !is_variadic) {
                    check_type_ref(semantic, blocks, block_count, &field_type,
                                   y + 1, 1, diagnostics);
                }
                free(param_name);
                cg_free_field_type(&field_type);
                continue;
            }
            continue;
        }

        if (block_count >= sizeof(blocks) / sizeof(blocks[0])) {
            free(name);
            attribute_noscope = false;
            continue;
        }
        blocks[block_count++] = (Block) {
            kind, indent, name, attribute_noscope, false, false, {0}, NULL
        };
        attribute_noscope = false;
    }

    param_names_free(&struct_params);
    cg_free_field_type(&fn_return_type);
    while (block_count > 0) {
        free(blocks[--block_count].name);
    }
}
