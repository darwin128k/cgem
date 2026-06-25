#define _POSIX_C_SOURCE 200809L

#include "cgem/compiler_internal.h"
#include "cgem/platform.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SymbolValueKind infer_symbol_value_kind(const char *value)
{
    const char *at = value;
    bool floating = false;
    bool digit = false;

    if (!at) return SYMBOL_VALUE_UNKNOWN;
    while (isspace((unsigned char) *at)) at++;
    if (*at == '"') return SYMBOL_VALUE_STRING;
    if (*at == '+' || *at == '-') at++;
    if (!*at) return SYMBOL_VALUE_UNKNOWN;
    for (; *at; at++) {
        unsigned char ch = (unsigned char) *at;

        if (isdigit(ch)) {
            digit = true;
        } else if (ch == '.' || ch == 'e' || ch == 'E' || ch == 'p' ||
                   ch == 'P') {
            floating = true;
        } else if (isalpha(ch) || ch == 'x' || ch == 'X' || ch == '+' ||
                   ch == '-' || ch == '(' || ch == ')' || isspace(ch)) {
            continue;
        } else {
            return SYMBOL_VALUE_UNKNOWN;
        }
    }
    if (!digit) return SYMBOL_VALUE_UNKNOWN;
    return floating ? SYMBOL_VALUE_FLOATING : SYMBOL_VALUE_INTEGER;
}

char *cg_build_symbol_prefix(Block *blocks, size_t count,
                                 const char *name, bool type_suffix)
{
    size_t name_length = name ? strlen(name) : 0;
    size_t length = name_length + 1 + (type_suffix ? 3 : 1);
    char *prefix;
    size_t at = 0;

    for (size_t i = 0; i < count; i++) {
        if (blocks[i].noscope) {
            continue;
        }
        length += strlen(blocks[i].name) + 1;
    }
    prefix = malloc(length);
    if (!prefix) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        size_t part = strlen(blocks[i].name);

        if (blocks[i].noscope) {
            continue;
        }
        memcpy(prefix + at, blocks[i].name, part);
        at += part;
        prefix[at++] = '_';
    }
    if (name_length) {
        memcpy(prefix + at, name, name_length);
        at += name_length;
    } else if (at > 0) {
        at--;
    }
    prefix[at] = '\0';
    if (type_suffix) {
        strcat(prefix, "_t");
    }
    return prefix;
}

char *cg_build_dsl_name(Block *blocks, size_t count, const char *name)
{
    size_t length = strlen(name) + 1;
    char *result;
    size_t at = 0;

    for (size_t i = 0; i < count; i++) {
        if (blocks[i].noscope) {
            continue;
        }
        length += strlen(blocks[i].name) + 1;
    }
    result = malloc(length);
    if (!result) return NULL;
    for (size_t i = 0; i < count; i++) {
        size_t part = strlen(blocks[i].name);

        if (blocks[i].noscope) {
            continue;
        }
        memcpy(result + at, blocks[i].name, part);
        at += part;
        result[at++] = '.';
    }
    strcpy(result + at, name);
    return result;
}

bool cg_is_module_export_name(const char *decl_name)
{
    return decl_name != NULL && strcmp(decl_name, "module") == 0;
}

char *cg_resolve_export_name(Block *blocks, size_t count, const char *decl_name)
{
    if (decl_name && !cg_is_module_export_name(decl_name)) {
        return strdup(decl_name);
    }
    if (count == 0) {
        return NULL;
    }
    return strdup(blocks[count - 1].name);
}

const char *cg_export_c_suffix(Block *blocks, size_t count,
                               const char *decl_name)
{
    if (decl_name != NULL && !cg_is_module_export_name(decl_name)) {
        return decl_name;
    }
    if (count == 0 || !blocks[count - 1].noscope) {
        return NULL;
    }
    return blocks[count - 1].name;
}

char *cg_build_export_symbol(Block *blocks, size_t count, const char *decl_name,
                             bool type_suffix)
{
    return cg_build_symbol_prefix(blocks, count,
                                  cg_export_c_suffix(blocks, count, decl_name),
                                  type_suffix);
}

int cg_note_primary_export(Block *blocks, size_t count, const char *decl_name,
                           size_t line_number, char *error, size_t error_size)
{
    if (!cg_is_module_export_name(decl_name)) {
        return 0;
    }
    if (count == 0 || blocks[count - 1].kind != BLOCK_MODULE) {
        cg_set_error(error, error_size,
                     "line %zu: primary export must be inside a module",
                     line_number);
        return -1;
    }
    if (blocks[count - 1].has_primary_export) {
        cg_set_error(error, error_size,
                     "line %zu: module already has a primary export",
                     line_number);
        return -1;
    }
    blocks[count - 1].has_primary_export = true;
    return 0;
}

int cg_add_primary_export_alias(Block *blocks, size_t block_count,
                                Symbol **symbols, size_t *symbol_count,
                                size_t *symbol_capacity)
{
    Symbol *last;
    char *parent_name;

    if (block_count < 2 || *symbol_count == 0) {
        return 0;
    }
    last = &(*symbols)[*symbol_count - 1];
    parent_name = cg_build_dsl_name(blocks, block_count - 1,
                                    blocks[block_count - 1].name);
    if (!parent_name) {
        return -1;
    }
    if (cg_add_symbol_ex(symbols, symbol_count, symbol_capacity, parent_name,
                         last->c_name, last->header, last->c_expr,
                         last->is_define, last->is_internal, last->is_mutable,
                         last->kind, last->type_dsl_name ?
                             strdup(last->type_dsl_name) : NULL) != 0) {
        free(parent_name);
        return -1;
    }
    return 0;
}

int cg_add_symbol_ex(Symbol **symbols, size_t *count, size_t *capacity,
                     char *dsl_name, const char *c_name, const char *header,
                     const char *c_expr, bool is_define, bool is_internal,
                     bool is_mutable, SymbolKind kind, char *type_dsl_name)
{
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 16;
        Symbol *grown = realloc(*symbols, next * sizeof(**symbols));
        if (!grown) return -1;
        *symbols = grown;
        *capacity = next;
    }
    (*symbols)[*count].dsl_name = dsl_name;
    (*symbols)[*count].c_name = strdup(c_name);
    (*symbols)[*count].header = strdup(header);
    (*symbols)[*count].c_expr = c_expr ? strdup(c_expr) : NULL;
    (*symbols)[*count].is_define = is_define;
    (*symbols)[*count].is_internal = is_internal;
    (*symbols)[*count].is_mutable = is_mutable;
    (*symbols)[*count].value_kind = is_define
        ? infer_symbol_value_kind(c_expr) : SYMBOL_VALUE_UNKNOWN;
    (*symbols)[*count].kind = kind;
    (*symbols)[*count].type_dsl_name = type_dsl_name;
    if (!(*symbols)[*count].c_name || !(*symbols)[*count].header) return -1;
    if (c_expr && !(*symbols)[*count].c_expr) return -1;
    (*count)++;
    return 0;
}

int cg_add_symbol(Symbol **symbols, size_t *count, size_t *capacity,
                  char *dsl_name, const char *c_name, const char *header,
                  const char *c_expr, bool is_define, bool is_internal)
{
    return cg_add_symbol_ex(symbols, count, capacity, dsl_name, c_name, header,
                            c_expr, is_define, is_internal, false,
                            SYMBOL_KIND_UNKNOWN, NULL);
}

int cg_add_builtin_c_types(Symbol **symbols, size_t *count, size_t *capacity)
{
    static const struct {
        const char *name;
        const char *spelling;
    } types[] = {
        {"c.void", "void"},
        {"c.bool", "_Bool"},
        {"c.char", "char"},
        {"c.schar", "signed char"},
        {"c.uchar", "unsigned char"},
        {"c.short", "short"},
        {"c.sshort", "signed short"},
        {"c.ushort", "unsigned short"},
        {"c.int", "int"},
        {"c.sint", "signed int"},
        {"c.uint", "unsigned int"},
        {"c.long", "long"},
        {"c.slong", "signed long"},
        {"c.ulong", "unsigned long"},
        {"c.llong", "long long"},
        {"c.sllong", "signed long long"},
        {"c.ullong", "unsigned long long"},
        {"c.float", "float"},
        {"c.double", "double"},
        {"c.ldouble", "long double"}
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        char *dsl_name = strdup(types[i].name);

        if (!dsl_name ||
            cg_add_symbol_ex(symbols, count, capacity, dsl_name,
                             types[i].spelling, "", types[i].spelling, false,
                             true, false, SYMBOL_KIND_TYPE,
                             strdup(types[i].name)) != 0) {
            free(dsl_name);
            return -1;
        }
    }
    return 0;
}

Symbol *cg_find_symbol(Symbol *symbols, size_t count, const char *name)
{
    for (size_t i = count; i > 0; i--) {
        if (strcmp(symbols[i - 1].dsl_name, name) == 0) {
            return &symbols[i - 1];
        }
    }
    return NULL;
}

Symbol *cg_find_symbol_by_c_name(Symbol *symbols, size_t count, const char *name)
{
    for (size_t i = count; i > 0; i--) {
        if (strcmp(symbols[i - 1].c_name, name) == 0) {
            return &symbols[i - 1];
        }
    }
    return NULL;
}
const char *cg_binding_value(Symbol *symbol, bool expand)
{
    if (expand && symbol->c_expr) {
        return symbol->c_expr;
    }
    return symbol->c_name;
}
char *cg_make_pointer_type(const char *base)
{
    size_t length = strlen(base);
    char *type = malloc(length + 3);

    if (!type) {
        return NULL;
    }
    memcpy(type, base, length);
    memcpy(type + length, " *", 3);
    return type;
}
