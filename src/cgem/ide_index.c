#define _POSIX_C_SOURCE 200809L

#include "cgem/ide_index.h"

#include "cgem/compiler_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *dsl_path;
    bool struct_module;
    bool inner_exports;
} ModuleFrame;

typedef struct {
    char *field_name;
    char *type_name;
} StructFieldDef;

typedef struct {
    StructFieldDef *fields;
    size_t field_count;
    size_t field_capacity;
} StructFieldList;

typedef struct {
    char *type_name;
    StructFieldList fields;
} StructDef;

typedef struct {
    StructDef *defs;
    size_t count;
    size_t capacity;
} StructRegistry;

static size_t leading_spaces(const char *line, size_t length)
{
    size_t count = 0;

    while (count < length && line[count] == ' ') {
        count++;
    }
    return count;
}

static char *build_block_path(Block *blocks, size_t count)
{
    size_t length = 0;
    char *path;
    size_t at = 0;

    for (size_t i = 0; i < count; i++) {
        if (blocks[i].noscope) {
            continue;
        }
        length += strlen(blocks[i].name) + 1;
    }
    if (length == 0) {
        return NULL;
    }
    path = malloc(length);
    if (!path) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        size_t part;

        if (blocks[i].noscope) {
            continue;
        }
        part = strlen(blocks[i].name);
        memcpy(path + at, blocks[i].name, part);
        at += part;
        path[at++] = '.';
    }
    if (at > 0) {
        path[at - 1] = '\0';
    } else {
        path[0] = '\0';
    }
    return path;
}

static void index_add(IdeIndex *index, const char *name)
{
    char *copy;
    size_t next;

    if (!index || !name || !name[0]) {
        return;
    }
    for (size_t i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i], name) == 0) {
            return;
        }
    }
    if (index->count == index->capacity) {
        next = index->capacity ? index->capacity * 2 : 32;
        {
            char **grown = realloc(index->entries, next * sizeof(*index->entries));

            if (!grown) {
                return;
            }
            index->entries = grown;
            index->capacity = next;
        }
    }
    copy = strdup(name);
    if (!copy) {
        return;
    }
    index->entries[index->count++] = copy;
}

static void index_add_terminal(IdeIndex *index, const char *path)
{
    char *copy;
    size_t next;

    if (!index || !path || !path[0]) {
        return;
    }
    for (size_t i = 0; i < index->terminal_count; i++) {
        if (strcmp(index->terminals[i], path) == 0) {
            return;
        }
    }
    if (index->terminal_count == index->terminal_capacity) {
        next = index->terminal_capacity ? index->terminal_capacity * 2 : 16;
        {
            char **grown =
                realloc(index->terminals, next * sizeof(*index->terminals));

            if (!grown) {
                return;
            }
            index->terminals = grown;
            index->terminal_capacity = next;
        }
    }
    copy = strdup(path);
    if (!copy) {
        return;
    }
    index->terminals[index->terminal_count++] = copy;
}

static void index_add_child(IdeIndex *index, const char *parent,
                            const char *child)
{
    IdeScopeChild *grown;
    size_t next;

    if (!index || !parent || !parent[0] || !child || !child[0]) {
        return;
    }
    for (size_t i = 0; i < index->child_count; i++) {
        if (strcmp(index->children[i].parent, parent) == 0 &&
            strcmp(index->children[i].child, child) == 0) {
            return;
        }
    }
    if (index->child_count == index->child_capacity) {
        next = index->child_capacity ? index->child_capacity * 2 : 16;
        grown = realloc(index->children, next * sizeof(*index->children));
        if (!grown) {
            return;
        }
        index->children = grown;
        index->child_capacity = next;
    }
    index->children[index->child_count].parent = strdup(parent);
    index->children[index->child_count].child = strdup(child);
    if (!index->children[index->child_count].parent ||
        !index->children[index->child_count].child) {
        free(index->children[index->child_count].parent);
        free(index->children[index->child_count].child);
        return;
    }
    index->child_count++;
}

static bool index_has_children(const IdeIndex *index, const char *parent,
                               size_t parent_length)
{
    if (!index || !parent || parent_length == 0) {
        return false;
    }
    for (size_t i = 0; i < index->child_count; i++) {
        const IdeScopeChild *edge = &index->children[i];

        if (strlen(edge->parent) == parent_length &&
            memcmp(edge->parent, parent, parent_length) == 0) {
            return true;
        }
    }
    return false;
}

static bool index_is_terminal(const IdeIndex *index, const char *path,
                              size_t path_length)
{
    if (!index || !path || path_length == 0) {
        return false;
    }
    for (size_t i = 0; i < index->terminal_count; i++) {
        const char *terminal = index->terminals[i];
        size_t length = strlen(terminal);

        if (length == path_length &&
            memcmp(terminal, path, path_length) == 0) {
            return true;
        }
    }
    return false;
}

static bool index_extends_terminal(const IdeIndex *index, const char *entry)
{
    if (!index || !entry) {
        return false;
    }
    for (size_t i = 0; i < index->terminal_count; i++) {
        const char *terminal = index->terminals[i];
        size_t length = strlen(terminal);
        size_t entry_length = strlen(entry);

        if (entry_length <= length) {
            continue;
        }
        if (entry[length] == '.' &&
            strncmp(entry, terminal, length) == 0) {
            if (index_has_children(index, terminal, length)) {
                continue;
            }
            return true;
        }
    }
    return false;
}

static void index_add_dsl_symbol(IdeIndex *index, Block *blocks, size_t count,
                                 const char *decl_name)
{
    char *dsl_name;

    (void) decl_name;
    if (count == 0) {
        return;
    }
    dsl_name = cg_build_dsl_name(blocks, count - 1, blocks[count - 1].name);
    if (dsl_name) {
        index_add(index, dsl_name);
        free(dsl_name);
    }
}

static void index_add_field_type(IdeIndex *index, const FieldType *type)
{
    if (type && type->name) {
        index_add(index, type->name);
    }
}

static void index_add_reference(IdeIndex *index, const char *reference)
{
    if (reference) {
        index_add(index, reference);
    }
}

static void index_scan_dotted_token(IdeIndex *index, const char *text,
                                    size_t length)
{
    size_t at = 0;

    while (at < length) {
        size_t start;

        while (at < length && !cg_name_start((unsigned char) text[at])) {
            at++;
        }
        if (at >= length) {
            return;
        }
        start = at++;
        while (at < length &&
               (cg_name_char((unsigned char) text[at]) || text[at] == '.')) {
            if (text[at] == '.' && at + 1 < length &&
                !cg_name_start((unsigned char) text[at + 1]) &&
                !cg_name_char((unsigned char) text[at + 1])) {
                break;
            }
            at++;
        }
        if (at > start) {
            char *token = cg_copy_text(text + start, at - start);

            if (token) {
                index_add(index, token);
                free(token);
            }
        }
    }
}

static void finalize_module(IdeIndex *index, ModuleFrame *frame)
{
    if (!frame || !frame->dsl_path) {
        return;
    }
    if (frame->struct_module || !frame->inner_exports) {
        index_add_terminal(index, frame->dsl_path);
    }
    index_add(index, frame->dsl_path);
    free(frame->dsl_path);
    frame->dsl_path = NULL;
}

static void pop_blocks(Block *blocks, size_t *count, size_t indent,
                       IdeIndex *index, ModuleFrame *modules,
                       size_t *module_depth)
{
    while (*count > 0 && blocks[*count - 1].indent >= indent) {
        if (blocks[*count - 1].kind == BLOCK_MODULE && *module_depth > 0) {
            finalize_module(index, &modules[--(*module_depth)]);
        }
        free(blocks[(*count) - 1].name);
        (*count)--;
    }
}

static void struct_field_list_add(StructFieldList *list, const char *field_name,
                                  const char *type_name)
{
    StructFieldDef *def;

    if (list->field_count == list->field_capacity) {
        size_t next = list->field_capacity ? list->field_capacity * 2 : 8;
        StructFieldDef *grown = realloc(list->fields,
                                       next * sizeof(*list->fields));

        if (!grown) {
            return;
        }
        list->fields = grown;
        list->field_capacity = next;
    }
    def = &list->fields[list->field_count++];
    def->field_name = strdup(field_name);
    def->type_name = type_name ? strdup(type_name) : NULL;
}

static void struct_field_list_free(StructFieldList *list)
{
    for (size_t i = 0; i < list->field_count; i++) {
        free(list->fields[i].field_name);
        free(list->fields[i].type_name);
    }
    free(list->fields);
    list->fields = NULL;
    list->field_count = 0;
    list->field_capacity = 0;
}

static void struct_registry_add(StructRegistry *reg, const char *type_name,
                                const StructFieldList *fields)
{
    StructDef *def;

    if (reg->count == reg->capacity) {
        size_t next = reg->capacity ? reg->capacity * 2 : 8;
        StructDef *grown = realloc(reg->defs, next * sizeof(*reg->defs));

        if (!grown) {
            return;
        }
        reg->defs = grown;
        reg->capacity = next;
    }
    def = &reg->defs[reg->count++];
    def->type_name = strdup(type_name);
    def->fields = (StructFieldList) {0};
    for (size_t i = 0; i < fields->field_count; i++) {
        struct_field_list_add(&def->fields, fields->fields[i].field_name,
                             fields->fields[i].type_name);
    }
}

static void struct_registry_free(StructRegistry *reg)
{
    for (size_t i = 0; i < reg->count; i++) {
        free(reg->defs[i].type_name);
        struct_field_list_free(&reg->defs[i].fields);
    }
    free(reg->defs);
    reg->defs = NULL;
    reg->count = 0;
    reg->capacity = 0;
}

static const StructFieldList *struct_registry_find(const StructRegistry *reg,
                                                   const char *type_name)
{
    size_t len = strlen(type_name);

    for (size_t i = 0; i < reg->count; i++) {
        if (strlen(reg->defs[i].type_name) == len &&
            memcmp(reg->defs[i].type_name, type_name, len) == 0) {
            return &reg->defs[i].fields;
        }
    }
    return NULL;
}

static void emit_nested_self_fields(IdeIndex *index,
                                    const StructRegistry *registry,
                                    const char *prefix, size_t prefix_len,
                                    const char *type_name, size_t depth)
{
    const StructFieldList *sub_fields;

    if (depth > 8 || !type_name) {
        return;
    }
    sub_fields = struct_registry_find(registry, type_name);
    if (!sub_fields) {
        return;
    }
    for (size_t i = 0; i < sub_fields->field_count; i++) {
        size_t sub_name_len = strlen(sub_fields->fields[i].field_name);
        size_t entry_len = prefix_len + 1 + sub_name_len;
        char *entry = malloc(entry_len + 1);

        if (!entry) {
            continue;
        }
        memcpy(entry, prefix, prefix_len);
        entry[prefix_len] = '.';
        memcpy(entry + prefix_len + 1, sub_fields->fields[i].field_name,
               sub_name_len);
        entry[entry_len] = '\0';
        index_add(index, entry);
        if (sub_fields->fields[i].type_name) {
            emit_nested_self_fields(index, registry, entry, entry_len,
                                    sub_fields->fields[i].type_name,
                                    depth + 1);
        }
        free(entry);
    }
}

static void emit_self_fields(IdeIndex *index, const StructFieldList *fields,
                             const StructRegistry *registry)
{
    for (size_t i = 0; i < fields->field_count; i++) {
        size_t name_len = strlen(fields->fields[i].field_name);
        char *entry = malloc(5 + name_len + 1);

        if (!entry) {
            continue;
        }
        memcpy(entry, "self.", 5);
        memcpy(entry + 5, fields->fields[i].field_name, name_len);
        entry[5 + name_len] = '\0';
        index_add(index, entry);
        if (registry && fields->fields[i].type_name) {
            emit_nested_self_fields(index, registry, entry, 5 + name_len,
                                    fields->fields[i].type_name, 0);
        }
        free(entry);
    }
}

void ide_index_init(IdeIndex *index)
{
    if (!index) {
        return;
    }
    *index = (IdeIndex) {0};
}

void ide_index_free(IdeIndex *index)
{
    if (!index) {
        return;
    }
    for (size_t i = 0; i < index->count; i++) {
        free(index->entries[i]);
    }
    free(index->entries);
    for (size_t i = 0; i < index->terminal_count; i++) {
        free(index->terminals[i]);
    }
    free(index->terminals);
    for (size_t i = 0; i < index->child_count; i++) {
        free(index->children[i].parent);
        free(index->children[i].child);
    }
    free(index->children);
    for (size_t i = 0; i < index->fn_hint_count; i++) {
        free(index->fn_hints[i].key);
        free(index->fn_hints[i].hint);
    }
    free(index->fn_hints);
    *index = (IdeIndex) {0};
}

#define IDE_FN_HINT_SIZE 480

typedef struct {
    bool active;
    size_t fn_line;
    size_t body_indent;
    char *dsl_key;
    char *self_key;
    char hint[IDE_FN_HINT_SIZE];
    bool has_params;
} FnHintCollector;

static void fn_hint_registry_add(IdeIndex *index, const char *key, const char *hint)
{
    size_t next;

    if (!index || !key || !key[0] || !hint || !hint[0]) {
        return;
    }
    for (size_t i = 0; i < index->fn_hint_count; i++) {
        if (strcmp(index->fn_hints[i].key, key) == 0) {
            char *copy = strdup(hint);

            if (!copy) {
                return;
            }
            free(index->fn_hints[i].hint);
            index->fn_hints[i].hint = copy;
            return;
        }
    }
    if (index->fn_hint_count == index->fn_hint_capacity) {
        next = index->fn_hint_capacity ? index->fn_hint_capacity * 2 : 16;
        {
            IdeFnHint *grown =
                realloc(index->fn_hints, next * sizeof(*index->fn_hints));

            if (!grown) {
                return;
            }
            index->fn_hints = grown;
            index->fn_hint_capacity = next;
        }
    }
    index->fn_hints[index->fn_hint_count].key = strdup(key);
    index->fn_hints[index->fn_hint_count].hint = strdup(hint);
    if (!index->fn_hints[index->fn_hint_count].key ||
        !index->fn_hints[index->fn_hint_count].hint) {
        free(index->fn_hints[index->fn_hint_count].key);
        free(index->fn_hints[index->fn_hint_count].hint);
        return;
    }
    index->fn_hint_count++;
}

static void fn_collector_append_param(FnHintCollector *collector,
                                      const char *param_name,
                                      const char *type_name)
{
    size_t length;

    if (!collector || !param_name || !type_name || !collector->hint[0]) {
        return;
    }
    length = strlen(collector->hint);
    if (length >= sizeof(collector->hint) - 1) {
        return;
    }
    snprintf(collector->hint + length, sizeof(collector->hint) - length,
             "%s param %s as %s",
             collector->has_params ? "," : " ", param_name, type_name);
    collector->has_params = true;
}

static void fn_collector_finish(FnHintCollector *collector, IdeIndex *index)
{
    if (!collector || !collector->active) {
        return;
    }
    if (collector->hint[0]) {
        if (collector->dsl_key) {
            fn_hint_registry_add(index, collector->dsl_key, collector->hint);
        }
        if (collector->self_key) {
            fn_hint_registry_add(index, collector->self_key, collector->hint);
        }
    }
    free(collector->dsl_key);
    free(collector->self_key);
    *collector = (FnHintCollector) {0};
}

static void fn_collector_start(FnHintCollector *collector, IdeIndex *index,
                               Block *blocks, size_t block_count,
                               bool in_struct, const char *fn_name,
                               const FieldType *return_type, size_t indent,
                               size_t line_y)
{
    if (!collector || !fn_name) {
        return;
    }
    fn_collector_finish(collector, index);
    collector->active = true;
    collector->fn_line = line_y;
    collector->body_indent = indent;
    collector->dsl_key = cg_build_dsl_name(blocks, block_count, fn_name);
    if (in_struct) {
        size_t name_len = strlen(fn_name);
        char *self_key = malloc(5 + name_len + 1);

        if (self_key) {
            memcpy(self_key, "self.", 5);
            memcpy(self_key + 5, fn_name, name_len + 1);
            collector->self_key = self_key;
        }
    }
    if (return_type && return_type->name) {
        snprintf(collector->hint, sizeof(collector->hint), "fn %s as %s:",
                 fn_name, return_type->name);
    } else {
        snprintf(collector->hint, sizeof(collector->hint), "fn %s:", fn_name);
    }
}

static bool fn_collector_consume(FnHintCollector *collector, IdeIndex *index,
                                 const char *text)
{
    char *param_name = NULL;
    FieldType field_type = {0};
    bool is_meta = false;
    bool is_variadic = false;
    char *method_name = NULL;
    char *expression = NULL;
    FieldType cast_type = {0};

    if (!collector || !collector->active || !text) {
        return false;
    }
    if (text[0] == '@') {
        return true;
    }
    if (cg_parse_param(text, &param_name, &field_type, &is_meta,
                       &is_variadic)) {
        if (!is_meta && field_type.name) {
            fn_collector_append_param(collector, param_name, field_type.name);
        }
        free(param_name);
        cg_free_field_type(&field_type);
        return true;
    }
    if (cg_parse_self_method_call_block_opener(text, &method_name)) {
        free(method_name);
        return true;
    }
    if (cg_parse_return(text, &expression, &cast_type)) {
        free(expression);
        cg_free_field_type(&cast_type);
        fn_collector_finish(collector, index);
        return true;
    }
    fn_collector_finish(collector, index);
    return false;
}

void ide_index_rebuild(IdeIndex *index, const IdeIndexRow *rows, size_t row_count)
{
    Block blocks[48];
    size_t block_count = 0;
    bool attribute_noscope = false;
    EnumOutput enum_output = {0};
    ModuleFrame modules[48];
    size_t module_depth = 0;
    size_t struct_indent = 0;
    bool in_struct = false;
    StructFieldList struct_fields = {0};
    StructRegistry struct_registry = {0};
    FnHintCollector fn_collector = {0};

    if (!index) {
        return;
    }
    ide_index_free(index);
    ide_index_init(index);

    for (size_t y = 0; y < row_count; y++) {
        const char *line = rows[y].data;
        size_t length = rows[y].length;
        size_t indent = leading_spaces(line, length);
        const char *text;
        char *name = NULL;
        char *case_value = NULL;
        BlockKind kind;

        if (length == indent) {
            continue;
        }
        if (fn_collector.active && y > fn_collector.fn_line &&
            indent <= fn_collector.body_indent) {
            fn_collector_finish(&fn_collector, index);
        }
        if (in_struct && indent <= struct_indent) {
            if (struct_fields.field_count > 0 && block_count > 0) {
                struct_registry_add(&struct_registry,
                                    blocks[block_count - 1].name,
                                    &struct_fields);
            }
            in_struct = false;
            struct_field_list_free(&struct_fields);
        }
        pop_blocks(blocks, &block_count, indent, index, modules, &module_depth);
        text = line + indent;

        if (fn_collector.active && y > fn_collector.fn_line &&
            indent > fn_collector.body_indent) {
            if (fn_collector_consume(&fn_collector, index, text)) {
                continue;
            }
        }

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
            if (module_depth > 0 && cg_is_module_export_name(name)) {
                modules[module_depth - 1].struct_module = true;
                struct_indent = indent;
                in_struct = true;
                struct_field_list_free(&struct_fields);
            }
            free(name);
            continue;
        } else {
            char *reference = NULL;
            const char *expression = NULL;
            size_t expression_length = 0;
            ExprArg *expr_args = NULL;
            size_t expr_arg_count = 0;
            char *base = NULL;
            FieldType field_type = {0};
            bool is_meta = false;
            bool is_variadic = false;
            char *let_value = NULL;

            if (cg_parse_type(text, &name, &expression, &expression_length,
                              &reference, &expr_args, &expr_arg_count)) {
                if (module_depth > 0 && !in_struct &&
                    (!cg_is_module_export_name(name) ||
                     modules[module_depth - 1].struct_module)) {
                    modules[module_depth - 1].inner_exports = true;
                }
                index_add_dsl_symbol(index, blocks, block_count, name);
                index_add_reference(index, reference);
                for (size_t i = 0; i < expr_arg_count; i++) {
                    index_add_reference(index, expr_args[i].dsl_name);
                }
                free(name);
                free(reference);
                cg_free_expr_args(expr_args, expr_arg_count);
                continue;
            }
            if (cg_parse_enum(text, &name, &base)) {
                if (module_depth > 0 && !in_struct) {
                    modules[module_depth - 1].inner_exports = true;
                }
                index_add_dsl_symbol(index, blocks, block_count, name);
                index_add_reference(index, base);
                free(enum_output.local_name);
                free(enum_output.dsl_name);
                enum_output.local_name =
                    cg_resolve_export_name(blocks, block_count, name);
                if (cg_is_module_export_name(name)) {
                    enum_output.dsl_name = block_count > 0 ?
                        cg_build_dsl_name(blocks, block_count - 1,
                                          blocks[block_count - 1].name) :
                        NULL;
                } else {
                    enum_output.dsl_name = enum_output.local_name ?
                        cg_build_dsl_name(blocks, block_count,
                                          enum_output.local_name) : NULL;
                }
                free(name);
                free(base);
                continue;
            }
            if (cg_parse_case(text, &name, &case_value)) {
                index_add(index, name);
                if (enum_output.dsl_name) {
                    size_t member_length =
                        strlen(enum_output.dsl_name) + 1 + strlen(name) + 1;
                    char *member = malloc(member_length);

                    if (member) {
                        snprintf(member, member_length, "%s.%s",
                                 enum_output.dsl_name, name);
                        index_add(index, member);
                        free(member);
                    }
                }
                free(name);
                free(case_value);
                continue;
            }
            if (cg_parse_field(text, &name, &field_type)) {
                index_add(index, name);
                index_add_field_type(index, &field_type);
                if (in_struct) {
                    struct_field_list_add(&struct_fields, name,
                                          field_type.name);
                }
                free(name);
                cg_free_field_type(&field_type);
                continue;
            }
            if (in_struct) {
                char *fn_name = NULL;
                FieldType fn_return_type = {0};

                if (cg_parse_fn(text, &fn_name, &fn_return_type)) {
                    fn_collector_start(&fn_collector, index, blocks, block_count,
                                        true, fn_name, &fn_return_type, indent,
                                        y);
                    index_add_dsl_symbol(index, blocks, block_count, fn_name);
                    if (fn_name) {
                        size_t name_len = strlen(fn_name);
                        char *self_entry = malloc(5 + name_len + 1);

                        if (self_entry) {
                            memcpy(self_entry, "self.", 5);
                            memcpy(self_entry + 5, fn_name, name_len);
                            self_entry[5 + name_len] = '\0';
                            index_add(index, self_entry);
                            free(self_entry);
                        }
                    }
                    emit_self_fields(index, &struct_fields, &struct_registry);
                    free(fn_name);
                    cg_free_field_type(&fn_return_type);
                    continue;
                }
                free(fn_name);
                cg_free_field_type(&fn_return_type);
            }
            {
                char *fn_name = NULL;
                FieldType fn_return_type = {0};

                if (cg_parse_fn(text, &fn_name, &fn_return_type)) {
                    fn_collector_start(&fn_collector, index, blocks, block_count,
                                        in_struct, fn_name, &fn_return_type,
                                        indent, y);
                    index_add_dsl_symbol(index, blocks, block_count, fn_name);
                    if (!in_struct && fn_name) {
                        const char *dot = strchr(fn_name, '.');

                        if (dot) {
                            const StructFieldList *sf =
                                struct_registry_find(&struct_registry,
                                                     fn_name);

                            if (!sf) {
                                char *type_name = cg_copy_text(fn_name,
                                                    (size_t)(dot - fn_name));

                                sf = struct_registry_find(&struct_registry,
                                                          type_name);
                                free(type_name);
                            }
                            if (sf) {
                                emit_self_fields(index, sf,
                                                 &struct_registry);
                            }
                        }
                    }
                    free(fn_name);
                    cg_free_field_type(&fn_return_type);
                    continue;
                }
                free(fn_name);
                cg_free_field_type(&fn_return_type);
            }
            if (cg_parse_param(text, &name, &field_type, &is_meta,
                               &is_variadic)) {
                index_add(index, name);
                index_add_field_type(index, &field_type);
                free(name);
                cg_free_field_type(&field_type);
                continue;
            }
            if (cg_parse_let(text, &name, &field_type, &let_value)) {
                if (module_depth > 0 && !in_struct) {
                    modules[module_depth - 1].inner_exports = true;
                }
                index_add_dsl_symbol(index, blocks, block_count, name);
                index_add_field_type(index, &field_type);
                free(name);
                cg_free_field_type(&field_type);
                free(let_value);
                continue;
            }
            free(let_value);

            {
                char *macro_callee = NULL;
                char **macro_args = NULL;
                size_t macro_arg_count = 0;

                if (cg_parse_paren_call(text, &macro_callee, &macro_args,
                                        &macro_arg_count)) {
                    index_add(index, macro_callee);
                    if (in_struct && macro_callee && macro_arg_count > 0) {
                        size_t callee_len = strlen(macro_callee);

                        if (callee_len >= 7 &&
                            memcmp(macro_callee + callee_len - 7, ".fields",
                                   7) == 0) {
                            for (size_t i = 0; i < macro_arg_count; i++) {
                                const char *arg = macro_args[i];
                                size_t arg_len = strlen(arg);
                                const char *last_dot = NULL;

                                for (size_t j = 0; j < arg_len; j++) {
                                    if (arg[j] == '.') {
                                        last_dot = arg + j;
                                    }
                                }
                                if (last_dot && last_dot[1] != '\0') {
                                    struct_field_list_add(
                                        &struct_fields,
                                        last_dot + 1, NULL);
                                }
                            }
                        }
                    }
                    free(macro_callee);
                    cg_free_cstr_array(macro_args, macro_arg_count);
                    continue;
                }
            }

            {
                const char *as = strstr(text, " as ");

                if (as != NULL) {
                    index_scan_dotted_token(index, as + 4,
                                            length - (size_t) (as + 4 - line));
                }
            }
            continue;
        }

        if (block_count >= sizeof(blocks) / sizeof(blocks[0])) {
            free(name);
            attribute_noscope = false;
            continue;
        }
        blocks[block_count] = (Block) {
            kind, indent, name, attribute_noscope, false, false, {0}, NULL
        };
        block_count++;

        if (kind == BLOCK_PACKAGE && block_count > 1) {
            char *parent_path = build_block_path(blocks, block_count - 1);

            if (parent_path) {
                index_add_child(index, parent_path, name);
                free(parent_path);
            }
        }

        if (kind == BLOCK_MODULE && module_depth < sizeof(modules) / sizeof(modules[0])) {
            char *parent_path = build_block_path(blocks, block_count - 1);
            char *dsl_path = build_block_path(blocks, block_count);

            if (parent_path) {
                index_add_child(index, parent_path, name);
                free(parent_path);
            }
            modules[module_depth++] = (ModuleFrame) {
                dsl_path, false, false
            };
        }

        attribute_noscope = false;
    }

    free(enum_output.local_name);
    free(enum_output.dsl_name);
    fn_collector_finish(&fn_collector, index);
    pop_blocks(blocks, &block_count, 0, index, modules, &module_depth);
    while (block_count > 0) {
        block_count--;
        free(blocks[block_count].name);
    }
    struct_field_list_free(&struct_fields);
    struct_registry_free(&struct_registry);
}

bool ide_index_completion_token_at(const char *line, size_t cursor,
                                   const char **token, size_t *token_length)
{
    size_t start;
    size_t end = cursor;

    if (!token || !token_length || !line || cursor == 0) {
        return false;
    }
    start = end;
    while (start > 0) {
        unsigned char ch = (unsigned char) line[start - 1];

        if (cg_name_char(ch) || line[start - 1] == '.') {
            start--;
            continue;
        }
        if (line[start - 1] == '(' || line[start - 1] == ',') {
            break;
        }
        break;
    }
    if (start >= end || !cg_name_start((unsigned char) line[start])) {
        return false;
    }
    *token = line + start;
    *token_length = end - start;
    return true;
}

bool ide_index_completion_token(const char *line, size_t length,
                                const char **token, size_t *token_length)
{
    size_t content_end = length;

    if (!token || !token_length || !line) {
        return false;
    }
    while (content_end > 0 && line[content_end - 1] == ' ') {
        content_end--;
    }
    if (content_end == 0) {
        return false;
    }
    return ide_index_completion_token_at(line, content_end, token, token_length);
}

static const char *ghost_from_children(const IdeIndex *index,
                                       const char *parent, size_t parent_length,
                                       const char *partial, size_t partial_length)
{
    static char suffix[256];
    const char *best = NULL;
    size_t best_length = 0;

    for (size_t i = 0; i < index->child_count; i++) {
        const IdeScopeChild *edge = &index->children[i];
        size_t child_length = strlen(edge->child);

        if (strlen(edge->parent) != parent_length ||
            memcmp(edge->parent, parent, parent_length) != 0) {
            continue;
        }
        if (partial_length > child_length ||
            memcmp(edge->child, partial, partial_length) != 0) {
            continue;
        }
        if (!best || strcmp(edge->child, best) < 0) {
            best = edge->child;
            best_length = child_length;
        }
    }
    if (!best || best_length <= partial_length) {
        return NULL;
    }
    if (partial_length > 0 && partial_length >= best_length - 1) {
        return NULL;
    }
    if (best_length - partial_length >= sizeof(suffix)) {
        return NULL;
    }
    memcpy(suffix, best + partial_length, best_length - partial_length + 1);
    return suffix;
}

const char *ide_index_ghost_suffix(const IdeIndex *index, const char *token,
                                   size_t token_length)
{
    static char suffix[256];
    const char *best = NULL;
    size_t suffix_length;
    size_t parent_length = 0;
    const char *last_dot = NULL;

    if (!index || !token || token_length == 0) {
        return NULL;
    }

    for (size_t i = 0; i < token_length; i++) {
        if (token[i] == '.') {
            last_dot = token + i;
        }
    }

    if (last_dot != NULL) {
        parent_length = (size_t) (last_dot - token);
        if (index_is_terminal(index, token, parent_length) &&
            !index_has_children(index, token, parent_length)) {
            return NULL;
        }
        if (last_dot + 1 == token + token_length) {
            return ghost_from_children(index, token, parent_length, "", 0);
        }
        {
            const char *partial = last_dot + 1;
            size_t partial_length = token_length - parent_length - 1;
            const char *child_suffix =
                ghost_from_children(index, token, parent_length, partial,
                                    partial_length);

            if (child_suffix) {
                return child_suffix;
            }
        }
    }

    for (size_t i = 0; i < index->count; i++) {
        const char *entry = index->entries[i];
        size_t entry_length = strlen(entry);

        if (index_extends_terminal(index, entry)) {
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
    if (!best) {
        return NULL;
    }
    suffix_length = strlen(best + token_length);
    if (suffix_length == 0 || suffix_length >= sizeof(suffix)) {
        return NULL;
    }
    memcpy(suffix, best + token_length, suffix_length + 1);
    return suffix;
}

bool ide_index_reference_known(const IdeIndex *index, const char *reference,
                               size_t length, bool allow_prefix)
{
    if (!index || !reference || length == 0) {
        return false;
    }
    for (size_t i = 0; i < index->child_count; i++) {
        const IdeScopeChild *edge = &index->children[i];
        size_t parent_length = strlen(edge->parent);
        size_t child_length = strlen(edge->child);
        size_t full_length = parent_length + 1 + child_length;

        if (full_length != length) {
            continue;
        }
        if (memcmp(reference, edge->parent, parent_length) != 0 ||
            reference[parent_length] != '.' ||
            memcmp(reference + parent_length + 1, edge->child,
                   child_length) != 0) {
            continue;
        }
        return true;
    }
    for (size_t i = 0; i < index->count; i++) {
        const char *entry = index->entries[i];
        size_t entry_length = strlen(entry);

        if (entry_length == length &&
            memcmp(entry, reference, length) == 0) {
            return true;
        }
        if (allow_prefix && entry_length > length &&
            memcmp(entry, reference, length) == 0) {
            return true;
        }
    }
    return false;
}

static bool scope_path_at_line(const IdeIndexRow *rows, size_t row_count,
                               size_t line_index, char *out, size_t out_size)
{
    Block blocks[48];
    size_t block_count = 0;
    bool attribute_noscope = false;
    size_t struct_indent = 0;
    bool in_struct = false;

    if (!out || out_size == 0) {
        return false;
    }
    out[0] = '\0';
    if (!rows || line_index >= row_count) {
        return false;
    }

    for (size_t y = 0; y < line_index; y++) {
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
        }
        while (block_count > 0 && blocks[block_count - 1].indent >= indent) {
            free(blocks[block_count - 1].name);
            block_count--;
        }
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
            if (block_count > 0 && blocks[block_count - 1].kind == BLOCK_MODULE &&
                cg_is_module_export_name(name)) {
                struct_indent = indent;
                in_struct = true;
            }
            free(name);
            continue;
        } else {
            attribute_noscope = false;
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

    if (block_count == 0) {
        return false;
    }
    {
        char *path = build_block_path(blocks, block_count);

        if (!path) {
            while (block_count > 0) {
                free(blocks[--block_count].name);
            }
            return false;
        }
        snprintf(out, out_size, "%s", path);
        free(path);
    }
    while (block_count > 0) {
        free(blocks[--block_count].name);
    }
    return out[0] != '\0';
}

bool ide_index_scope_path(const IdeIndexRow *rows, size_t row_count,
                          size_t line_index, char *out, size_t out_size)
{
    return scope_path_at_line(rows, row_count, line_index, out, out_size);
}

const char *ide_index_scoped_ghost_suffix(const IdeIndex *index,
                                          const char *scope, const char *token,
                                          size_t token_length)
{
    const char *suffix = ide_index_ghost_suffix(index, token, token_length);
    char qualified[256];
    char scope_copy[256];

    if (suffix && suffix[0] != '\0') {
        return suffix;
    }
    if (!scope || !scope[0] || !token || token_length == 0) {
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
        suffix = ide_index_ghost_suffix(index, qualified, strlen(qualified));
        if (suffix && suffix[0] != '\0') {
            return suffix;
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

const char *ide_index_fn_hint(const IdeIndex *index, const char *callee)
{
    if (!index || !callee || !callee[0]) {
        return NULL;
    }
    for (size_t i = 0; i < index->fn_hint_count; i++) {
        if (strcmp(index->fn_hints[i].key, callee) == 0) {
            return index->fn_hints[i].hint;
        }
    }
    return NULL;
}

const char *ide_index_fn_hint_scoped(const IdeIndex *index, const char *scope,
                                     const char *callee)
{
    const char *hint = ide_index_fn_hint(index, callee);
    char qualified[256];
    char scope_copy[256];

    if (hint) {
        return hint;
    }
    if (!scope || !scope[0] || !callee || !callee[0] || strchr(callee, '.')) {
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
        hint = ide_index_fn_hint(index, qualified);
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
