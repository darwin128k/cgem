#define _POSIX_C_SOURCE 200809L

#include "cgem/semantic.h"

#include "cgem/compiler_internal.h"
#include "cgem/platform.h"

#include <stdio.h>
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

static bool paths_equal(const char *a, const char *b)
{
    if (!a || !b) {
        return a == b;
    }
    return strcmp(a, b) == 0;
}

static bool definition_exists(const CgemSemantic *semantic,
                              const char *dsl_name, const char *file_path)
{
    size_t i;

    for (i = 0; i < semantic->definition_count; i++) {
        const CgemSemanticDefinition *def = &semantic->definitions[i];

        if (def->dsl_name && strcmp(def->dsl_name, dsl_name) == 0 &&
            paths_equal(def->file_path, file_path)) {
            return true;
        }
    }
    return false;
}

static int append_definition(CgemSemantic *semantic,
                             CgemSemanticDefinition *definition)
{
    CgemSemanticDefinition *grown;

    if (!definition || !definition->dsl_name) {
        return 0;
    }
    if (definition_exists(semantic, definition->dsl_name,
                          definition->file_path)) {
        free(definition->dsl_name);
        free(definition->file_path);
        return 0;
    }
    grown = realloc(semantic->definitions,
                    (semantic->definition_count + 1) *
                        sizeof(*semantic->definitions));
    if (!grown) {
        free(definition->dsl_name);
        free(definition->file_path);
        return -1;
    }
    semantic->definitions = grown;
    semantic->definitions[semantic->definition_count++] = *definition;
    return 0;
}

static bool is_dsl_keyword(const char *name)
{
    static const char *const keywords[] = {
        "package", "module", "scope", "type", "enum", "struct", "fn", "let",
        "case", "field", "param", "return", "if", "elif", "else", "self",
        NULL
    };
    size_t i;

    for (i = 0; keywords[i]; i++) {
        if (strcmp(keywords[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static char *declaration_dsl_name(Block *blocks, size_t block_count,
                                  const char *decl_name)
{
    char *export_name;
    char *dsl_name;

    export_name = cg_resolve_export_name(blocks, block_count, decl_name);
    if (!export_name) {
        return NULL;
    }
    dsl_name = cg_build_dsl_name(blocks, block_count, export_name);
    free(export_name);
    return dsl_name;
}

static void record_definition(CgemSemantic *semantic, const char *dsl_name,
                              size_t line, size_t column,
                              const char *file_path)
{
    CgemSemanticDefinition def;

    if (!semantic || !dsl_name || !dsl_name[0] || is_dsl_keyword(dsl_name)) {
        return;
    }
    def.dsl_name = strdup(dsl_name);
    def.line = line;
    def.column = column;
    def.file_path = file_path ? strdup(file_path) : NULL;
    if (!def.dsl_name) {
        free(def.file_path);
        return;
    }
    if (append_definition(semantic, &def) != 0) {
        free(def.dsl_name);
        free(def.file_path);
    }
}

void cgem_semantic_index_definitions(const IdeIndexRow *rows, size_t row_count,
                                     const char *current_file,
                                     CgemSemantic *semantic)
{
    Block blocks[48];
    size_t block_count = 0;
    bool attribute_noscope = false;
    char *enum_dsl_name = NULL;
    size_t struct_indent = 0;
    bool in_struct = false;
    size_t y;

    if (!rows || !semantic) {
        return;
    }
    for (y = 0; y < row_count; y++) {
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
            free(blocks[--block_count].name);
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
            free(name);
            if (block_count > 0 &&
                blocks[block_count - 1].kind == BLOCK_MODULE) {
                struct_indent = indent;
                in_struct = true;
            }
            continue;
        } else {
            char *base = NULL;
            FieldType field_type = {0};
            char *let_value = NULL;
            char *type_reference = NULL;
            char *case_value = NULL;

            if (cg_parse_type(text, &name, NULL, NULL, &type_reference, NULL,
                              NULL)) {
                char *dsl = declaration_dsl_name(blocks, block_count, name);

                record_definition(semantic, dsl, y + 1, indent + 1, current_file);
                free(dsl);
                free(name);
                free(type_reference);
                continue;
            }
            if (cg_parse_enum(text, &name, &base)) {
                char *dsl = declaration_dsl_name(blocks, block_count, name);

                free(enum_dsl_name);
                enum_dsl_name = dsl ? strdup(dsl) : NULL;
                record_definition(semantic, dsl, y + 1, indent + 1, current_file);
                free(dsl);
                free(name);
                free(base);
                continue;
            }
            if (cg_parse_case(text, &name, &case_value)) {
                if (enum_dsl_name && name) {
                    size_t member_length =
                        strlen(enum_dsl_name) + 1 + strlen(name) + 1;
                    char *member = malloc(member_length);

                    if (member) {
                        snprintf(member, member_length, "%s.%s", enum_dsl_name,
                                 name);
                        record_definition(semantic, member, y + 1, indent + 1,
                                          current_file);
                        free(member);
                    }
                }
                free(name);
                free(case_value);
                continue;
            }
            if (cg_parse_fn(text, &name, &field_type)) {
                char *dsl = declaration_dsl_name(blocks, block_count, name);

                record_definition(semantic, dsl, y + 1, indent + 1, current_file);
                free(dsl);
                free(name);
                cg_free_field_type(&field_type);
                continue;
            }
            if (cg_parse_let(text, &name, &field_type, &let_value)) {
                char *dsl = declaration_dsl_name(blocks, block_count, name);

                record_definition(semantic, dsl, y + 1, indent + 1, current_file);
                free(dsl);
                free(name);
                cg_free_field_type(&field_type);
                free(let_value);
                continue;
            }
            if (in_struct && cg_parse_field(text, &name, &field_type)) {
                record_definition(semantic, name, y + 1, indent + 1,
                                  current_file);
                free(name);
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
    free(enum_dsl_name);
    while (block_count > 0) {
        free(blocks[--block_count].name);
    }
}

bool cgem_semantic_reference_at(const char *line, size_t length, size_t cursor,
                                char *reference, size_t reference_size,
                                size_t *start_column)
{
    size_t start;
    size_t end;

    if (!line || !reference || reference_size == 0 || cursor > length) {
        return false;
    }
    if (cursor >= length) {
        if (length == 0) {
            return false;
        }
        cursor = length - 1;
    }
    if (!cg_name_start((unsigned char) line[cursor]) &&
        !(line[cursor] == '.' && cursor + 1 < length &&
          cg_name_start((unsigned char) line[cursor + 1]))) {
        if (cursor == 0) {
            return false;
        }
        cursor--;
    }
    start = cursor;
    while (start > 0) {
        unsigned char ch = (unsigned char) line[start - 1];

        if (cg_name_char(ch) || line[start - 1] == '.') {
            start--;
            continue;
        }
        break;
    }
    end = cursor + 1;
    while (end < length) {
        unsigned char ch = (unsigned char) line[end];

        if (cg_name_char(ch) || line[end] == '.') {
            end++;
            continue;
        }
        break;
    }
    while (end > start && line[end - 1] == '.') {
        end--;
    }
    if (end <= start || (size_t) (end - start) >= reference_size) {
        return false;
    }
    if (!cg_name_start((unsigned char) line[start])) {
        return false;
    }
    memcpy(reference, line + start, end - start);
    reference[end - start] = '\0';
    if (start_column) {
        *start_column = start + 1;
    }
    return reference[0] != '\0';
}

bool cgem_semantic_qualify_reference(const char *reference, const char *scope,
                                     const CgemSemantic *semantic, char *out,
                                     size_t out_size)
{
    char scope_copy[256];
    size_t reference_length;

    if (!reference || !out || out_size == 0) {
        return false;
    }
    reference_length = strlen(reference);
    if (reference_length + 1 > out_size) {
        return false;
    }
    memcpy(out, reference, reference_length + 1);
    if (strchr(reference, '.')) {
        return cgem_semantic_find(semantic, out) != NULL ||
               cgem_semantic_reference_known(semantic, out, reference_length,
                                             false);
    }
    if (!scope || !scope[0]) {
        return cgem_semantic_find(semantic, out) != NULL;
    }
    snprintf(scope_copy, sizeof(scope_copy), "%s", scope);
    for (;;) {
        size_t scope_length = strlen(scope_copy);

        if (scope_length + 1 + reference_length + 1 > out_size) {
            return false;
        }
        snprintf(out, out_size, "%s.%s", scope_copy, reference);
        if (cgem_semantic_find(semantic, out) != NULL ||
            cgem_semantic_reference_known(semantic, out, strlen(out), false)) {
            return true;
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
    memcpy(out, reference, reference_length + 1);
    return cgem_semantic_find(semantic, out) != NULL;
}

bool cgem_semantic_find_definition(const CgemSemantic *semantic,
                                   const char *qualified_reference,
                                   size_t *line, size_t *column,
                                   char *file_path, size_t file_path_size)
{
    size_t i;
    ssize_t best_index = -1;

    if (!semantic || !qualified_reference || !line || !column) {
        return false;
    }
    for (i = 0; i < semantic->definition_count; i++) {
        const CgemSemanticDefinition *def = &semantic->definitions[i];

        if (!def->dsl_name) {
            continue;
        }
        if (strcmp(def->dsl_name, qualified_reference) == 0) {
            best_index = (ssize_t) i;
            break;
        }
    }
    if (best_index < 0) {
        return false;
    }
    *line = semantic->definitions[best_index].line;
    *column = semantic->definitions[best_index].column;
    if (file_path && file_path_size > 0) {
        if (semantic->definitions[best_index].file_path) {
            snprintf(file_path, file_path_size, "%s",
                     semantic->definitions[best_index].file_path);
        } else {
            file_path[0] = '\0';
        }
    }
    return true;
}

typedef struct {
    char **paths;
    size_t count;
    size_t capacity;
} CgemFileList;

static void file_list_free(CgemFileList *list)
{
    size_t i;

    if (!list) {
        return;
    }
    for (i = 0; i < list->count; i++) {
        free(list->paths[i]);
    }
    free(list->paths);
    *list = (CgemFileList) {0};
}

static bool file_list_add(CgemFileList *list, const char *path)
{
    char *copy;
    char **grown;
    size_t i;

    if (!list || !path) {
        return false;
    }
    for (i = 0; i < list->count; i++) {
        if (strcmp(list->paths[i], path) == 0) {
            return true;
        }
    }
    if (list->count == list->capacity) {
        size_t next = list->capacity ? list->capacity * 2 : 16;

        grown = realloc(list->paths, next * sizeof(*list->paths));
        if (!grown) {
            return false;
        }
        list->paths = grown;
        list->capacity = next;
    }
    copy = strdup(path);
    if (!copy) {
        return false;
    }
    list->paths[list->count++] = copy;
    return true;
}

static bool has_cgem_suffix(const char *path)
{
    size_t length = strlen(path);

    return length >= 5 && strcmp(path + length - 5, ".cgem") == 0;
}

static bool collect_cgem_files_cb(const char *path, void *context)
{
    CgemFileList *list = context;

    if (platform_path_is_directory(path)) {
        return true;
    }
    if (!has_cgem_suffix(path)) {
        return true;
    }
    return file_list_add(list, path);
}

static int merge_symbol(CgemSemantic *dst, CgemSemanticSymbol *symbol)
{
    size_t i;
    CgemSemanticSymbol *grown;

    if (!symbol || !symbol->dsl_name) {
        return 0;
    }
    for (i = 0; i < dst->symbol_count; i++) {
        if (dst->symbols[i].dsl_name &&
            strcmp(dst->symbols[i].dsl_name, symbol->dsl_name) == 0) {
            free(dst->symbols[i].type_dsl_name);
            dst->symbols[i].kind = symbol->kind;
            dst->symbols[i].is_define = symbol->is_define;
            dst->symbols[i].type_dsl_name = symbol->type_dsl_name;
            symbol->type_dsl_name = NULL;
            free(symbol->dsl_name);
            return 0;
        }
    }
    grown = realloc(dst->symbols,
                    (dst->symbol_count + 1) * sizeof(*dst->symbols));
    if (!grown) {
        return -1;
    }
    dst->symbols = grown;
    dst->symbols[dst->symbol_count++] = *symbol;
    return 0;
}

static int load_file_rows(const char *path, IdeIndexRow **rows_out,
                          size_t *row_count_out)
{
    FILE *input;
    char buffer[4096];
    IdeIndexRow *rows = NULL;
    size_t count = 0;
    size_t capacity = 0;

    *rows_out = NULL;
    *row_count_out = 0;
    input = fopen(path, "rb");
    if (!input) {
        return -1;
    }
    while (fgets(buffer, sizeof(buffer), input)) {
        size_t length = strlen(buffer);
        char *copy;

        while (length > 0 &&
               (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
            length--;
        }
        if (count == capacity) {
            size_t next = capacity ? capacity * 2 : 32;
            IdeIndexRow *grown = realloc(rows, next * sizeof(*rows));

            if (!grown) {
                fclose(input);
                for (size_t i = 0; i < count; i++) {
                    free((void *) rows[i].data);
                }
                free(rows);
                return -1;
            }
            rows = grown;
            capacity = next;
        }
        copy = malloc(length + 1);
        if (!copy) {
            fclose(input);
            for (size_t i = 0; i < count; i++) {
                free((void *) rows[i].data);
            }
            free(rows);
            return -1;
        }
        memcpy(copy, buffer, length);
        copy[length] = '\0';
        rows[count++] = (IdeIndexRow) { .data = copy, .length = length };
    }
    if (ferror(input)) {
        fclose(input);
        for (size_t i = 0; i < count; i++) {
            free((void *) rows[i].data);
        }
        free(rows);
        return -1;
    }
    fclose(input);
    *rows_out = rows;
    *row_count_out = count;
    return 0;
}

static void free_file_rows(IdeIndexRow *rows, size_t row_count)
{
    size_t i;

    for (i = 0; i < row_count; i++) {
        free((void *) rows[i].data);
    }
    free(rows);
}

static int analyze_file_into_semantic(const char *path, const char *compiler,
                                      CgemSemantic *semantic)
{
    FILE *input;
    CgemSemantic file_semantic;
    DiagnosticList diagnostics = {0};
    IdeIndexRow *rows = NULL;
    size_t row_count = 0;
    size_t i;
    int result;

    if (load_file_rows(path, &rows, &row_count) != 0) {
        return -1;
    }
    input = fopen(path, "rb");
    if (!input) {
        free_file_rows(rows, row_count);
        return -1;
    }
    cgem_semantic_init(&file_semantic);
    cg_diagnostic_init(&diagnostics);
    result = cgem_analyze(input, compiler, &diagnostics, &file_semantic);
    fclose(input);
    if (result != 0) {
        cgem_semantic_free(&file_semantic);
        cg_diagnostic_free(&diagnostics);
        free_file_rows(rows, row_count);
        return result;
    }
    cgem_semantic_index_definitions(rows, row_count, path, &file_semantic);
    for (i = 0; i < file_semantic.symbol_count; i++) {
        if (merge_symbol(semantic, &file_semantic.symbols[i]) != 0) {
            result = -1;
            break;
        }
        file_semantic.symbols[i].dsl_name = NULL;
        file_semantic.symbols[i].type_dsl_name = NULL;
    }
    for (i = 0; i < file_semantic.definition_count; i++) {
        CgemSemanticDefinition def = file_semantic.definitions[i];

        if (!def.file_path) {
            def.file_path = strdup(path);
        }
        if (append_definition(semantic, &def) != 0) {
            result = -1;
            break;
        }
        file_semantic.definitions[i].dsl_name = NULL;
        file_semantic.definitions[i].file_path = NULL;
    }
    cgem_semantic_free(&file_semantic);
    cg_diagnostic_free(&diagnostics);
    free_file_rows(rows, row_count);
    return result;
}

void cgem_semantic_load_workspace(const char *workspace_root,
                                  const char *skip_file, const char *compiler,
                                  CgemSemantic *semantic)
{
    CgemFileList files = {0};
    size_t i;

    if (!workspace_root || !workspace_root[0] || !compiler || !semantic) {
        return;
    }
    platform_scan_directory(workspace_root, collect_cgem_files_cb, &files);
    for (i = 0; i < files.count; i++) {
        if (skip_file && skip_file[0] && strcmp(files.paths[i], skip_file) == 0) {
            continue;
        }
        analyze_file_into_semantic(files.paths[i], compiler, semantic);
    }
    file_list_free(&files);
}
