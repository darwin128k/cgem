#define _POSIX_C_SOURCE 200809L

#include "cgem/compiler.h"
#include "cgem/common.h"
#include "cgem/compiler_internal.h"
#include "cgem/lint.h"
#include "cgem/platform.h"
#include "cgem/semantic.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CgemSemantic *cg_analyze_semantic_out;

extern void cgem_semantic_adopt_symbols(CgemSemantic *semantic, Symbol *symbols,
                                        size_t symbol_count);

static char *copy_field_type_dsl(const FieldType *type)
{
    if (!type || !type->name || type->is_param_ref) {
        return NULL;
    }
    return strdup(type->name);
}

static int flush_includes(ModuleOutput *module, IncludeAttributes *includes,
                          HeaderDeps **header_deps, size_t *header_deps_count,
                          size_t *header_deps_capacity, bool emit,
                          bool to_source, char *error, size_t error_size)
{
    if (includes->count == 0) {
        return 0;
    }
    if (!emit || !module->relative_header) {
        cg_clear_include_attributes(includes);
        return 0;
    }
    if (cg_apply_include_attributes(module, includes, header_deps,
                                    header_deps_count, header_deps_capacity,
                                    to_source, error, error_size) != 0) {
        return -1;
    }
    cg_clear_include_attributes(includes);
    return 0;
}

static char *resolve_c_field_type(FieldType *type, Symbol *symbols,
                                  size_t symbol_count, ModuleOutput *module,
                                  HeaderDeps **header_deps,
                                  size_t *header_deps_count,
                                  size_t *header_deps_capacity,
                                  size_t line_number, char *error,
                                  size_t error_size)
{
    Symbol *symbol = cg_find_symbol(symbols, symbol_count, type->name);
    const char *base;
    char *resolved;

    if (!symbol) {
        cg_set_error(error, error_size, "line %zu: unknown type: %s",
                  line_number, type->name);
        return NULL;
    }
    base = cg_binding_value(symbol, false);
    resolved = type->is_ptr ? cg_make_pointer_type(base) : strdup(base);
    if (!resolved) {
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    if (cg_module_require_include(module, symbol->header, header_deps,
                                  header_deps_count,
                                  header_deps_capacity) != 0) {
        free(resolved);
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    return resolved;
}

static bool append_text(char **text, size_t *length, size_t *capacity,
                        const char *chunk, size_t chunk_length)
{
    if (*length + chunk_length + 1 > *capacity) {
        size_t next = *capacity ? *capacity * 2 : 64;
        char *grown;

        while (next < *length + chunk_length + 1) {
            next *= 2;
        }
        grown = realloc(*text, next);
        if (!grown) {
            return false;
        }
        *text = grown;
        *capacity = next;
    }
    memcpy(*text + *length, chunk, chunk_length);
    *length += chunk_length;
    (*text)[*length] = '\0';
    return true;
}

static bool append_initializer_value(char **expression, const char *value)
{
    size_t length = *expression ? strlen(*expression) : 0;
    size_t capacity = length + 1;

    if (length > 0 &&
        !append_text(expression, &length, &capacity, ", ", 2)) {
        return false;
    }
    return append_text(expression, &length, &capacity, value, strlen(value));
}

static int check_param_type_use(const char **params, const ParamRequire *requires,
                                size_t param_count, const char *name,
                                size_t line_number, char *error,
                                size_t error_size)
{
    ssize_t index = cg_param_index(params, param_count, name);

    if (index < 0) {
        return 0;
    }
    if (requires && requires[index] == PARAM_REQUIRE_VALUE) {
        cg_set_error(error, error_size,
                     "line %zu: param %s is @require value and cannot be "
                     "used as a type",
                     line_number, name);
        return -1;
    }
    return 0;
}

static int check_param_value_use(const char **params, const ParamRequire *requires,
                                 size_t param_count, const char *name,
                                 size_t line_number, char *error,
                                 size_t error_size)
{
    ssize_t index = cg_param_index(params, param_count, name);

    if (index < 0) {
        return 0;
    }
    if (requires && requires[index] == PARAM_REQUIRE_TYPE) {
        cg_set_error(error, error_size,
                     "line %zu: param %s is @require type and cannot be "
                     "used as a value",
                     line_number, name);
        return -1;
    }
    return 0;
}

static int append_param(char ***params, bool **param_variadic,
                        ParamRequire **param_requires, size_t *param_count,
                        const char *text, bool struct_context,
                        FieldType *type, bool *is_meta, ParamRequire require,
                        size_t line_number, char *error, size_t error_size)
{
    char *name = NULL;
    bool variadic = false;
    char **grown_params;
    bool *grown_variadic;
    ParamRequire *grown_requires;

    if (!cg_parse_param(text, &name, type, is_meta, &variadic)) {
        cg_set_error(error, error_size,
                     "line %zu: expected param name, param name as ..., or "
                     "param name as <type>",
                     line_number);
        return -1;
    }
    if (struct_context && !*is_meta) {
        cg_set_error(error, error_size,
                     "line %zu: struct params cannot declare concrete types",
                     line_number);
        free(name);
        cg_free_field_type(type);
        return -1;
    }
    if (*param_count > 0 && (*param_variadic)[*param_count - 1]) {
        free(name);
        cg_free_field_type(type);
        cg_set_error(error, error_size,
                     "line %zu: variadic parameter must be last", line_number);
        return -1;
    }
    if (cg_name_in_list(*params, *param_count, name)) {
        cg_set_error(error, error_size, "line %zu: duplicate parameter: %s",
                     line_number, name);
        free(name);
        cg_free_field_type(type);
        return -1;
    }
    grown_params = realloc(*params, (*param_count + 1) * sizeof(**params));
    if (!grown_params) {
        free(name);
        cg_free_field_type(type);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    *params = grown_params;
    grown_variadic = realloc(*param_variadic,
                             (*param_count + 1) * sizeof(**param_variadic));
    if (!grown_variadic) {
        free(name);
        cg_free_field_type(type);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    *param_variadic = grown_variadic;
    grown_requires = realloc(*param_requires,
                             (*param_count + 1) * sizeof(**param_requires));
    if (!grown_requires) {
        free(name);
        cg_free_field_type(type);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    *param_requires = grown_requires;
    (*params)[*param_count] = name;
    (*param_variadic)[*param_count] = variadic;
    (*param_requires)[*param_count] = require;
    (*param_count)++;
    return 0;
}

static void rollback_param(char **params, size_t *param_count)
{
    if (*param_count == 0) {
        return;
    }
    free(params[*param_count - 1]);
    (*param_count)--;
}

static int append_parsed_param(char ***params, bool **param_variadic,
                               ParamRequire **param_requires,
                               size_t *param_count, char *name,
                               ParamRequire require, size_t line_number,
                               char *error, size_t error_size)
{
    char **grown_params;
    bool *grown_variadic;
    ParamRequire *grown_requires;

    if (*param_count > 0 && (*param_variadic)[*param_count - 1]) {
        free(name);
        cg_set_error(error, error_size,
                     "line %zu: variadic parameter must be last", line_number);
        return -1;
    }
    if (cg_name_in_list(*params, *param_count, name)) {
        cg_set_error(error, error_size, "line %zu: duplicate parameter: %s",
                     line_number, name);
        free(name);
        return -1;
    }
    grown_params = realloc(*params, (*param_count + 1) * sizeof(**params));
    if (!grown_params) {
        free(name);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    *params = grown_params;
    grown_variadic = realloc(*param_variadic,
                             (*param_count + 1) * sizeof(**param_variadic));
    if (!grown_variadic) {
        free(name);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    *param_variadic = grown_variadic;
    grown_requires = realloc(*param_requires,
                             (*param_count + 1) * sizeof(**param_requires));
    if (!grown_requires) {
        free(name);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    *param_requires = grown_requires;
    (*params)[*param_count] = name;
    (*param_variadic)[*param_count] = false;
    (*param_requires)[*param_count] = require;
    (*param_count)++;
    return 0;
}

static char *doc_attributes_to_text(const DocAttributes *docs)
{
    size_t length = 0;
    char *text;

    if (!docs || docs->entry_count == 0) {
        return NULL;
    }
    for (size_t i = 0; i < docs->entry_count; i++) {
        length += strlen(docs->entries[i].text);
        if (i + 1 < docs->entry_count) {
            length++;
        }
    }
    text = malloc(length + 1);
    if (!text) {
        return NULL;
    }
    text[0] = '\0';
    for (size_t i = 0; i < docs->entry_count; i++) {
        strcat(text, docs->entries[i].text);
        if (i + 1 < docs->entry_count) {
            strcat(text, "\n");
        }
    }
    return text;
}

static int set_struct_param_doc(StructOutput *output, size_t param_index,
                                const DocAttributes *docs, size_t line_number,
                                char *error, size_t error_size)
{
    char *text;

    if (!docs || docs->entry_count == 0) {
        return 0;
    }
    if (param_index >= output->param_count) {
        cg_set_error(error, error_size, "line %zu: internal param doc error",
                     line_number);
        return -1;
    }
    if (output->param_docs[param_index]) {
        cg_set_error(error, error_size,
                     "line %zu: param %s is already documented",
                     line_number, output->params[param_index]);
        return -1;
    }
    text = doc_attributes_to_text(docs);
    if (!text) {
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    output->param_docs[param_index] = text;
    return 0;
}

static int append_struct_param(StructOutput *output, const char *line,
                               ParamRequire require, const DocAttributes *docs,
                               size_t line_number, char *error,
                               size_t error_size)
{
    FieldType param_type = {0};
    bool param_is_meta = false;
    char **grown_docs;
    size_t param_index;

    if (append_param(&output->params, &output->param_variadic,
                     &output->param_requires, &output->param_count, line, true,
                     &param_type, &param_is_meta, require, line_number, error,
                     error_size) != 0) {
        return -1;
    }
    cg_free_field_type(&param_type);
    param_index = output->param_count - 1;
    grown_docs =
        realloc(output->param_docs, output->param_count * sizeof(char *));
    if (!grown_docs) {
        rollback_param(output->params, &output->param_count);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    output->param_docs = grown_docs;
    output->param_docs[param_index] = NULL;
    return set_struct_param_doc(output, param_index, docs, line_number, error,
                                error_size);
}

static int ensure_struct_meta_param(StructOutput *output, const char *param_name,
                                    ParamRequire require,
                                    const DocAttributes *docs,
                                    size_t line_number, char *error,
                                    size_t error_size)
{
    ssize_t index;
    char line[256];

    index = cg_param_index((const char **) output->params, output->param_count,
                           param_name);
    if (index >= 0) {
        return set_struct_param_doc(output, (size_t) index, docs, line_number,
                                    error, error_size);
    }
    snprintf(line, sizeof(line), "param %s", param_name);
    return append_struct_param(output, line, require, docs, line_number, error,
                               error_size);
}

static char *with_const_param_type(const char *c_type)
{
    size_t length;
    char *qualified;

    if (!c_type || strncmp(c_type, "const ", 6) == 0) {
        return c_type ? strdup(c_type) : NULL;
    }
    length = strlen(c_type) + strlen("const ") + 1;
    qualified = malloc(length);
    if (!qualified) {
        return NULL;
    }
    snprintf(qualified, length, "const %s", c_type);
    return qualified;
}

static bool function_local_exists(const FunctionOutput *function,
                                  const char *name)
{
    for (size_t i = 0; i < function->param_count; i++) {
        if (strcmp(function->params[i], name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < function->local_count; i++) {
        if (strcmp(function->locals[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static int add_function_local(FunctionOutput *function, char *name,
                              char *c_type, char *value, bool is_mutable,
                              bool force_used, bool used,
                              size_t line_number, char *error,
                              size_t error_size)
{
    FunctionLocal *grown;
    size_t capacity;

    if (function_local_exists(function, name)) {
        cg_set_error(error, error_size,
                  "line %zu: duplicate local variable: %s",
                  line_number, name);
        return -1;
    }
    if (function->local_count == function->local_capacity) {
        capacity = function->local_capacity ? function->local_capacity * 2 : 4;
        grown = realloc(function->locals, capacity * sizeof(*function->locals));
        if (!grown) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        function->locals = grown;
        function->local_capacity = capacity;
    }
    function->locals[function->local_count++] = (FunctionLocal) {
        name, c_type, value, is_mutable, force_used, used, line_number
    };
    return 0;
}

static int add_function_arg(FunctionOutput *function, char *value, bool is_ref,
                            char *error, size_t error_size)
{
    FunctionArg *grown;
    size_t capacity;

    if (function->arg_count == function->arg_capacity) {
        capacity = function->arg_capacity ? function->arg_capacity * 2 : 4;
        grown = realloc(function->args, capacity * sizeof(*function->args));
        if (!grown) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        function->args = grown;
        function->arg_capacity = capacity;
    }
    function->args[function->arg_count++] = (FunctionArg) {value, is_ref};
    return 0;
}

static int add_function_statement(FunctionOutput *function, char *statement,
                                  char *error, size_t error_size)
{
    char **grown;
    size_t capacity;

    if (function->statement_count == function->statement_capacity) {
        capacity = function->statement_capacity
                       ? function->statement_capacity * 2
                       : 4;
        grown = realloc(function->statements, capacity * sizeof(*grown));
        if (!grown) {
            free(statement);
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        function->statements = grown;
        function->statement_capacity = capacity;
    }
    function->statements[function->statement_count++] = statement;
    return 0;
}

typedef enum {
    METHOD_ASSIGN_NORMAL,
    METHOD_ASSIGN_OPTIONAL_WRITE,
    METHOD_ASSIGN_OPTIONAL_READ,
    METHOD_ASSIGN_NULL_COALESCE
} MethodAssignOp;

static bool parse_method_assignment(const char *text, char **lhs, char **rhs,
                                    MethodAssignOp *op)
{
    const char *at = text;
    const char *start;
    const char *end;
    MethodAssignOp parsed_op = METHOD_ASSIGN_NORMAL;

    while (*at == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) *at)) {
        return false;
    }
    start = at++;
    while (cg_name_char((unsigned char) *at) || *at == '.') {
        at++;
    }
    end = at;
    while (*at == ' ') {
        at++;
    }
    if (*at == '?' && at[1] == '?' && at[2] == '=') {
        parsed_op = METHOD_ASSIGN_NULL_COALESCE;
        at += 3;
    } else if (*at == '?' && at[1] == '=') {
        parsed_op = METHOD_ASSIGN_OPTIONAL_WRITE;
        at += 2;
    } else if (*at == '=' && at[1] == '?') {
        parsed_op = METHOD_ASSIGN_OPTIONAL_READ;
        at += 2;
    } else if (*at == '=') {
        at++;
    } else {
        return false;
    }
    while (*at == ' ') {
        at++;
    }
    if (*at == '\0') {
        return false;
    }
    *lhs = cg_copy_text(start, (size_t) (end - start));
    *rhs = strdup(at);
    if (!*lhs || !*rhs) {
        free(*lhs);
        free(*rhs);
        *lhs = NULL;
        *rhs = NULL;
        return false;
    }
    if (op) {
        *op = parsed_op;
    }
    return true;
}

static bool parse_inline_param_optional_write(const char *text, bool *pointer,
                                              bool *mutable_attr,
                                              char **param_name,
                                              FieldType *param_type, char **rhs)
{
    size_t consumed = 0;
    const char *at;

    *rhs = NULL;
    if (!cg_parse_inline_param(text, &consumed, pointer, mutable_attr, param_name,
                               param_type)) {
        return false;
    }
    at = text + consumed;
    while (*at == ' ') {
        at++;
    }
    if (at[0] != '?' || at[1] != '=') {
        free(*param_name);
        cg_free_field_type(param_type);
        *param_name = NULL;
        return false;
    }
    at += 2;
    while (*at == ' ') {
        at++;
    }
    if (*at == '\0') {
        free(*param_name);
        cg_free_field_type(param_type);
        *param_name = NULL;
        return false;
    }
    *rhs = strdup(at);
    if (!*rhs) {
        free(*param_name);
        cg_free_field_type(param_type);
        *param_name = NULL;
        return false;
    }
    return true;
}

static bool rhs_is_inline_param(const char *rhs, bool implicit_pointer,
                                char **param_name, FieldType *param_type,
                                bool *pointer, bool *mutable_attr)
{
    size_t consumed = 0;

    if (!cg_parse_inline_param(rhs, &consumed, pointer, mutable_attr, param_name,
                               param_type)) {
        return false;
    }
    while (rhs[consumed] == ' ') {
        consumed++;
    }
    if (rhs[consumed] != '\0') {
        free(*param_name);
        cg_free_field_type(param_type);
        *param_name = NULL;
        return false;
    }
    if (implicit_pointer && pointer && !*pointer) {
        *pointer = true;
    }
    return true;
}

static bool self_method_name_valid(const char *method_name)
{
    size_t i;

    if (!cg_name_start((unsigned char) method_name[0])) {
        return false;
    }
    for (i = 0; method_name[i] != '\0'; i++) {
        if (!cg_name_char((unsigned char) method_name[i])) {
            return false;
        }
    }
    return true;
}

static bool parse_self_method_call(const char *text, char **callee, char ***args,
                                   size_t *arg_count)
{
    static const char prefix[] = "self.";
    const char *at;
    const char *method_name;

    if (!cg_parse_paren_call(text, callee, args, arg_count)) {
        return false;
    }
    if (strncmp(*callee, prefix, sizeof(prefix) - 1) != 0) {
        cg_free_cstr_array(*args, *arg_count);
        free(*callee);
        *callee = NULL;
        *args = NULL;
        *arg_count = 0;
        return false;
    }
    method_name = *callee + sizeof(prefix) - 1;
    if (!self_method_name_valid(method_name)) {
        cg_free_cstr_array(*args, *arg_count);
        free(*callee);
        *callee = NULL;
        *args = NULL;
        *arg_count = 0;
        return false;
    }
    at = text;
    while (*at == ' ') {
        at++;
    }
    {
        size_t depth = 0;
        char quote = '\0';
        bool escaped = false;
        bool saw_open = false;

        while (*at) {
            char ch = *at++;

            if (quote) {
                if (ch == quote && !escaped) {
                    quote = '\0';
                }
                escaped = ch == '\\' && !escaped;
                if (ch != '\\') {
                    escaped = false;
                }
                continue;
            }
            if (ch == '"' || ch == '\'') {
                quote = ch;
            } else if (ch == '(') {
                saw_open = true;
                depth++;
            } else if (ch == ')') {
                depth--;
                if (depth == 0 && saw_open) {
                    while (*at == ' ') {
                        at++;
                    }
                    if (*at != '\0') {
                        cg_free_cstr_array(*args, *arg_count);
                        free(*callee);
                        *callee = NULL;
                        *args = NULL;
                        *arg_count = 0;
                        return false;
                    }
                    return true;
                }
            }
        }
    }
    cg_free_cstr_array(*args, *arg_count);
    free(*callee);
    *callee = NULL;
    *args = NULL;
    *arg_count = 0;
    return false;
}

static size_t scan_paren_call_span(const char *expression, size_t start)
{
    size_t at = start;
    size_t depth = 0;
    char quote = '\0';
    bool escaped = false;

    if (!cg_name_start((unsigned char) expression[at])) {
        return 0;
    }
    at++;
    while (expression[at] &&
           (cg_name_char((unsigned char) expression[at]) ||
            expression[at] == '.')) {
        at++;
    }
    while (expression[at] == ' ') {
        at++;
    }
    if (expression[at] != '(') {
        return 0;
    }
    depth = 1;
    at++;
    while (expression[at]) {
        char ch = expression[at++];

        if (quote) {
            if (ch == quote && !escaped) {
                quote = '\0';
            }
            escaped = ch == '\\' && !escaped;
            if (ch != '\\') {
                escaped = false;
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
        } else if (ch == '(' || ch == '[' || ch == '{') {
            depth++;
        } else if (ch == ')' || ch == ']' || ch == '}') {
            if (ch == ')' && depth == 1) {
                return at - start;
            }
            if (depth == 0) {
                return 0;
            }
            depth--;
        }
    }
    return 0;
}

static const char *skip_const_spaces(const char *text)
{
    while (text && *text == ' ') {
        text++;
    }
    return text;
}

static bool parse_const_integer(const char **text, long long *value)
{
    char *end = NULL;

    *text = skip_const_spaces(*text);
    if (!**text) {
        return false;
    }
    *value = strtoll(*text, &end, 0);
    if (end == *text) {
        return false;
    }
    *text = end;
    return true;
}

static bool parse_const_identifier(const char **text, char *name, size_t name_size)
{
    size_t length = 0;

    *text = skip_const_spaces(*text);
    if (!**text) {
        return false;
    }
    if (!isalpha((unsigned char) **text) && **text != '_') {
        return false;
    }
    while (**text && (isalnum((unsigned char) **text) || **text == '_')) {
        if (length + 1 >= name_size) {
            return false;
        }
        name[length++] = **text;
        (*text)++;
    }
    name[length] = '\0';
    return length > 0;
}

typedef struct {
    const char *at;
    Symbol *symbols;
    size_t symbol_count;
} ConstEvalCtx;

static bool const_eval_at_end(ConstEvalCtx *ctx)
{
    ctx->at = skip_const_spaces(ctx->at);
    return *ctx->at == '\0';
}

static bool const_eval_expr(ConstEvalCtx *ctx, long long *value);
static bool const_eval_or(ConstEvalCtx *ctx, long long *value);
static bool const_eval_and(ConstEvalCtx *ctx, long long *value);
static bool const_eval_bitor(ConstEvalCtx *ctx, long long *value);
static bool const_eval_bitxor(ConstEvalCtx *ctx, long long *value);
static bool const_eval_bitand(ConstEvalCtx *ctx, long long *value);
static bool const_eval_eq(ConstEvalCtx *ctx, long long *value);
static bool const_eval_rel(ConstEvalCtx *ctx, long long *value);
static bool const_eval_shift(ConstEvalCtx *ctx, long long *value);
static bool const_eval_add(ConstEvalCtx *ctx, long long *value);
static bool const_eval_mul(ConstEvalCtx *ctx, long long *value);
static bool const_eval_unary(ConstEvalCtx *ctx, long long *value);
static bool const_eval_primary(ConstEvalCtx *ctx, long long *value);

static bool const_eval_primary(ConstEvalCtx *ctx, long long *value)
{
    char ident[256];

    ctx->at = skip_const_spaces(ctx->at);
    if (*ctx->at == '(') {
        ctx->at++;
        if (!const_eval_expr(ctx, value)) {
            return false;
        }
        ctx->at = skip_const_spaces(ctx->at);
        if (*ctx->at != ')') {
            return false;
        }
        ctx->at++;
        return true;
    }
    if (parse_const_integer(&ctx->at, value)) {
        return true;
    }
    if (parse_const_identifier(&ctx->at, ident, sizeof(ident))) {
        Symbol *symbol = cg_find_symbol_by_c_name(ctx->symbols,
                                                  ctx->symbol_count, ident);
        ConstEvalCtx nested;

        if (!symbol || !symbol->c_expr) {
            return false;
        }
        nested = (ConstEvalCtx) {
            symbol->c_expr, ctx->symbols, ctx->symbol_count
        };
        return const_eval_expr(&nested, value) && const_eval_at_end(&nested);
    }
    return false;
}

static bool const_eval_unary(ConstEvalCtx *ctx, long long *value)
{
    ctx->at = skip_const_spaces(ctx->at);
    if (*ctx->at == '+') {
        ctx->at++;
        return const_eval_unary(ctx, value);
    }
    if (*ctx->at == '-') {
        ctx->at++;
        if (!const_eval_unary(ctx, value)) {
            return false;
        }
        *value = -*value;
        return true;
    }
    if (*ctx->at == '!') {
        ctx->at++;
        if (!const_eval_unary(ctx, value)) {
            return false;
        }
        *value = *value ? 0 : 1;
        return true;
    }
    if (*ctx->at == '~') {
        ctx->at++;
        if (!const_eval_unary(ctx, value)) {
            return false;
        }
        *value = ~*value;
        return true;
    }
    return const_eval_primary(ctx, value);
}

static bool const_eval_mul(ConstEvalCtx *ctx, long long *value)
{
    long long right;

    if (!const_eval_unary(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        if (*ctx->at == '*') {
            ctx->at++;
            if (!const_eval_unary(ctx, &right)) {
                return false;
            }
            *value *= right;
        } else if (*ctx->at == '/') {
            ctx->at++;
            if (!const_eval_unary(ctx, &right) || right == 0) {
                return false;
            }
            *value /= right;
        } else if (*ctx->at == '%') {
            ctx->at++;
            if (!const_eval_unary(ctx, &right) || right == 0) {
                return false;
            }
            *value %= right;
        } else {
            break;
        }
    }
    return true;
}

static bool const_eval_add(ConstEvalCtx *ctx, long long *value)
{
    long long right;

    if (!const_eval_mul(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        if (*ctx->at == '+') {
            ctx->at++;
            if (!const_eval_mul(ctx, &right)) {
                return false;
            }
            *value += right;
        } else if (*ctx->at == '-') {
            ctx->at++;
            if (!const_eval_mul(ctx, &right)) {
                return false;
            }
            *value -= right;
        } else {
            break;
        }
    }
    return true;
}

static bool const_eval_shift(ConstEvalCtx *ctx, long long *value)
{
    long long right;

    if (!const_eval_add(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        if (ctx->at[0] == '<' && ctx->at[1] == '<') {
            ctx->at += 2;
            if (!const_eval_add(ctx, &right)) {
                return false;
            }
            *value <<= right;
        } else if (ctx->at[0] == '>' && ctx->at[1] == '>') {
            ctx->at += 2;
            if (!const_eval_add(ctx, &right)) {
                return false;
            }
            *value >>= right;
        } else {
            break;
        }
    }
    return true;
}

static bool const_eval_rel(ConstEvalCtx *ctx, long long *value)
{
    long long right;
    int relation = 0;

    if (!const_eval_shift(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        relation = 0;
        if (ctx->at[0] == '<' && ctx->at[1] == '=') {
            relation = 1;
            ctx->at += 2;
        } else if (ctx->at[0] == '>' && ctx->at[1] == '=') {
            relation = 2;
            ctx->at += 2;
        } else if (*ctx->at == '<') {
            relation = 3;
            ctx->at++;
        } else if (*ctx->at == '>') {
            relation = 4;
            ctx->at++;
        } else {
            break;
        }
        if (!const_eval_shift(ctx, &right)) {
            return false;
        }
        switch (relation) {
        case 1:
            *value = *value <= right;
            break;
        case 2:
            *value = *value >= right;
            break;
        case 3:
            *value = *value < right;
            break;
        case 4:
            *value = *value > right;
            break;
        default:
            return false;
        }
    }
    return true;
}

static bool const_eval_eq(ConstEvalCtx *ctx, long long *value)
{
    long long right;
    bool equal = false;

    if (!const_eval_rel(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        equal = false;
        if (ctx->at[0] == '=' && ctx->at[1] == '=') {
            equal = true;
            ctx->at += 2;
        } else if (ctx->at[0] == '!' && ctx->at[1] == '=') {
            equal = false;
            ctx->at += 2;
        } else {
            break;
        }
        if (!const_eval_rel(ctx, &right)) {
            return false;
        }
        *value = equal ? (*value == right) : (*value != right);
    }
    return true;
}

static bool const_eval_bitand(ConstEvalCtx *ctx, long long *value)
{
    long long right;

    if (!const_eval_eq(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        if (*ctx->at != '&' || ctx->at[1] == '&') {
            break;
        }
        ctx->at++;
        if (!const_eval_eq(ctx, &right)) {
            return false;
        }
        *value &= right;
    }
    return true;
}

static bool const_eval_bitxor(ConstEvalCtx *ctx, long long *value)
{
    long long right;

    if (!const_eval_bitand(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        if (*ctx->at != '^') {
            break;
        }
        ctx->at++;
        if (!const_eval_bitand(ctx, &right)) {
            return false;
        }
        *value ^= right;
    }
    return true;
}

static bool const_eval_bitor(ConstEvalCtx *ctx, long long *value)
{
    long long right;

    if (!const_eval_bitxor(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        if (*ctx->at != '|' || ctx->at[1] == '|') {
            break;
        }
        ctx->at++;
        if (!const_eval_bitxor(ctx, &right)) {
            return false;
        }
        *value |= right;
    }
    return true;
}

static bool const_eval_and(ConstEvalCtx *ctx, long long *value)
{
    long long right;

    if (!const_eval_bitor(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        if (ctx->at[0] != '&' || ctx->at[1] != '&') {
            break;
        }
        ctx->at += 2;
        if (!const_eval_bitor(ctx, &right)) {
            return false;
        }
        *value = *value && right;
    }
    return true;
}

static bool const_eval_or(ConstEvalCtx *ctx, long long *value)
{
    long long right;

    if (!const_eval_and(ctx, value)) {
        return false;
    }
    for (;;) {
        ctx->at = skip_const_spaces(ctx->at);
        if (ctx->at[0] != '|' || ctx->at[1] != '|') {
            break;
        }
        ctx->at += 2;
        if (!const_eval_and(ctx, &right)) {
            return false;
        }
        *value = *value || right;
    }
    return true;
}

static bool const_eval_expr(ConstEvalCtx *ctx, long long *value)
{
    return const_eval_or(ctx, value);
}

static bool eval_const_integer_expr(const char *text, Symbol *symbols,
                                    size_t symbol_count, long long *value)
{
    ConstEvalCtx ctx = { text, symbols, symbol_count };

    return const_eval_expr(&ctx, value) && const_eval_at_end(&ctx);
}

static StructConstructor *compile_constructors = NULL;
static size_t compile_constructor_count = 0;
static size_t compile_constructor_capacity = 0;

static void reset_compile_constructors(void)
{
    for (size_t i = 0; i < compile_constructor_count; i++) {
        free(compile_constructors[i].dsl_name);
        free(compile_constructors[i].c_call_name);
    }
    free(compile_constructors);
    compile_constructors = NULL;
    compile_constructor_count = 0;
    compile_constructor_capacity = 0;
}

static int add_compile_constructor(const char *dsl_name, const char *c_call_name)
{
    if (compile_constructor_count == compile_constructor_capacity) {
        size_t next = compile_constructor_capacity
                          ? compile_constructor_capacity * 2
                          : 4;
        StructConstructor *grown =
            realloc(compile_constructors, next * sizeof(*compile_constructors));

        if (!grown) {
            return -1;
        }
        compile_constructors = grown;
        compile_constructor_capacity = next;
    }
    compile_constructors[compile_constructor_count].dsl_name = strdup(dsl_name);
    compile_constructors[compile_constructor_count].c_call_name =
        strdup(c_call_name);
    if (!compile_constructors[compile_constructor_count].dsl_name ||
        !compile_constructors[compile_constructor_count].c_call_name) {
        free(compile_constructors[compile_constructor_count].dsl_name);
        free(compile_constructors[compile_constructor_count].c_call_name);
        return -1;
    }
    compile_constructor_count++;
    return 0;
}

static const StructConstructor *find_compile_constructor(const char *dsl_name)
{
    for (size_t i = compile_constructor_count; i > 0; i--) {
        if (strcmp(compile_constructors[i - 1].dsl_name, dsl_name) == 0) {
            return &compile_constructors[i - 1];
        }
    }
    return NULL;
}

static char *transform_function_expression(
    const char *expression, FunctionOutput *function, Symbol *symbols,
    size_t symbol_count, ModuleOutput *module, HeaderDeps **header_deps,
    size_t *header_deps_count, size_t *header_deps_capacity,
    size_t line_number, char *error, size_t error_size);

static char *transform_self_method_call(
    const char *method_callee, char **raw_args, size_t raw_arg_count,
    FunctionOutput *function, Symbol *symbols, size_t symbol_count,
    ModuleOutput *module, HeaderDeps **header_deps, size_t *header_deps_count,
    size_t *header_deps_capacity, size_t line_number, char *error,
    size_t error_size)
{
    static const char prefix[] = "self.";
    const char *method_name;
    char *dsl_name;
    Symbol *symbol;
    char *out = NULL;
    size_t out_length = 0;
    size_t out_capacity = 0;

    method_name = method_callee + sizeof(prefix) - 1;
    dsl_name = malloc(strlen(function->struct_dsl_name) + strlen(method_name) + 2);
    if (!dsl_name) {
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    snprintf(dsl_name, strlen(function->struct_dsl_name) + strlen(method_name) + 2,
             "%s.%s", function->struct_dsl_name, method_name);
    symbol = cg_find_symbol(symbols, symbol_count, dsl_name);
    free(dsl_name);
    if (!symbol) {
        cg_set_error(error, error_size,
                     "line %zu: unknown struct method: %s", line_number,
                     method_name);
        return NULL;
    }
    if (symbol->is_mutable && !function->self_mutable) {
        cg_set_error(error, error_size,
                     "line %zu: calling mutable method %s requires @mutable fn",
                     line_number, method_name);
        return NULL;
    }
    if (!symbol->is_internal &&
        cg_module_require_include(module, symbol->header, header_deps,
                                  header_deps_count, header_deps_capacity) != 0) {
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    if (!append_text(&out, &out_length, &out_capacity, symbol->c_name,
                     strlen(symbol->c_name)) ||
        !append_text(&out, &out_length, &out_capacity, "(self", 5)) {
        free(out);
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    for (size_t i = 0; i < raw_arg_count; i++) {
        char *transformed = transform_function_expression(
            raw_args[i], function, symbols, symbol_count, module, header_deps,
            header_deps_count, header_deps_capacity, line_number, error,
            error_size);

        if (!transformed) {
            free(out);
            return NULL;
        }
        if (!append_text(&out, &out_length, &out_capacity, ", ", 2) ||
            !append_text(&out, &out_length, &out_capacity, transformed,
                         strlen(transformed))) {
            free(transformed);
            free(out);
            cg_set_error(error, error_size, "out of memory");
            return NULL;
        }
        free(transformed);
    }
    if (!append_text(&out, &out_length, &out_capacity, ")", 1)) {
        free(out);
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    return out;
}

static int process_function_param_line(
    FunctionOutput *function_output, const char *line_text, Symbol *symbols,
    size_t symbol_count, ModuleOutput *module, HeaderDeps **header_deps,
    size_t *header_deps_count, size_t *header_deps_capacity, size_t line_number,
    char *error, size_t error_size)
{
    FieldType param_type = {0};
    bool param_is_meta = false;
    char *param_c_type = NULL;
    char **grown_types;
    ParamRequire require = PARAM_REQUIRE_ANY;
    bool param_mutable = function_output->param_mutable_pending;
    bool param_pointer = function_output->param_pointer_pending;

    function_output->param_mutable_pending = false;
    function_output->param_pointer_pending = false;

    if (function_output->pending_param_require) {
        require = function_output->pending_param_require_kind;
        function_output->pending_param_require = false;
    }
    if (function_output->has_return || function_output->local_count > 0 ||
        function_output->local_mutable || function_output->local_used) {
        cg_set_error(error, error_size,
                     "line %zu: params must precede the function body",
                     line_number);
        return -1;
    }
    if (append_param(&function_output->params, &function_output->param_variadic,
                     &function_output->param_requires,
                     &function_output->param_count, line_text, false,
                     &param_type, &param_is_meta, require, line_number, error,
                     error_size) != 0) {
        return -1;
    }
    if (param_pointer) {
        if (param_is_meta) {
            cg_free_field_type(&param_type);
            rollback_param(function_output->params,
                           &function_output->param_count);
            cg_set_error(error, error_size,
                         "line %zu: @pointer cannot apply to meta param",
                         line_number);
            return -1;
        }
        param_type.is_ptr = true;
    }
    if (!param_is_meta) {
        if (cg_param_c_type((const char **) function_output->params,
                            function_output->param_variadic,
                            function_output->param_count - 1,
                            param_type.name)) {
            if (check_param_type_use(
                    (const char **) function_output->params,
                    function_output->param_requires,
                    function_output->param_count - 1, param_type.name,
                    line_number, error, error_size) != 0) {
                cg_free_field_type(&param_type);
                rollback_param(function_output->params,
                               &function_output->param_count);
                return -1;
            }
            param_c_type = strdup(param_type.name);
        } else {
            param_c_type = resolve_c_field_type(
                &param_type, symbols, symbol_count, module, header_deps,
                header_deps_count, header_deps_capacity, line_number, error,
                error_size);
        }
        if (!param_c_type) {
            cg_free_field_type(&param_type);
            rollback_param(function_output->params,
                           &function_output->param_count);
            return -1;
        }
        if (!param_mutable) {
            char *qualified = with_const_param_type(param_c_type);

            free(param_c_type);
            if (!qualified) {
                cg_free_field_type(&param_type);
                rollback_param(function_output->params,
                               &function_output->param_count);
                cg_set_error(error, error_size, "out of memory");
                return -1;
            }
            param_c_type = qualified;
        }
    }
    cg_free_field_type(&param_type);
    grown_types = realloc(
        function_output->param_types,
        function_output->param_count * sizeof(*function_output->param_types));
    if (!grown_types) {
        free(param_c_type);
        rollback_param(function_output->params, &function_output->param_count);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    function_output->param_types = grown_types;
    function_output->param_types[function_output->param_count - 1] =
        param_c_type;
    if (param_is_meta) {
        function_output->has_meta_params = true;
    }
    return 0;
}

static int process_inline_function_param(
    FunctionOutput *function_output, char *param_name, FieldType *param_type,
    bool param_pointer, bool param_mutable, Symbol *symbols,
    size_t symbol_count, ModuleOutput *module, HeaderDeps **header_deps,
    size_t *header_deps_count, size_t *header_deps_capacity, size_t line_number,
    char *error, size_t error_size)
{
    char *param_c_type = NULL;
    char **grown_types;
    FieldType resolved_type = *param_type;

    if (append_parsed_param(&function_output->params,
                            &function_output->param_variadic,
                            &function_output->param_requires,
                            &function_output->param_count, param_name,
                            PARAM_REQUIRE_ANY, line_number, error,
                            error_size) != 0) {
        return -1;
    }
    if (param_pointer) {
        resolved_type.is_ptr = true;
    }
    if (cg_param_c_type((const char **) function_output->params,
                        function_output->param_variadic,
                        function_output->param_count - 1, resolved_type.name)) {
        if (check_param_type_use(
                (const char **) function_output->params,
                function_output->param_requires,
                function_output->param_count - 1, resolved_type.name,
                line_number, error, error_size) != 0) {
            rollback_param(function_output->params,
                           &function_output->param_count);
            return -1;
        }
        param_c_type = strdup(resolved_type.name);
    } else {
        param_c_type = resolve_c_field_type(
            &resolved_type, symbols, symbol_count, module, header_deps,
            header_deps_count, header_deps_capacity, line_number, error,
            error_size);
    }
    if (!param_c_type) {
        rollback_param(function_output->params, &function_output->param_count);
        return -1;
    }
    if (!param_mutable) {
        char *qualified = with_const_param_type(param_c_type);

        free(param_c_type);
        if (!qualified) {
            rollback_param(function_output->params,
                           &function_output->param_count);
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        param_c_type = qualified;
    }
    grown_types = realloc(
        function_output->param_types,
        function_output->param_count * sizeof(*function_output->param_types));
    if (!grown_types) {
        free(param_c_type);
        rollback_param(function_output->params, &function_output->param_count);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    function_output->param_types = grown_types;
    function_output->param_types[function_output->param_count - 1] = param_c_type;
    return 0;
}

static int finalize_call_block(
    FunctionOutput *function_output, Symbol *symbols, size_t symbol_count,
    ModuleOutput *module, HeaderDeps **header_deps, size_t *header_deps_count,
    size_t *header_deps_capacity, size_t line_number, char *error,
    size_t error_size)
{
    char *method_name;
    char *callee = NULL;
    char **call_args = NULL;
    char *statement;
    size_t start;
    size_t count;

    if (!function_output->call_block_method) {
        return 0;
    }
    method_name = function_output->call_block_method;
    function_output->call_block_method = NULL;
    start = function_output->call_block_param_start;
    count = function_output->param_count - start;
    if (count == 0) {
        free(method_name);
        cg_set_error(error, error_size,
                     "line %zu: call block requires at least one param",
                     line_number);
        return -1;
    }
    call_args = calloc(count, sizeof(*call_args));
    if (!call_args) {
        free(method_name);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        call_args[i] = strdup(function_output->params[start + i]);
        if (!call_args[i]) {
            cg_free_cstr_array(call_args, i);
            free(method_name);
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
    }
    callee = malloc(strlen("self.") + strlen(method_name) + 1);
    if (!callee) {
        cg_free_cstr_array(call_args, count);
        free(method_name);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    snprintf(callee, strlen("self.") + strlen(method_name) + 1, "self.%s",
             method_name);
    free(method_name);
    statement = transform_self_method_call(
        callee, call_args, count, function_output, symbols, symbol_count,
        module, header_deps, header_deps_count, header_deps_capacity,
        line_number, error, error_size);
    free(callee);
    cg_free_cstr_array(call_args, count);
    if (!statement) {
        return -1;
    }
    if (add_function_statement(function_output, statement, error, error_size) !=
        0) {
        free(statement);
        return -1;
    }
    return 0;
}

static char *transform_method_lvalue(const char *lhs, char *error,
                                     size_t error_size)
{
    static const char prefix[] = "self.";
    size_t prefix_length = sizeof(prefix) - 1;
    size_t field_length;
    char *out;

    if (strncmp(lhs, prefix, prefix_length) != 0 ||
        !cg_name_start((unsigned char) lhs[prefix_length])) {
        cg_set_error(error, error_size,
                     "struct method assignment must target self.field");
        return NULL;
    }
    field_length = prefix_length;
    while (cg_name_char((unsigned char) lhs[field_length])) {
        field_length++;
    }
    if (lhs[field_length] != '\0') {
        cg_set_error(error, error_size,
                     "struct method assignment must target self.field");
        return NULL;
    }
    out = malloc(prefix_length + field_length + 2);
    if (!out) {
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    snprintf(out, prefix_length + field_length + 2, "self->%s",
             lhs + prefix_length);
    return out;
}

static char *infer_method_return_type(
    const char *expr, const char *struct_dsl, Symbol *symbols,
    size_t symbol_count, ModuleOutput *module, HeaderDeps **header_deps,
    size_t *header_deps_count, size_t *header_deps_capacity,
    size_t line_number, char *error, size_t error_size)
{
    static const char prefix[] = "self.";
    size_t prefix_length = sizeof(prefix) - 1;
    size_t field_length;
    FieldType field_type = {0};
    char *dsl_name;
    char *c_type;

    if (strncmp(expr, prefix, prefix_length) != 0 ||
        !cg_name_start((unsigned char) expr[prefix_length])) {
        cg_set_error(error, error_size,
                     "line %zu: method return type must be declared with "
                     "`fn name as type:` or use `return self.field`",
                     line_number);
        return NULL;
    }
    field_length = prefix_length;
    while (cg_name_char((unsigned char) expr[field_length])) {
        field_length++;
    }
    if (expr[field_length] != '\0') {
        cg_set_error(error, error_size,
                     "line %zu: method return type must be declared with "
                     "`fn name as type:` or use `return self.field`",
                     line_number);
        return NULL;
    }
    dsl_name = malloc(strlen(struct_dsl) + field_length - prefix_length + 2);
    if (!dsl_name) {
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    snprintf(dsl_name, strlen(struct_dsl) + field_length - prefix_length + 2,
             "%s.%s", struct_dsl, expr + prefix_length);
    field_type.name = dsl_name;
    c_type = resolve_c_field_type(&field_type, symbols, symbol_count, module,
                                  header_deps, header_deps_count,
                                  header_deps_capacity, line_number, error,
                                  error_size);
    free(dsl_name);
    return c_type;
}

static int register_struct_known_field(StructOutput *output, const char *name,
                                       bool is_mutable, char *error,
                                       size_t error_size)
{
    StructKnownField *grown;
    size_t capacity;

    for (size_t i = 0; i < output->known_field_count; i++) {
        if (strcmp(output->known_fields[i].name, name) == 0) {
            return 0;
        }
    }
    if (output->known_field_count == output->known_field_capacity) {
        capacity = output->known_field_capacity
                       ? output->known_field_capacity * 2
                       : 4;
        grown = realloc(output->known_fields,
                        capacity * sizeof(*output->known_fields));
        if (!grown) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        output->known_fields = grown;
        output->known_field_capacity = capacity;
    }
    output->known_fields[output->known_field_count].name = strdup(name);
    if (!output->known_fields[output->known_field_count].name) {
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    output->known_fields[output->known_field_count].is_mutable = is_mutable;
    output->known_field_count++;
    return 0;
}

static int register_struct_template_fields(
    StructOutput *output, const StructTemplate *template, char *error,
    size_t error_size)
{
    for (size_t i = 0; i < template->field_count; i++) {
        bool is_mutable = output->all_mutable || template->all_mutable ||
                          template->fields[i].is_mutable;

        if (register_struct_known_field(output, template->fields[i].name,
                                        is_mutable, error, error_size) != 0) {
            return -1;
        }
    }
    return 0;
}

static const StructKnownField *
find_struct_known_field(const StructKnownField *fields, size_t count,
                        const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(fields[i].name, name) == 0) {
            return &fields[i];
        }
    }
    return NULL;
}

static const char *self_field_name_from_lvalue(const char *lhs)
{
    static const char prefix[] = "self.";

    if (strncmp(lhs, prefix, sizeof(prefix) - 1) != 0 ||
        !cg_name_start((unsigned char) lhs[sizeof(prefix) - 1])) {
        return NULL;
    }
    return lhs + sizeof(prefix) - 1;
}

static bool c_type_is_pointer(const char *c_type)
{
    size_t length;

    if (!c_type || c_type[0] == '\0') {
        return false;
    }
    length = strlen(c_type);
    while (length > 0 &&
           (c_type[length - 1] == ' ' || c_type[length - 1] == '\t')) {
        length--;
    }
    return length > 0 && c_type[length - 1] == '*';
}

static const char *function_binding_c_type(const FunctionOutput *function,
                                           const char *name)
{
    for (size_t i = 0; i < function->param_count; i++) {
        if (strcmp(function->params[i], name) == 0) {
            return function->param_types[i];
        }
    }
    for (size_t i = 0; i < function->local_count; i++) {
        if (strcmp(function->locals[i].name, name) == 0) {
            return function->locals[i].c_type;
        }
    }
    return NULL;
}

static const char *resolve_simple_lvalue_c_type(const char *lvalue,
                                                const FunctionOutput *function)
{
    const char *field_name;

    field_name = self_field_name_from_lvalue(lvalue);
    if (field_name) {
        for (size_t i = 0; field_name[i] != '\0'; i++) {
            if (!cg_name_char((unsigned char) field_name[i])) {
                return NULL;
            }
        }
        for (size_t i = 0; i < function->struct_field_count; i++) {
            if (strcmp(function->struct_fields[i].name, field_name) == 0) {
                return function->struct_fields[i].type;
            }
        }
        return NULL;
    }
    if (!cg_name_start((unsigned char) lvalue[0])) {
        return NULL;
    }
    for (size_t i = 0; lvalue[i] != '\0'; i++) {
        if (!cg_name_char((unsigned char) lvalue[i])) {
            return NULL;
        }
    }
    return function_binding_c_type(function, lvalue);
}

static int require_pointer_lvalue(const char *lvalue,
                                  const FunctionOutput *function,
                                  const char *operator_name,
                                  size_t line_number, char *error,
                                  size_t error_size)
{
    const char *c_type = resolve_simple_lvalue_c_type(lvalue, function);

    if (!c_type) {
        cg_set_error(error, error_size,
                     "line %zu: %s requires a simple pointer lvalue",
                     line_number, operator_name);
        return -1;
    }
    if (!c_type_is_pointer(c_type)) {
        cg_set_error(error, error_size,
                     "line %zu: %s requires a pointer operand, not %s",
                     line_number, operator_name, c_type);
        return -1;
    }
    return 0;
}

static int validate_method_field_write(const FunctionOutput *function,
                                       const char *lhs, size_t line_number,
                                       char *error, size_t error_size)
{
    const char *field_name = self_field_name_from_lvalue(lhs);
    const StructKnownField *field;

    if (!field_name) {
        cg_set_error(error, error_size,
                     "line %zu: struct method assignment must target "
                     "self.field",
                     line_number);
        return -1;
    }
    for (size_t i = sizeof("self.") - 1; field_name[i] != '\0'; i++) {
        if (!cg_name_char((unsigned char) field_name[i])) {
            cg_set_error(error, error_size,
                         "line %zu: struct method assignment must target "
                         "self.field",
                         line_number);
            return -1;
        }
    }
    if (!function->self_mutable) {
        cg_set_error(error, error_size,
                     "line %zu: writing through self requires @mutable fn",
                     line_number);
        return -1;
    }
    field = find_struct_known_field(function->struct_known_fields,
                                    function->struct_known_field_count,
                                    field_name);
    if (!field) {
        cg_set_error(error, error_size,
                     "line %zu: unknown struct field: %s", line_number,
                     field_name);
        return -1;
    }
    if (!field->is_mutable) {
        cg_set_error(error, error_size,
                     "line %zu: struct field %s is const", line_number,
                     field_name);
        return -1;
    }
    return 0;
}

static int begin_struct_method(
    StructOutput *struct_output, Block *blocks, size_t block_count,
    char *fn_name, FieldType *fn_return_type, bool method_mutable,
    bool method_initializer, DocAttributes doc_attributes,
    Symbol **symbols, size_t *symbol_count, size_t *symbol_capacity,
    HeaderDeps **header_deps, size_t *header_deps_count,
    size_t *header_deps_capacity, FunctionOutput *function_output,
    size_t indent, size_t line_number, char *error, size_t error_size)
{
    char *fn_symbol;
    char *fn_dsl;
    char *fn_c_return_type = NULL;
    char *self_param_type;
    bool fn_emit = struct_output->emit;

    if (struct_output->param_count > 0) {
        cg_set_error(error, error_size,
                     "line %zu: fn is not allowed in parameterized struct",
                     line_number);
        return -1;
    }
    if (struct_output->field_expand || struct_output->field_pointer ||
        struct_output->pending_param_require) {
        cg_set_error(error, error_size,
                     "line %zu: field attributes must precede a field",
                     line_number);
        return -1;
    }
    if (fn_return_type->name) {
        fn_c_return_type = resolve_c_field_type(
            fn_return_type, *symbols, *symbol_count, struct_output->module,
            header_deps, header_deps_count, header_deps_capacity, line_number,
            error, error_size);
        if (!fn_c_return_type) {
            return -1;
        }
    }
    fn_symbol = malloc(strlen(struct_output->c_name) + strlen(fn_name) + 2);
    fn_dsl = malloc(strlen(struct_output->dsl_name) + strlen(fn_name) + 2);
    self_param_type = malloc(strlen(struct_output->c_name) + 20);
    if (!fn_symbol || !fn_dsl || !self_param_type) {
        free(fn_symbol);
        free(fn_dsl);
        free(fn_c_return_type);
        free(self_param_type);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    snprintf(fn_symbol, strlen(struct_output->c_name) + strlen(fn_name) + 2,
             "%s_%s", struct_output->c_name, fn_name);
    snprintf(fn_dsl, strlen(struct_output->dsl_name) + strlen(fn_name) + 2,
             "%s.%s", struct_output->dsl_name, fn_name);
    snprintf(self_param_type, strlen(struct_output->c_name) + 20,
             "%s%s_t *", method_mutable ? "" : "const ",
             struct_output->c_name);
    if (fn_emit &&
        cg_add_symbol_ex(symbols, symbol_count, symbol_capacity, fn_dsl,
                         fn_symbol, struct_output->header, NULL, false, false,
                         method_mutable, SYMBOL_KIND_FN,
                         copy_field_type_dsl(fn_return_type)) != 0) {
        free(fn_symbol);
        free(fn_dsl);
        free(fn_c_return_type);
        free(self_param_type);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    *function_output = (FunctionOutput) {
        .active = true,
        .indent = indent,
        .c_name = fn_symbol,
        .return_type = fn_c_return_type,
        .module = struct_output->module,
        .docs = doc_attributes,
        .emit = fn_emit,
        .is_method = true,
        .self_mutable = method_mutable,
        .struct_dsl_name = strdup(struct_output->dsl_name),
        .self_struct_tag = strdup(struct_output->c_name),
        .struct_known_fields = struct_output->known_fields,
        .struct_known_field_count = struct_output->known_field_count,
        .struct_fields = struct_output->fields,
        .struct_field_count = struct_output->field_count,
        .is_initializer = method_initializer,
        .params = calloc(1, sizeof(char *)),
        .param_types = calloc(1, sizeof(char *)),
        .param_variadic = calloc(1, sizeof(bool)),
        .param_requires = calloc(1, sizeof(ParamRequire)),
        .param_count = 1,
    };
    if (!function_output->struct_dsl_name || !function_output->self_struct_tag ||
        !function_output->params || !function_output->param_types ||
        !function_output->param_variadic || !function_output->param_requires) {
        cg_clear_function(function_output);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    function_output->params[0] = strdup("self");
    function_output->param_types[0] = self_param_type;
    function_output->param_variadic[0] = false;
    function_output->param_requires[0] = PARAM_REQUIRE_ANY;
    if (!function_output->params[0]) {
        cg_clear_function(function_output);
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    (void) blocks;
    (void) block_count;
    return 0;
}

static void mark_function_local_used(FunctionOutput *function,
                                     const char *name, size_t length)
{
    if (!function) return;
    for (size_t i = 0; i < function->local_count; i++) {
        if (strlen(function->locals[i].name) == length &&
            memcmp(function->locals[i].name, name, length) == 0) {
            function->locals[i].used = true;
        }
    }
}

static char *copy_trimmed_text(const char *text, size_t length)
{
    while (length > 0 && text[0] == ' ') {
        text++;
        length--;
    }
    while (length > 0 && text[length - 1] == ' ') {
        length--;
    }
    return cg_copy_text(text, length);
}

static bool find_top_level_null_fallback(const char *expression,
                                         size_t *operator_at)
{
    int depth = 0;
    bool in_string = false;
    bool escaped = false;

    for (size_t i = 0; expression[i] != '\0'; i++) {
        char ch = expression[i];

        if (in_string) {
            if (ch == '"' && !escaped) {
                in_string = false;
            }
            escaped = ch == '\\' && !escaped;
            if (ch != '\\') {
                escaped = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
            escaped = false;
            continue;
        }
        if (ch == '(') {
            depth++;
            continue;
        }
        if (ch == ')' && depth > 0) {
            depth--;
            continue;
        }
        if (depth == 0 && ch == '?' && expression[i + 1] == '?') {
            *operator_at = i;
            return true;
        }
    }
    return false;
}

static bool find_top_level_elvis(const char *expression, size_t *question_at,
                                 size_t *colon_at)
{
    int depth = 0;
    bool in_string = false;
    bool escaped = false;

    for (size_t i = 0; expression[i] != '\0'; i++) {
        char ch = expression[i];

        if (in_string) {
            if (ch == '"' && !escaped) {
                in_string = false;
            }
            escaped = ch == '\\' && !escaped;
            if (ch != '\\') {
                escaped = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
            escaped = false;
            continue;
        }
        if (ch == '(') {
            depth++;
            continue;
        }
        if (ch == ')' && depth > 0) {
            depth--;
            continue;
        }
        if (depth == 0 && ch == '?' && expression[i + 1] != '?') {
            size_t at = i + 1;

            while (expression[at] == ' ') {
                at++;
            }
            if (expression[at] == ':') {
                *question_at = i;
                *colon_at = at;
                return true;
            }
        }
    }
    return false;
}

static char *transform_function_expression(
    const char *expression, FunctionOutput *function, Symbol *symbols,
    size_t symbol_count, ModuleOutput *module, HeaderDeps **header_deps,
    size_t *header_deps_count, size_t *header_deps_capacity,
    size_t line_number, char *error, size_t error_size)
{
    char *out = NULL;
    size_t out_length = 0;
    size_t out_capacity = 0;
    size_t operator_at = 0;
    size_t colon_at = 0;

    (void) line_number;
    if (find_top_level_null_fallback(expression, &operator_at)) {
        char *left = copy_trimmed_text(expression, operator_at);
        char *right = copy_trimmed_text(expression + operator_at + 2,
                                       strlen(expression + operator_at + 2));
        char *c_left;
        char *c_right;

        if (!left || !right) {
            free(left);
            free(right);
            cg_set_error(error, error_size, "out of memory");
            return NULL;
        }
        if (require_pointer_lvalue(left, function, "??", line_number, error,
                                   error_size) != 0) {
            free(left);
            free(right);
            return NULL;
        }
        c_left = transform_function_expression(
            left, function, symbols, symbol_count, module, header_deps,
            header_deps_count, header_deps_capacity, line_number, error,
            error_size);
        c_right = c_left ? transform_function_expression(
            right, function, symbols, symbol_count, module, header_deps,
            header_deps_count, header_deps_capacity, line_number, error,
            error_size) : NULL;
        free(left);
        free(right);
        if (!c_left || !c_right) {
            free(c_left);
            free(c_right);
            return NULL;
        }
        out = malloc(strlen(c_left) * 2 + strlen(c_right) + 27);
        if (!out) {
            free(c_left);
            free(c_right);
            cg_set_error(error, error_size, "out of memory");
            return NULL;
        }
        snprintf(out, strlen(c_left) * 2 + strlen(c_right) + 27,
                 "((%s) != NULL ? (%s) : (%s))", c_left, c_left, c_right);
        free(c_left);
        free(c_right);
        return out;
    }
    if (find_top_level_elvis(expression, &operator_at, &colon_at)) {
        char *left = copy_trimmed_text(expression, operator_at);
        char *right = copy_trimmed_text(expression + colon_at + 1,
                                       strlen(expression + colon_at + 1));
        char *c_left;
        char *c_right;

        if (!left || !right) {
            free(left);
            free(right);
            cg_set_error(error, error_size, "out of memory");
            return NULL;
        }
        c_left = transform_function_expression(
            left, function, symbols, symbol_count, module, header_deps,
            header_deps_count, header_deps_capacity, line_number, error,
            error_size);
        c_right = c_left ? transform_function_expression(
            right, function, symbols, symbol_count, module, header_deps,
            header_deps_count, header_deps_capacity, line_number, error,
            error_size) : NULL;
        free(left);
        free(right);
        if (!c_left || !c_right) {
            free(c_left);
            free(c_right);
            return NULL;
        }
        out = malloc(strlen(c_left) * 2 + strlen(c_right) + 15);
        if (!out) {
            free(c_left);
            free(c_right);
            cg_set_error(error, error_size, "out of memory");
            return NULL;
        }
        snprintf(out, strlen(c_left) * 2 + strlen(c_right) + 15,
                 "((%s) ? (%s) : (%s))", c_left, c_left, c_right);
        free(c_left);
        free(c_right);
        return out;
    }
    for (size_t i = 0; expression[i] != '\0';) {
        if (expression[i] == '"') {
            size_t start = i++;
            bool escaped = false;

            while (expression[i] != '\0') {
                char ch = expression[i++];

                if (ch == '"' && !escaped) {
                    break;
                }
                escaped = ch == '\\' && !escaped;
                if (ch != '\\') {
                    escaped = false;
                }
            }
            if (!append_text(&out, &out_length, &out_capacity,
                             expression + start, i - start)) {
                free(out);
                cg_set_error(error, error_size, "out of memory");
                return NULL;
            }
            continue;
        }
        if (cg_name_start((unsigned char) expression[i])) {
            size_t start = i;
            size_t end;
            Symbol *symbol = NULL;
            char *dsl_name = NULL;
            size_t ctor_span = scan_paren_call_span(expression, start);

            if (ctor_span > 0) {
                char *fragment = cg_copy_text(expression + start, ctor_span);
                char *callee = NULL;
                char **args = NULL;
                size_t arg_count = 0;
                const StructConstructor *constructor;

                if (!fragment) {
                    free(out);
                    cg_set_error(error, error_size, "out of memory");
                    return NULL;
                }
                if (cg_parse_paren_call(fragment, &callee, &args, &arg_count) &&
                    (constructor = find_compile_constructor(callee)) != NULL) {
                    char *call = NULL;
                    size_t call_length = 0;
                    size_t call_capacity = 0;

                    free(fragment);
                    if (!append_text(&call, &call_length, &call_capacity,
                                     constructor->c_call_name,
                                     strlen(constructor->c_call_name)) ||
                        !append_text(&call, &call_length, &call_capacity, "(",
                                     1)) {
                        free(call);
                        free(callee);
                        cg_free_cstr_array(args, arg_count);
                        free(out);
                        cg_set_error(error, error_size, "out of memory");
                        return NULL;
                    }
                    for (size_t arg_i = 0; arg_i < arg_count; arg_i++) {
                        char *transformed = transform_function_expression(
                            args[arg_i], function, symbols, symbol_count,
                            module, header_deps, header_deps_count,
                            header_deps_capacity, line_number, error,
                            error_size);

                        if (!transformed) {
                            free(call);
                            free(callee);
                            cg_free_cstr_array(args, arg_count);
                            free(out);
                            return NULL;
                        }
                        if (!append_text(&call, &call_length, &call_capacity,
                                         arg_i ? ", " : "", arg_i ? 2 : 0) ||
                            !append_text(&call, &call_length, &call_capacity,
                                         transformed, strlen(transformed))) {
                            free(transformed);
                            free(call);
                            free(callee);
                            cg_free_cstr_array(args, arg_count);
                            free(out);
                            cg_set_error(error, error_size, "out of memory");
                            return NULL;
                        }
                        free(transformed);
                    }
                    free(callee);
                    cg_free_cstr_array(args, arg_count);
                    if (!append_text(&call, &call_length, &call_capacity, ")",
                                     1)) {
                        free(call);
                        free(out);
                        cg_set_error(error, error_size, "out of memory");
                        return NULL;
                    }
                    if (!append_text(&out, &out_length, &out_capacity, call,
                                     strlen(call))) {
                        free(call);
                        free(out);
                        cg_set_error(error, error_size, "out of memory");
                        return NULL;
                    }
                    free(call);
                    i = start + ctor_span;
                    continue;
                }
                free(callee);
                cg_free_cstr_array(args, arg_count);
                free(fragment);
            }

            if (function && function->is_method &&
                strncmp(expression + start, "self.", 5) == 0 &&
                cg_name_start((unsigned char) expression[start + 5])) {
                size_t span = scan_paren_call_span(expression, start);

                if (span > 0) {
                    char *callee = NULL;
                    char **args = NULL;
                    size_t arg_count = 0;
                    char *fragment = cg_copy_text(expression + start, span);
                    char *transformed;

                    if (!fragment) {
                        free(out);
                        cg_set_error(error, error_size, "out of memory");
                        return NULL;
                    }
                    if (parse_self_method_call(fragment, &callee, &args,
                                               &arg_count)) {
                        transformed = transform_self_method_call(
                            callee, args, arg_count, function, symbols,
                            symbol_count, module, header_deps, header_deps_count,
                            header_deps_capacity, line_number, error,
                            error_size);
                        free(fragment);
                        free(callee);
                        cg_free_cstr_array(args, arg_count);
                        if (!transformed) {
                            free(out);
                            return NULL;
                        }
                        if (!append_text(&out, &out_length, &out_capacity,
                                         transformed, strlen(transformed))) {
                            free(transformed);
                            free(out);
                            cg_set_error(error, error_size, "out of memory");
                            return NULL;
                        }
                        free(transformed);
                        i = start + span;
                        continue;
                    }
                    free(fragment);
                }
                {
                    size_t field_end = start + 5;

                    while (cg_name_char((unsigned char) expression[field_end])) {
                        field_end++;
                    }
                    if (!append_text(&out, &out_length, &out_capacity, "self->",
                                     6) ||
                        !append_text(&out, &out_length, &out_capacity,
                                     expression + start + 5,
                                     field_end - start - 5)) {
                        free(out);
                        cg_set_error(error, error_size, "out of memory");
                        return NULL;
                    }
                    i = field_end;
                    continue;
                }
            }
            i = start + 1;
            while (cg_name_char((unsigned char) expression[i]) ||
                   expression[i] == '.') {
                i++;
            }
            end = i;
            mark_function_local_used(function, expression + start,
                                     end - start);
            dsl_name = cg_copy_text(expression + start, end - start);
            if (!dsl_name) {
                free(out);
                cg_set_error(error, error_size, "out of memory");
                return NULL;
            }
            symbol = cg_find_symbol(symbols, symbol_count, dsl_name);
            free(dsl_name);
            if (symbol) {
                if (!symbol->is_internal &&
                    cg_module_require_include(module, symbol->header,
                                              header_deps, header_deps_count,
                                              header_deps_capacity) != 0) {
                    free(out);
                    cg_set_error(error, error_size, "out of memory");
                    return NULL;
                }
                if (!append_text(&out, &out_length, &out_capacity,
                                 symbol->c_name, strlen(symbol->c_name))) {
                    free(out);
                    cg_set_error(error, error_size, "out of memory");
                    return NULL;
                }
            } else if (!append_text(&out, &out_length, &out_capacity,
                                    expression + start, end - start)) {
                free(out);
                cg_set_error(error, error_size, "out of memory");
                return NULL;
            }
            continue;
        }
        if (!append_text(&out, &out_length, &out_capacity, expression + i, 1)) {
            free(out);
            cg_set_error(error, error_size, "out of memory");
            return NULL;
        }
        i++;
    }
    if (!out) {
        out = strdup("");
        if (!out) {
            cg_set_error(error, error_size, "out of memory");
        }
    }
    return out;
}

static StructTemplate *find_struct_template(StructTemplate *templates,
                                            size_t count, const char *dsl_name)
{
    for (size_t i = count; i > 0; i--) {
        if (strcmp(templates[i - 1].dsl_name, dsl_name) == 0) {
            return &templates[i - 1];
        }
    }
    return NULL;
}

static void free_struct_template(StructTemplate *template)
{
    free(template->dsl_name);
    free(template->c_name);
    free(template->header);
    cg_free_names(template->params, template->param_count);
    for (size_t i = 0; i < template->field_count; i++) {
        free(template->fields[i].name);
        free(template->fields[i].type);
        free(template->fields[i].doc);
    }
    free(template->fields);
    *template = (StructTemplate) {0};
}

static int save_struct_template(StructTemplate **templates, size_t *count,
                                size_t *capacity, const StructOutput *output)
{
    StructTemplate entry = {0};

    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 4;
        StructTemplate *grown = realloc(*templates, next * sizeof(*grown));

        if (!grown) {
            return -1;
        }
        *templates = grown;
        *capacity = next;
    }
    entry.dsl_name = strdup(output->dsl_name);
    entry.c_name = strdup(output->c_name);
    entry.header = strdup(output->header);
    entry.all_mutable = output->all_mutable;
    entry.param_count = output->param_count;
    if (entry.param_count) {
        entry.params = calloc(entry.param_count, sizeof(*entry.params));
        if (!entry.params) {
            free_struct_template(&entry);
            return -1;
        }
        for (size_t i = 0; i < entry.param_count; i++) {
            entry.params[i] = strdup(output->params[i]);
            if (!entry.params[i]) {
                free_struct_template(&entry);
                return -1;
            }
        }
    }
    entry.field_count = output->field_count;
    if (entry.field_count) {
        entry.fields = calloc(entry.field_count, sizeof(*entry.fields));
        if (!entry.fields) {
            free_struct_template(&entry);
            return -1;
        }
        for (size_t i = 0; i < entry.field_count; i++) {
            entry.fields[i].name = strdup(output->fields[i].name);
            entry.fields[i].type = strdup(output->fields[i].type);
            entry.fields[i].is_mutable = output->fields[i].is_mutable;
            entry.fields[i].doc = output->fields[i].doc
                                      ? strdup(output->fields[i].doc)
                                      : NULL;
            if (!entry.fields[i].name || !entry.fields[i].type) {
                free_struct_template(&entry);
                return -1;
            }
        }
    }
    if (!entry.dsl_name || !entry.c_name || !entry.header) {
        free_struct_template(&entry);
        return -1;
    }
    (*templates)[(*count)++] = entry;
    return 0;
}

static char *resolve_struct_type_argument(
    const char *argument, size_t line_number, Symbol *symbols,
    size_t symbol_count, ModuleOutput *module, HeaderDeps **header_deps,
    size_t *header_deps_count, size_t *header_deps_capacity, char *error,
    size_t error_size)
{
    Symbol *symbol = cg_find_symbol(symbols, symbol_count, argument);

    if (!symbol) {
        cg_set_error(error, error_size,
                     "line %zu: unknown type reference: %s", line_number,
                     argument);
        return NULL;
    }
    if (!symbol->is_internal &&
        cg_module_require_include(module, symbol->header, header_deps,
                                  header_deps_count,
                                  header_deps_capacity) != 0) {
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    return strdup(symbol->c_name);
}

static char *build_macro_call_expression(const char *c_name, char **args,
                                         size_t arg_count)
{
    size_t length = strlen(c_name) + 2;
    char *expr;

    for (size_t i = 0; i < arg_count; i++) {
        length += strlen(args[i]) + 2;
    }
    expr = malloc(length);
    if (!expr) {
        return NULL;
    }
    strcpy(expr, c_name);
    strcat(expr, "(");
    for (size_t i = 0; i < arg_count; i++) {
        if (i) {
            strcat(expr, ", ");
        }
        strcat(expr, args[i]);
    }
    strcat(expr, ")");
    return expr;
}

static int append_struct_macro_line(StructOutput *output, const char *expr)
{
    if (output->macro_line_count == output->macro_line_capacity) {
        size_t capacity = output->macro_line_capacity
                              ? output->macro_line_capacity * 2 : 4;
        StructMacroLine *grown = realloc(
            output->macro_lines, capacity * sizeof(*output->macro_lines));

        if (!grown) {
            return -1;
        }
        output->macro_lines = grown;
        output->macro_line_capacity = capacity;
    }
    output->macro_lines[output->macro_line_count].expr = strdup(expr);
    if (!output->macro_lines[output->macro_line_count].expr) {
        return -1;
    }
    output->macro_line_count++;
    return 0;
}

static int expand_struct_macro_fields(
    StructOutput *output, const StructTemplate *template, char **resolved_args,
    bool all_mutable, char *error, size_t error_size)
{
    for (size_t i = 0; i < template->field_count; i++) {
        const char *c_type = template->fields[i].type;
        char *field_name = strdup(template->fields[i].name);
        char *resolved_type = NULL;

        for (size_t j = 0; j < template->param_count; j++) {
            if (strcmp(template->fields[i].type, template->params[j]) == 0) {
                c_type = resolved_args[j];
                break;
            }
        }
        resolved_type = strdup(c_type);
        if (!field_name || !resolved_type) {
            free(field_name);
            free(resolved_type);
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        if (output->field_count == output->field_capacity) {
            size_t capacity = output->field_capacity ? output->field_capacity * 2
                                                     : 4;
            StructField *grown = realloc(
                output->fields, capacity * sizeof(*output->fields));

            if (!grown) {
                free(field_name);
                free(resolved_type);
                cg_set_error(error, error_size, "out of memory");
                return -1;
            }
            output->fields = grown;
            output->field_capacity = capacity;
        }
        output->fields[output->field_count++] = (StructField) {
            field_name,
            resolved_type,
            all_mutable || template->all_mutable || template->fields[i].is_mutable,
            template->fields[i].doc ? strdup(template->fields[i].doc) : NULL
        };
    }
    return 0;
}

static int finalize_struct(
    StructOutput *output, StructTemplate **templates, size_t *template_count,
    size_t *template_capacity, Symbol **symbols, size_t *symbol_count,
    size_t *symbol_capacity, Block *blocks, size_t block_count, char *error,
    size_t error_size)
{
    if (!output->active || !output->dsl_name || !output->header) {
        return 0;
    }
    if (output->param_count > 0) {
        if (save_struct_template(templates, template_count, template_capacity,
                                 output) != 0) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        if (output->emit) {
            char *dsl_name = strdup(output->dsl_name);

            if (!dsl_name) {
                cg_set_error(error, error_size, "out of memory");
                return -1;
            }
            if (cg_add_symbol_ex(symbols, symbol_count, symbol_capacity, dsl_name,
                                 output->c_name, output->header, NULL, false,
                                 false, false, SYMBOL_KIND_TYPE, NULL) != 0) {
                free(dsl_name);
                cg_set_error(error, error_size, "out of memory");
                return -1;
            }
        }
        return 0;
    }
    if (!output->emit) {
        return 0;
    }
    if (output->field_count == 0 && output->macro_line_count == 0) {
        cg_set_error(error, error_size, "struct requires at least one field");
        return -1;
    }
    {
        size_t typedef_length = strlen(output->c_name) + 3;
        char *typedef_name = malloc(typedef_length);
        char *dsl_name = strdup(output->dsl_name);

        if (!typedef_name || !dsl_name) {
            free(typedef_name);
            free(dsl_name);
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        snprintf(typedef_name, typedef_length, "%s_t", output->c_name);
        if (cg_add_symbol_ex(symbols, symbol_count, symbol_capacity, dsl_name,
                             typedef_name, output->header, NULL, false, false,
                             false, SYMBOL_KIND_TYPE, NULL) != 0 ||
            cg_add_primary_export_alias(blocks, block_count, symbols,
                                        symbol_count, symbol_capacity) != 0) {
            free(dsl_name);
            free(typedef_name);
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        free(typedef_name);
    }
    if (output->initializer_init_c_name && output->emit &&
        add_compile_constructor(output->dsl_name, output->c_name) != 0) {
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    return 0;
}

static size_t module_body_indent(const Block *blocks, size_t block_count,
                                 const IfFrame *frames, size_t if_depth)
{
    size_t parent = block_count ? blocks[block_count - 1].indent : 0;

    if (if_depth > 0 && frames[if_depth - 1].indent > parent) {
        return frames[if_depth - 1].indent;
    }
    return parent;
}

static bool if_chain_define_mode(const IfFrame *frames, size_t depth,
                                 size_t indent, bool attribute_define)
{
    if (attribute_define) {
        return true;
    }
    for (size_t i = depth; i > 0; i--) {
        if (indent > frames[i - 1].indent && frames[i - 1].define_mode) {
            return true;
        }
    }
    return false;
}

static int emit_pp_branch(ModuleOutput *module, const char *keyword,
                          const char *condition, char *error,
                          size_t error_size)
{
    char directive[1024];

    if (snprintf(directive, sizeof(directive), "%s %s", keyword,
                 condition) >= (int) sizeof(directive)) {
        cg_set_error(error, error_size, "preprocessor condition too long");
        return -1;
    }
    return cg_emit_preprocessor(module, directive, error, error_size);
}

static int push_if_frame(IfFrame **frames, size_t *depth, size_t *capacity,
                         size_t indent, bool define_mode, char *error,
                         size_t error_size)
{
    IfFrame *grown;

    if (*depth == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 4;

        grown = realloc(*frames, next * sizeof(**frames));
        if (!grown) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        *frames = grown;
        *capacity = next;
    }
    (*frames)[(*depth)++] = (IfFrame) {
        indent, true, define_mode
    };
    return 0;
}

static char *transform_pp_condition(
    const char *condition, Symbol *symbols, size_t symbol_count,
    ModuleOutput *module, HeaderDeps **header_deps, size_t *header_deps_count,
    size_t *header_deps_capacity, char *error, size_t error_size)
{
    static const char compiler_prefix[] = "c.compiler.";
    const char *at = condition;
    Symbol *symbol;

    while (*at == ' ') {
        at++;
    }
    if (strncmp(at, compiler_prefix, sizeof(compiler_prefix) - 1) == 0) {
        const char *suffix = at + sizeof(compiler_prefix) - 1;
        const char *end = suffix;

        while (*end && *end != ' ') {
            end++;
        }
        if (*end == '\0') {
            char *dsl_name = cg_copy_text(at, (size_t) (end - at));

            if (!dsl_name) {
                cg_set_error(error, error_size, "out of memory");
                return NULL;
            }
            symbol = cg_find_symbol(symbols, symbol_count, dsl_name);
            if (symbol) {
                char *result = strdup(symbol->c_name);

                free(dsl_name);
                if (!result) {
                    cg_set_error(error, error_size, "out of memory");
                    return NULL;
                }
                return result;
            }
            free(dsl_name);
            return cg_copy_text(suffix, (size_t) (end - suffix));
        }
    }
    return transform_function_expression(
        condition, NULL, symbols, symbol_count, module, header_deps,
        header_deps_count, header_deps_capacity, 0, error, error_size);
}

static int transform_inline_initializer(
    const char *expression, FunctionOutput *function, Symbol *symbols,
    size_t symbol_count, ModuleOutput *module, HeaderDeps **header_deps,
    size_t *header_deps_count, size_t *header_deps_capacity,
    size_t line_number, char **result, size_t *value_count,
    char *error, size_t error_size)
{
    static const char prefix[] = "c.initializer(";
    const char *at;
    const char *end;
    const char *part;
    size_t depth = 0;
    char quote = '\0';
    bool escaped = false;

    *result = NULL;
    *value_count = 0;
    if (strncmp(expression, prefix, sizeof(prefix) - 1) != 0) return 0;
    end = expression + strlen(expression);
    while (end > expression && end[-1] == ' ') end--;
    if (end <= expression + sizeof(prefix) - 1 || end[-1] != ')') {
        cg_set_error(error, error_size,
                     "line %zu: invalid c.initializer call", line_number);
        return -1;
    }
    at = expression + sizeof(prefix) - 1;
    part = at;
    for (; at < end; at++) {
        char ch = *at;

        if (quote) {
            if (ch == quote && !escaped) quote = '\0';
            escaped = ch == '\\' && !escaped;
            if (ch != '\\') escaped = false;
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
        } else if (ch == '(' || ch == '[' || ch == '{') {
            depth++;
        } else if (ch == ')' || ch == ']' || ch == '}') {
            if (ch == ')' && at == end - 1 && depth == 0) {
                /* The closing parenthesis of c.initializer itself. */
            } else if (depth == 0) {
                cg_set_error(error, error_size,
                             "line %zu: invalid c.initializer call",
                             line_number);
                free(*result);
                *result = NULL;
                return -1;
            } else {
                depth--;
            }
        }
        if ((ch == ',' && depth == 0) ||
            (ch == ')' && at == end - 1 && depth == 0)) {
            const char *part_end = at;
            char *argument;
            char *transformed;
            const char *parameter;

            while (part < part_end && *part == ' ') part++;
            while (part_end > part && part_end[-1] == ' ') part_end--;
            if (part == part_end) {
                cg_set_error(error, error_size,
                             "line %zu: c.initializer requires values",
                             line_number);
                free(*result);
                *result = NULL;
                return -1;
            }
            argument = cg_copy_text(part, (size_t) (part_end - part));
            if (!argument) {
                cg_set_error(error, error_size, "out of memory");
                free(*result);
                *result = NULL;
                return -1;
            }
            parameter = cg_param_c_type(
                (const char **) function->params, function->param_variadic,
                function->param_count, argument);
            if (parameter) {
                if (check_param_value_use(
                        (const char **) function->params,
                        function->param_requires, function->param_count,
                        argument, line_number, error, error_size) != 0) {
                    free(argument);
                    free(*result);
                    *result = NULL;
                    return -1;
                }
                transformed = strdup(parameter);
            } else {
                transformed = transform_function_expression(
                    argument, function, symbols, symbol_count, module,
                    header_deps, header_deps_count, header_deps_capacity,
                    line_number, error, error_size);
            }
            free(argument);
            if (!transformed) {
                free(*result);
                *result = NULL;
                return -1;
            }
            if (!append_initializer_value(result, transformed)) {
                free(transformed);
                free(*result);
                *result = NULL;
                cg_set_error(error, error_size, "out of memory");
                return -1;
            }
            free(transformed);
            (*value_count)++;
            part = at + 1;
        }
    }
    if (quote || depth != 0 || *value_count == 0) {
        cg_set_error(error, error_size,
                     "line %zu: invalid c.initializer call", line_number);
        free(*result);
        *result = NULL;
        return -1;
    }
    return 1;
}

static char *transform_function_callee(
    const char *callee, FunctionOutput *function, Symbol *symbols,
    size_t symbol_count, ModuleOutput *module, HeaderDeps **header_deps,
    size_t *header_deps_count, size_t *header_deps_capacity,
    size_t line_number, char *error, size_t error_size)
{
    size_t length = strlen(callee);
    char *call = malloc(length + 3);
    char *transformed;
    size_t transformed_length;

    if (!call) {
        cg_set_error(error, error_size, "out of memory");
        return NULL;
    }
    memcpy(call, callee, length);
    memcpy(call + length, "()", 3);
    transformed = transform_function_expression(
        call, function, symbols, symbol_count, module, header_deps,
        header_deps_count, header_deps_capacity, line_number, error,
        error_size);
    free(call);
    if (!transformed) {
        return NULL;
    }
    transformed_length = strlen(transformed);
    if (transformed_length < 2 ||
        strcmp(transformed + transformed_length - 2, "()") != 0) {
        return transformed;
    }
    transformed[transformed_length - 2] = '\0';
    return transformed;
}

static int append_return_call_args(
    FunctionOutput *function, char **raw_args, size_t raw_arg_count,
    Symbol *symbols, size_t symbol_count, ModuleOutput *module,
    HeaderDeps **header_deps, size_t *header_deps_count,
    size_t *header_deps_capacity, size_t line_number, char *error,
    size_t error_size)
{
    for (size_t i = 0; i < raw_arg_count; i++) {
        char *transformed = transform_function_expression(
            raw_args[i], function, symbols, symbol_count, module, header_deps,
            header_deps_count, header_deps_capacity, line_number, error,
            error_size);

        if (!transformed) {
            return -1;
        }
        if (add_function_arg(function, transformed, false, error, error_size) !=
            0) {
            free(transformed);
            return -1;
        }
    }
    return 0;
}

static int finalize_function_body(
    FunctionOutput *function_output, Symbol *symbols, size_t symbol_count,
    ModuleOutput *module, HeaderDeps **header_deps, size_t *header_deps_count,
    size_t *header_deps_capacity, size_t line_number, char *error,
    size_t error_size)
{
    return finalize_call_block(function_output, symbols, symbol_count, module,
                               header_deps, header_deps_count,
                               header_deps_capacity, line_number, error,
                               error_size);
}

static void reset_compiler_attributes(size_t *attribute_count,
                                       bool *attribute_noscope,
                                       bool *attribute_public,
                                       bool *attribute_private,
                                       bool *attribute_extern,
                                       bool *attribute_opaque,
                                       bool *attribute_internal,
                                       bool *attribute_define,
                                       bool *attribute_mutable,
                                       bool *attribute_expand,
                                       bool *attribute_pointer,
                                       bool *attribute_initializer)
{
    *attribute_count = 0;
    *attribute_noscope = false;
    *attribute_public = false;
    *attribute_private = false;
    *attribute_extern = false;
    *attribute_opaque = false;
    *attribute_internal = false;
    *attribute_define = false;
    *attribute_mutable = false;
    *attribute_expand = false;
    *attribute_pointer = false;
    *attribute_initializer = false;
}

int cgem_compile(FILE *input, const char *include_path,
                 const char *source_path, const char *format_style,
                 const char *compiler, bool clean_output,
                 char *warning, size_t warning_size,
                 char *error, size_t error_size,
                 DiagnosticList *diagnostics_out)
{
    Block *blocks = NULL;
    size_t block_count = 0;
    size_t block_capacity = 0;
    ModuleOutput module = {0};
    EnumOutput enum_output = {0};
    StructOutput struct_output = {0};
    StructTemplate *struct_templates = NULL;
    size_t struct_template_count = 0;
    size_t struct_template_capacity = 0;
    FunctionOutput function_output = {0};
    Symbol *symbols = NULL;
    size_t symbol_count = 0;
    size_t symbol_capacity = 0;
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    size_t line_number = 0;
    bool reuse_line = false;
    size_t attribute_count = 0;
    size_t attribute_indent = 0;
    bool attribute_noscope = false;
    bool attribute_public = false;
    bool attribute_private = false;
    bool attribute_extern = false;
    bool attribute_opaque = false;
    bool attribute_internal = false;
    bool attribute_define = false;
    bool attribute_mutable = false;
    bool attribute_expand = false;
    bool attribute_pointer = false;
    bool attribute_initializer = false;
    DocAttributes doc_attributes = {0};
    IncludeAttributes include_attributes = {0};
    BlockAttributeKind block_attribute = BLOCK_ATTR_NONE;
    size_t block_attribute_indent = 0;
    size_t block_attribute_values = 0;
    HeaderDeps *header_deps = NULL;
    size_t header_deps_count = 0;
    size_t header_deps_capacity = 0;
    IfFrame *if_frames = NULL;
    size_t if_depth = 0;
    size_t if_capacity = 0;
    DiagnosticList local_diagnostics = {0};
    DiagnosticList *diagnostics = diagnostics_out ? diagnostics_out
                                                  : &local_diagnostics;
    char **cleaned_packages = NULL;
    size_t cleaned_package_count = 0;
    size_t cleaned_package_capacity = 0;
    int result = -1;

    cg_diagnostic_clear(diagnostics);
    cg_diagnostic_set_active(diagnostics);
    reset_compile_constructors();
    if (error && error_size > 0) {
        error[0] = '\0';
    }
    if (warning && warning_size > 0) {
        warning[0] = '\0';
    }
    if (cg_add_builtin_c_types(&symbols, &symbol_count, &symbol_capacity) != 0) {
        cg_set_error(error, error_size, "out of memory");
        goto done;
    }
    if (cg_add_compiler_macros(&symbols, &symbol_count, &symbol_capacity,
                               compiler, error, error_size) != 0) {
        goto done;
    }
    if (cg_lint(input, diagnostics) != 0) {
        cg_set_error(error, error_size, "failed to lint input");
        goto done;
    }
    while (reuse_line ||
           (line_length = cg_read_input_line(input, &line, &line_capacity)) != -1) {
        size_t indent = 0;
        char *text;
        char *name = NULL;
        const char *attribute_name;
        size_t attribute_name_length;
        BlockKind kind;

        if (reuse_line) {
            reuse_line = false;
        } else {
            line_number++;
        }
        while (indent < (size_t) line_length && line[indent] == ' ') indent++;
        while (line_length > 0 &&
               (line[line_length - 1] == '\n' ||
                line[line_length - 1] == '\r')) {
            line[--line_length] = '\0';
        }
        if (line[indent] == '\0') {
            if (module.relative_header) {
                module.pending_blank_lines++;
            }
            continue;
        }
        if (indent % 4 != 0 || strchr(line, '\t')) {
            cg_set_error(error, error_size,
                      "line %zu: indentation must use groups of 4 spaces",
                      line_number);
            goto done;
        }
        if (block_attribute != BLOCK_ATTR_NONE) {
            char *block_value = NULL;

            if (indent == block_attribute_indent + 4 &&
                cg_parse_block_attribute_string(line + indent, &block_value)) {
                if (block_attribute == BLOCK_ATTR_DOC) {
                    DocEntry entry = {DOC_ENTRY_TEXT, block_value};

                    if (cg_add_doc_entry(&doc_attributes, &entry) != 0) {
                        cg_free_doc_entry(&entry);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                } else if (cg_add_include_attribute(&include_attributes,
                                                    block_value) != 0) {
                    cg_set_error(error, error_size, "out of memory");
                    goto done;
                }
                block_attribute_values++;
                continue;
            }
            if (block_attribute_values == 0) {
                cg_set_error(error, error_size,
                          "line %zu: @%s: requires at least one string",
                          line_number,
                          block_attribute == BLOCK_ATTR_DOC ? "doc"
                                                            : "include");
                goto done;
            }
            block_attribute = BLOCK_ATTR_NONE;
            block_attribute_values = 0;
        }
        if (enum_output.active && indent <= enum_output.indent) {
            cg_close_enum(&enum_output);
        }
        if (enum_output.active) {
            char *member_name = NULL;
            char *member_value = NULL;
            char *explicit_value = NULL;
            const char *c_type;
            char *c_symbol = NULL;
            char *member_dsl = NULL;
            long long numeric_value;

            if (indent != enum_output.indent + 4) {
                cg_set_error(error, error_size,
                          "line %zu: expected case inside enum",
                          line_number);
                goto done;
            }
            if (!cg_parse_case(line + indent, &member_name, &explicit_value)) {
                cg_set_error(error, error_size,
                          "line %zu: expected case inside enum",
                          line_number);
                goto done;
            }
            c_type = enum_output.type_name;
            if (explicit_value) {
                member_value = strdup(explicit_value);
                if (!member_value) {
                    free(member_name);
                    free(explicit_value);
                    cg_set_error(error, error_size, "out of memory");
                    goto done;
                }
            } else {
                char auto_value[32];

                snprintf(auto_value, sizeof(auto_value), "%lld",
                         enum_output.next_implicit_value);
                member_value = strdup(auto_value);
                if (!member_value) {
                    free(member_name);
                    free(explicit_value);
                    cg_set_error(error, error_size, "out of memory");
                    goto done;
                }
            }
            {
                char *transformed = transform_function_expression(
                    member_value, NULL, symbols, symbol_count, &module,
                    &header_deps, &header_deps_count, &header_deps_capacity,
                    line_number, error, error_size);

                if (!transformed) {
                    free(member_name);
                    free(explicit_value);
                    free(member_value);
                    goto done;
                }
                free(member_value);
                member_value = transformed;
            }
            if (explicit_value) {
                if (!eval_const_integer_expr(member_value, symbols, symbol_count,
                                             &numeric_value)) {
                    free(member_name);
                    free(explicit_value);
                    free(member_value);
                    cg_set_error(error, error_size,
                                 "line %zu: enum case value must be a constant "
                                 "integer expression",
                                 line_number);
                    goto done;
                }
                enum_output.next_implicit_value = numeric_value + 1;
            } else {
                enum_output.next_implicit_value++;
            }
            c_symbol = malloc(strlen(enum_output.c_name) +
                              strlen(member_name) + 2);
            if (!c_symbol) {
                free(member_name);
                free(explicit_value);
                free(member_value);
                cg_set_error(error, error_size, "out of memory");
                goto done;
            }
            snprintf(c_symbol, strlen(enum_output.c_name) +
                              strlen(member_name) + 2,
                     "%s_%s", enum_output.c_name, member_name);
            if (enum_output.emit) {
                if (cg_emit_let(enum_output.module, enum_output.use_source,
                                enum_output.use_define, true, false, false,
                                false, &doc_attributes, c_type, c_symbol,
                                member_value, error, error_size) != 0) {
                    free(member_name);
                    free(explicit_value);
                    free(member_value);
                    free(c_symbol);
                    goto done;
                }
            }
            member_dsl = malloc(strlen(enum_output.dsl_name) +
                                strlen(member_name) + 2);
            if (!member_dsl) {
                free(member_name);
                free(explicit_value);
                free(member_value);
                free(c_symbol);
                cg_set_error(error, error_size, "out of memory");
                goto done;
            }
            snprintf(member_dsl, strlen(enum_output.dsl_name) +
                                strlen(member_name) + 2,
                     "%s.%s", enum_output.dsl_name, member_name);
            if (cg_add_symbol_ex(&symbols, &symbol_count, &symbol_capacity,
                                 member_dsl, c_symbol, module.relative_header,
                                 member_value, enum_output.use_define,
                                 !enum_output.emit, false, SYMBOL_KIND_VALUE,
                                 enum_output.dsl_name ?
                                     strdup(enum_output.dsl_name) : NULL) !=
                0) {
                free(member_name);
                free(explicit_value);
                free(member_value);
                free(c_symbol);
                cg_set_error(error, error_size, "out of memory");
                goto done;
            }
            free(member_name);
            free(explicit_value);
            free(member_value);
            free(c_symbol);
            cg_clear_doc_attributes(&doc_attributes);
            continue;
        }
        if (function_output.active && indent <= function_output.indent) {
            if (finalize_function_body(
                    &function_output, symbols, symbol_count, &module,
                    &header_deps, &header_deps_count, &header_deps_capacity,
                    line_number, error, error_size) != 0) {
                goto done;
            }
            if (cg_close_function(
                    &function_output,
                    struct_output.active && function_output.is_method
                        ? &struct_output
                        : NULL,
                    error, error_size) != 0) {
                goto done;
            }
        }
        if (function_output.active) {
            size_t function_body_indent = function_output.indent + 4;
            char *let_name = NULL;
            FieldType let_type = {0};
            char *let_value = NULL;
            char *let_c_type = NULL;
            char *let_c_value = NULL;
            char *return_expr = NULL;
            FieldType cast_type = {0};
            char *cast_c_type = NULL;

            if (indent == function_body_indent &&
                cg_parse_require_attribute(line + indent,
                                           &function_output.pending_param_require_kind)) {
                if (function_output.has_return || function_output.local_count > 0 ||
                    function_output.local_mutable || function_output.local_used) {
                    cg_set_error(error, error_size,
                              "line %zu: params must precede the function body",
                              line_number);
                    goto done;
                }
                if (function_output.pending_param_require) {
                    cg_set_error(error, error_size,
                              "line %zu: @require specified more than once",
                              line_number);
                    goto done;
                }
                function_output.pending_param_require = true;
                continue;
            }
            if (indent == function_body_indent &&
                strcmp(line + indent, "@mutable") == 0) {
                if (function_output.has_return || function_output.local_count > 0 ||
                    function_output.statement_count > 0) {
                    if (function_output.local_mutable) {
                        cg_set_error(error, error_size,
                                  "line %zu: @mutable specified more than once",
                                  line_number);
                        goto done;
                    }
                    function_output.local_mutable = true;
                    continue;
                }
                if (function_output.param_mutable_pending) {
                    cg_set_error(error, error_size,
                              "line %zu: @mutable specified more than once",
                              line_number);
                    goto done;
                }
                function_output.param_mutable_pending = true;
                continue;
            }
            if (indent == function_body_indent &&
                strcmp(line + indent, "@pointer") == 0) {
                if (function_output.has_return || function_output.local_count > 0 ||
                    function_output.statement_count > 0) {
                    if (function_output.local_pointer_pending) {
                        cg_set_error(error, error_size,
                                  "line %zu: @pointer specified more than once",
                                  line_number);
                        goto done;
                    }
                    function_output.local_pointer_pending = true;
                    continue;
                }
                if (function_output.param_pointer_pending) {
                    cg_set_error(error, error_size,
                              "line %zu: @pointer specified more than once",
                              line_number);
                    goto done;
                }
                function_output.param_pointer_pending = true;
                continue;
            }
            if (indent == function_body_indent) {
                char *call_block_method = NULL;

                if (cg_parse_self_method_call_block_opener(
                        line + indent, &call_block_method)) {
                    if (function_output.has_return) {
                        free(call_block_method);
                        cg_set_error(error, error_size,
                                  "line %zu: call block must precede return",
                                  line_number);
                        goto done;
                    }
                    if (function_output.call_block_method) {
                        if (finalize_call_block(
                                &function_output, symbols, symbol_count, &module,
                                &header_deps, &header_deps_count, &header_deps_capacity,
                                line_number, error, error_size) != 0) {
                            free(call_block_method);
                            goto done;
                        }
                    } else if (function_output.local_count > 0 ||
                               function_output.local_mutable ||
                               function_output.local_used ||
                               function_output.local_pointer_pending) {
                        free(call_block_method);
                        cg_set_error(error, error_size,
                                  "line %zu: call block must precede local "
                                  "variables",
                                  line_number);
                        goto done;
                    }
                    function_output.call_block_method = call_block_method;
                    function_output.call_block_param_start =
                        function_output.param_count;
                    continue;
                }
            }
            if (function_output.call_block_method &&
                indent == function_body_indent + 4 &&
                strncmp(line + indent, "param ", 6) == 0) {
                if (process_function_param_line(
                        &function_output, line + indent, symbols, symbol_count,
                        &module, &header_deps, &header_deps_count,
                        &header_deps_capacity, line_number, error,
                        error_size) != 0) {
                    goto done;
                }
                continue;
            }
            if (function_output.call_block_method &&
                indent == function_body_indent + 4) {
                cg_set_error(error, error_size,
                          "line %zu: expected param inside call block",
                          line_number);
                goto done;
            }
            if (function_output.call_block_method &&
                indent == function_body_indent) {
                if (finalize_call_block(
                        &function_output, symbols, symbol_count, &module,
                        &header_deps, &header_deps_count, &header_deps_capacity,
                        line_number, error, error_size) != 0) {
                    goto done;
                }
            }
            if (indent == function_body_indent &&
                strncmp(line + indent, "param ", 6) == 0) {
                if (process_function_param_line(
                        &function_output, line + indent, symbols, symbol_count,
                        &module, &header_deps, &header_deps_count,
                        &header_deps_capacity, line_number, error,
                        error_size) != 0) {
                    goto done;
                }
                continue;
            }
            if (indent == function_body_indent &&
                function_output.pending_param_require) {
                cg_set_error(error, error_size,
                          "line %zu: @require must precede param",
                          line_number);
                goto done;
            }
            if (indent != function_body_indent) {
                cg_set_error(error, error_size,
                          "line %zu: expected let or return inside fn",
                          line_number);
                goto done;
            }
            if (strcmp(line + indent, "@used") == 0) {
                if (function_output.has_return) {
                    cg_set_error(error, error_size,
                              "line %zu: statement after return",
                              line_number);
                    goto done;
                }
                if (function_output.local_used) {
                    cg_set_error(error, error_size,
                              "line %zu: @used specified more than once",
                              line_number);
                    goto done;
                }
                function_output.local_used = true;
                continue;
            }
            if (cg_parse_let(line + indent, &let_name, &let_type,
                             &let_value)) {
                if (!let_type.name || !let_value) {
                    free(let_name);
                    cg_free_field_type(&let_type);
                    free(let_value);
                    cg_set_error(error, error_size,
                                 "line %zu: local let requires a type and value",
                                 line_number);
                    goto done;
                }
                if (function_output.param_mutable_pending) {
                    function_output.local_mutable = true;
                    function_output.param_mutable_pending = false;
                }
                if (function_output.param_pointer_pending) {
                    function_output.local_pointer_pending = true;
                    function_output.param_pointer_pending = false;
                }
                if (function_output.has_return) {
                    free(let_name);
                    cg_free_field_type(&let_type);
                    free(let_value);
                    cg_set_error(error, error_size,
                              "line %zu: statement after return",
                              line_number);
                    goto done;
                }
                if (function_output.local_pointer_pending) {
                    let_type.is_ptr = true;
                }
                let_c_type = resolve_c_field_type(
                    &let_type, symbols, symbol_count, &module, &header_deps,
                    &header_deps_count, &header_deps_capacity, line_number,
                    error, error_size);
                cg_free_field_type(&let_type);
                if (!let_c_type) {
                    free(let_name);
                    free(let_value);
                    goto done;
                }
                let_c_value = transform_function_expression(
                    let_value, &function_output, symbols, symbol_count,
                    &module, &header_deps, &header_deps_count,
                    &header_deps_capacity, line_number, error,
                    error_size);
                free(let_value);
                if (!let_c_value) {
                    free(let_name);
                    free(let_c_type);
                    goto done;
                }
                if (add_function_local(&function_output, let_name, let_c_type,
                                       let_c_value,
                                       function_output.local_mutable,
                                       function_output.local_used,
                                       false,
                                       line_number, error, error_size) != 0) {
                    free(let_name);
                    free(let_c_type);
                    free(let_c_value);
                    goto done;
                }
                function_output.local_mutable = false;
                function_output.local_used = false;
                function_output.local_pointer_pending = false;
                continue;
            }
            if (function_output.local_mutable || function_output.local_used ||
                function_output.local_pointer_pending) {
                cg_set_error(error, error_size,
                          "line %zu: local attributes must precede let",
                          line_number);
                goto done;
            }
            if (function_output.is_method) {
                char *assign_lhs = NULL;
                char *assign_rhs = NULL;
                char *c_lhs = NULL;
                char *c_rhs = NULL;
                char *statement = NULL;
                bool assign_lhs_is_self_field = false;
                MethodAssignOp assign_op = METHOD_ASSIGN_NORMAL;
                char *inline_param_name = NULL;
                FieldType inline_param_type = {0};
                bool inline_pointer = false;
                bool inline_mutable = false;

                if (parse_inline_param_optional_write(
                        line + indent, &inline_pointer, &inline_mutable,
                        &inline_param_name, &inline_param_type, &assign_rhs)) {
                    if (function_output.has_return) {
                        free(assign_rhs);
                        cg_free_field_type(&inline_param_type);
                        cg_set_error(error, error_size,
                                  "line %zu: statement after return",
                                  line_number);
                        goto done;
                    }
                    if (process_inline_function_param(
                            &function_output, inline_param_name,
                            &inline_param_type, inline_pointer, inline_mutable,
                            symbols, symbol_count, &module, &header_deps,
                            &header_deps_count, &header_deps_capacity,
                            line_number, error, error_size) != 0) {
                        free(assign_rhs);
                        cg_free_field_type(&inline_param_type);
                        goto done;
                    }
                    cg_free_field_type(&inline_param_type);
                    inline_param_name =
                        function_output.params[function_output.param_count - 1];
                    c_lhs = transform_function_expression(
                        inline_param_name, &function_output, symbols,
                        symbol_count, &module, &header_deps, &header_deps_count,
                        &header_deps_capacity, line_number, error, error_size);
                    if (!c_lhs) {
                        free(assign_rhs);
                        goto done;
                    }
                    c_rhs = transform_function_expression(
                        assign_rhs, &function_output, symbols, symbol_count,
                        &module, &header_deps, &header_deps_count,
                        &header_deps_capacity, line_number, error, error_size);
                    free(assign_rhs);
                    if (!c_rhs) {
                        free(c_lhs);
                        goto done;
                    }
                    statement = malloc(strlen(c_lhs) * 2 + strlen(c_rhs) + 12);
                    if (!statement) {
                        free(c_lhs);
                        free(c_rhs);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    snprintf(statement, strlen(c_lhs) * 2 + strlen(c_rhs) + 12,
                             "if (%s) *%s = %s", c_lhs, c_lhs, c_rhs);
                    free(c_lhs);
                    free(c_rhs);
                    if (add_function_statement(&function_output, statement,
                                                 error, error_size) != 0) {
                        goto done;
                    }
                    continue;
                }

                if (parse_method_assignment(line + indent, &assign_lhs,
                                            &assign_rhs, &assign_op)) {
                    if (function_output.has_return) {
                        free(assign_lhs);
                        free(assign_rhs);
                        cg_set_error(error, error_size,
                                  "line %zu: statement after return",
                                  line_number);
                        goto done;
                    }
                    if ((assign_op == METHOD_ASSIGN_NORMAL ||
                         assign_op == METHOD_ASSIGN_OPTIONAL_READ) &&
                        rhs_is_inline_param(
                            assign_rhs,
                            assign_op == METHOD_ASSIGN_OPTIONAL_READ,
                            &inline_param_name, &inline_param_type,
                            &inline_pointer, &inline_mutable)) {
                        if (process_inline_function_param(
                                &function_output, inline_param_name,
                                &inline_param_type, inline_pointer,
                                inline_mutable, symbols, symbol_count, &module,
                                &header_deps, &header_deps_count,
                                &header_deps_capacity, line_number, error,
                                error_size) != 0) {
                            free(assign_lhs);
                            free(assign_rhs);
                            cg_free_field_type(&inline_param_type);
                            goto done;
                        }
                        cg_free_field_type(&inline_param_type);
                        free(assign_rhs);
                        assign_rhs = strdup(
                            function_output
                                .params[function_output.param_count - 1]);
                        if (!assign_rhs) {
                            free(assign_lhs);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                    }
                    assign_lhs_is_self_field =
                        self_field_name_from_lvalue(assign_lhs) != NULL;
                    if (assign_op == METHOD_ASSIGN_NULL_COALESCE &&
                        require_pointer_lvalue(assign_lhs, &function_output,
                                               "??" "=", line_number, error,
                                               error_size) != 0) {
                        free(assign_lhs);
                        free(assign_rhs);
                        goto done;
                    }
                    if (assign_op == METHOD_ASSIGN_NORMAL ||
                        assign_op == METHOD_ASSIGN_OPTIONAL_READ ||
                        (assign_op == METHOD_ASSIGN_NULL_COALESCE &&
                         assign_lhs_is_self_field)) {
                        if (validate_method_field_write(&function_output,
                                                        assign_lhs, line_number,
                                                        error, error_size) != 0) {
                            free(assign_lhs);
                            free(assign_rhs);
                            goto done;
                        }
                        c_lhs = transform_method_lvalue(assign_lhs, error,
                                                        error_size);
                    } else {
                        c_lhs = transform_function_expression(
                            assign_lhs, &function_output, symbols, symbol_count,
                            &module, &header_deps, &header_deps_count,
                            &header_deps_capacity, line_number, error,
                            error_size);
                    }
                    free(assign_lhs);
                    if (!c_lhs) {
                        free(assign_rhs);
                        goto done;
                    }
                    c_rhs = transform_function_expression(
                        assign_rhs, &function_output, symbols, symbol_count,
                        &module, &header_deps, &header_deps_count,
                        &header_deps_capacity, line_number, error,
                        error_size);
                    free(assign_rhs);
                    if (!c_rhs) {
                        free(c_lhs);
                        goto done;
                    }
                    if (assign_op == METHOD_ASSIGN_OPTIONAL_READ) {
                        statement = malloc(strlen(c_lhs) + strlen(c_rhs) * 2 + 12);
                        if (!statement) {
                            free(c_lhs);
                            free(c_rhs);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        snprintf(statement, strlen(c_lhs) + strlen(c_rhs) * 2 + 12,
                                 "if (%s) %s = *%s", c_rhs, c_lhs, c_rhs);
                    } else if (assign_op == METHOD_ASSIGN_OPTIONAL_WRITE) {
                        statement = malloc(strlen(c_lhs) * 2 + strlen(c_rhs) + 12);
                        if (!statement) {
                            free(c_lhs);
                            free(c_rhs);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        snprintf(statement, strlen(c_lhs) * 2 + strlen(c_rhs) + 12,
                                 "if (%s) *%s = %s", c_lhs, c_lhs, c_rhs);
                    } else if (assign_op == METHOD_ASSIGN_NULL_COALESCE) {
                        statement = malloc(strlen(c_lhs) * 2 + strlen(c_rhs) + 22);
                        if (!statement) {
                            free(c_lhs);
                            free(c_rhs);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        snprintf(statement, strlen(c_lhs) * 2 + strlen(c_rhs) + 22,
                                 "if (%s == NULL) %s = %s", c_lhs, c_lhs,
                                 c_rhs);
                    } else {
                        statement = malloc(strlen(c_lhs) + strlen(c_rhs) + 4);
                        if (!statement) {
                            free(c_lhs);
                            free(c_rhs);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        snprintf(statement, strlen(c_lhs) + strlen(c_rhs) + 4,
                                 "%s = %s", c_lhs, c_rhs);
                    }
                    free(c_lhs);
                    free(c_rhs);
                    if (add_function_statement(&function_output, statement,
                                                 error, error_size) != 0) {
                        goto done;
                    }
                    continue;
                }
                {
                    char *call_callee = NULL;
                    char **call_args = NULL;
                    size_t call_arg_count = 0;

                    if (parse_self_method_call(line + indent, &call_callee,
                                               &call_args, &call_arg_count)) {
                        if (function_output.has_return) {
                            cg_free_cstr_array(call_args, call_arg_count);
                            free(call_callee);
                            cg_set_error(error, error_size,
                                      "line %zu: statement after return",
                                      line_number);
                            goto done;
                        }
                        statement = transform_self_method_call(
                            call_callee, call_args, call_arg_count,
                            &function_output, symbols, symbol_count, &module,
                            &header_deps, &header_deps_count,
                            &header_deps_capacity, line_number, error,
                            error_size);
                        free(call_callee);
                        cg_free_cstr_array(call_args, call_arg_count);
                        if (!statement) {
                            goto done;
                        }
                        if (add_function_statement(&function_output, statement,
                                                     error, error_size) != 0) {
                            goto done;
                        }
                        continue;
                    }
                }
            }
            if (!cg_parse_return(line + indent, &return_expr, &cast_type)) {
                cg_set_error(error, error_size,
                          function_output.is_method
                              ? "line %zu: expected let, return, "
                                "self.field = expr, or self.method(...) inside fn"
                              : "line %zu: expected let or return inside fn",
                          line_number);
                goto done;
            }
            if (function_output.has_return) {
                free(return_expr);
                cg_free_field_type(&cast_type);
                cg_set_error(error, error_size,
                          "line %zu: function allows only one return",
                          line_number);
                goto done;
            }
            if (cast_type.name) {
                cast_c_type = resolve_c_field_type(
                    &cast_type, symbols, symbol_count, &module, &header_deps,
                    &header_deps_count, &header_deps_capacity, line_number,
                    error, error_size);
                if (!cast_c_type) {
                    free(return_expr);
                    cg_free_field_type(&cast_type);
                    goto done;
                }
                if (!function_output.return_type) {
                    function_output.return_type = strdup(cast_c_type);
                    if (!function_output.return_type) {
                        free(return_expr);
                        free(cast_c_type);
                        cg_free_field_type(&cast_type);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                }
            }
            if (!function_output.return_type && function_output.is_method &&
                return_expr) {
                function_output.return_type = infer_method_return_type(
                    return_expr, function_output.struct_dsl_name, symbols,
                    symbol_count, &module, &header_deps, &header_deps_count,
                    &header_deps_capacity, line_number, error, error_size);
                if (!function_output.return_type) {
                    free(return_expr);
                    cg_free_field_type(&cast_type);
                    goto done;
                }
            }
            if (return_expr) {
                char *transformed_return = NULL;
                char *call_callee = NULL;
                char **call_args = NULL;
                size_t call_arg_count = 0;
                size_t expr_length = strlen(return_expr);
                int initializer_call;

                while (expr_length > 0 && return_expr[expr_length - 1] == ' ') {
                    expr_length--;
                }
                if (expr_length > 0 && return_expr[expr_length - 1] == ':') {
                    free(return_expr);
                    free(cast_c_type);
                    cg_free_field_type(&cast_type);
                    cg_set_error(error, error_size,
                                 "line %zu: use inline call syntax callee(args)",
                                 line_number);
                    goto done;
                }
                initializer_call = transform_inline_initializer(
                    return_expr, &function_output, symbols, symbol_count,
                    &module, &header_deps, &header_deps_count,
                    &header_deps_capacity, line_number, &transformed_return,
                    &function_output.initializer_value_count, error,
                    error_size);
                if (initializer_call < 0) {
                    free(return_expr);
                    free(cast_c_type);
                    cg_free_field_type(&cast_type);
                    goto done;
                }
                if (initializer_call > 0) {
                    free(return_expr);
                    return_expr = transformed_return;
                    function_output.return_is_initializer = true;
                    function_output.return_is_call = false;
                    function_output.return_expr = return_expr;
                    function_output.return_cast_type = cast_c_type;
                    function_output.has_return = true;
                    cg_free_field_type(&cast_type);
                    continue;
                }
                if (cg_parse_paren_call(return_expr, &call_callee, &call_args,
                                     &call_arg_count)) {
                    transformed_return = transform_function_callee(
                        call_callee, &function_output, symbols, symbol_count,
                        &module, &header_deps, &header_deps_count,
                        &header_deps_capacity, line_number, error,
                        error_size);
                    free(call_callee);
                    if (!transformed_return) {
                        free(return_expr);
                        free(cast_c_type);
                        cg_free_field_type(&cast_type);
                        for (size_t i = 0; i < call_arg_count; i++) {
                            free(call_args[i]);
                        }
                        free(call_args);
                        goto done;
                    }
                    if (append_return_call_args(
                            &function_output, call_args, call_arg_count,
                            symbols, symbol_count, &module, &header_deps,
                            &header_deps_count, &header_deps_capacity,
                            line_number, error, error_size) != 0) {
                        free(transformed_return);
                        free(return_expr);
                        free(cast_c_type);
                        cg_free_field_type(&cast_type);
                        for (size_t i = 0; i < call_arg_count; i++) {
                            free(call_args[i]);
                        }
                        free(call_args);
                        goto done;
                    }
                    for (size_t i = 0; i < call_arg_count; i++) {
                        free(call_args[i]);
                    }
                    free(call_args);
                    free(return_expr);
                    return_expr = transformed_return;
                    function_output.return_is_call = true;
                } else {
                    transformed_return = transform_function_expression(
                        return_expr, &function_output, symbols, symbol_count,
                        &module, &header_deps, &header_deps_count,
                        &header_deps_capacity, line_number, error,
                        error_size);
                    free(return_expr);
                    if (!transformed_return) {
                        free(cast_c_type);
                        cg_free_field_type(&cast_type);
                        goto done;
                    }
                    return_expr = transformed_return;
                    function_output.return_is_call = false;
                }
            }
            function_output.return_expr = return_expr;
            function_output.return_cast_type = cast_c_type;
            function_output.has_return = true;
            cg_free_field_type(&cast_type);
            continue;
        }

        if (struct_output.active && indent <= struct_output.indent) {
            if (attribute_mutable) {
                cg_set_error(error, error_size,
                          "line %zu: @mutable must precede a field or struct fn",
                          line_number);
                goto done;
            }
            if (struct_output.field_expand || struct_output.field_pointer) {
                cg_set_error(error, error_size,
                          "line %zu: field attributes must precede a field",
                          line_number);
                goto done;
            }
            if (struct_output.pending_param_require) {
                cg_set_error(error, error_size,
                          "line %zu: @require must precede param",
                          line_number);
                goto done;
            }
            if (function_output.active && function_output.is_method &&
                finalize_function_body(
                    &function_output, symbols, symbol_count, &module,
                    &header_deps, &header_deps_count, &header_deps_capacity,
                    line_number, error, error_size) != 0) {
                goto done;
            }
            if (function_output.active && function_output.is_method &&
                cg_close_function(
                    &function_output,
                    struct_output.active && function_output.is_method
                        ? &struct_output
                        : NULL,
                    error, error_size) != 0) {
                goto done;
            }
            if (finalize_struct(&struct_output, &struct_templates,
                                  &struct_template_count,
                                  &struct_template_capacity, &symbols,
                                  &symbol_count, &symbol_capacity, blocks,
                                  block_count, error, error_size) != 0) {
                goto done;
            }
            if (cg_close_struct(&struct_output, error, error_size) != 0) {
                cg_set_error(error, error_size, "out of memory");
                goto done;
            }
        }
        if (struct_output.active) {
            char *field_name = NULL;
            FieldType field_type = {0};
            const char *c_type;
            Symbol *field_symbol = NULL;

            if (indent == struct_output.indent + 4) {
                BlockAttributeKind kind;
                char *inline_value = NULL;
                const char *struct_text = line + indent;

                if (cg_parse_inline_block_attribute(struct_text, &kind,
                                                  &inline_value)) {
                    if (kind != BLOCK_ATTR_DOC) {
                        free(inline_value);
                        cg_set_error(error, error_size,
                                  "line %zu: unsupported attribute inside "
                                  "struct",
                                  line_number);
                        goto done;
                    }
                    {
                        DocEntry entry = {DOC_ENTRY_TEXT, inline_value};

                        if (cg_add_doc_entry(&doc_attributes, &entry) != 0) {
                            cg_free_doc_entry(&entry);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                    }
                    continue;
                }
                if (cg_parse_block_attribute_opener(struct_text, &kind)) {
                    if (kind != BLOCK_ATTR_DOC) {
                        cg_set_error(error, error_size,
                                  "line %zu: unsupported attribute inside "
                                  "struct",
                                  line_number);
                        goto done;
                    }
                    block_attribute = kind;
                    block_attribute_indent = indent;
                    block_attribute_values = 0;
                    continue;
                }
            }

            if (indent == struct_output.indent + 4 &&
                strncmp(line + indent, "param ", 6) == 0) {
                ParamRequire require = PARAM_REQUIRE_ANY;

                if (struct_output.pending_param_require) {
                    require = struct_output.pending_param_require_kind;
                    struct_output.pending_param_require = false;
                }
                if (struct_output.field_count > 0 ||
                    struct_output.macro_line_count > 0 ||
                    attribute_mutable ||
                    struct_output.field_expand ||
                    struct_output.field_pointer) {
                    cg_set_error(error, error_size,
                              "line %zu: params must precede struct fields",
                              line_number);
                    goto done;
                }
                if (append_struct_param(&struct_output, line + indent, require,
                                        &doc_attributes, line_number, error,
                                        error_size) != 0) {
                    goto done;
                }
                cg_clear_doc_attributes(&doc_attributes);
                continue;
            }

            if (indent == struct_output.indent + 4 &&
                cg_parse_require_attribute(line + indent,
                                           &struct_output.pending_param_require_kind)) {
                if (struct_output.macro_line_count > 0 ||
                    attribute_mutable ||
                    struct_output.field_expand ||
                    struct_output.field_pointer) {
                    cg_set_error(error, error_size,
                              "line %zu: @require must precede struct params",
                              line_number);
                    goto done;
                }
                if (struct_output.pending_param_require) {
                    cg_set_error(error, error_size,
                              "line %zu: @require specified more than once",
                              line_number);
                    goto done;
                }
                struct_output.pending_param_require = true;
                continue;
            }

            if (indent == struct_output.indent + 4 &&
                strcmp(line + indent, "@mutable") == 0) {
                if (attribute_mutable) {
                    cg_set_error(error, error_size,
                              "line %zu: @mutable specified more than once",
                              line_number);
                    goto done;
                }
                attribute_mutable = true;
                continue;
            }
            if (indent == struct_output.indent + 4 &&
                strcmp(line + indent, "@initializer") == 0) {
                if (attribute_initializer) {
                    cg_set_error(error, error_size,
                              "line %zu: @initializer specified more than once",
                              line_number);
                    goto done;
                }
                attribute_initializer = true;
                continue;
            }
            if (indent == struct_output.indent + 4 &&
                strcmp(line + indent, "@expand") == 0) {
                if (struct_output.field_expand) {
                    cg_set_error(error, error_size,
                              "line %zu: @expand specified more than once",
                              line_number);
                    goto done;
                }
                struct_output.field_expand = true;
                continue;
            }
            if (indent == struct_output.indent + 4 &&
                strcmp(line + indent, "@pointer") == 0) {
                if (struct_output.field_pointer) {
                    cg_set_error(error, error_size,
                              "line %zu: @pointer specified more than once",
                              line_number);
                    goto done;
                }
                struct_output.field_pointer = true;
                continue;
            }
            if (indent == struct_output.indent + 4) {
                char *macro_callee = NULL;
                char **macro_args = NULL;
                size_t macro_arg_count = 0;
                bool macro_expand = struct_output.field_expand;

                if (cg_parse_paren_call(line + indent, &macro_callee, &macro_args,
                                        &macro_arg_count)) {
                    StructTemplate *macro_template;
                    char **resolved_args = NULL;

                    if (struct_output.param_count > 0) {
                        cg_set_error(error, error_size,
                                  "line %zu: macro invocation is not allowed "
                                  "in parameterized struct",
                                  line_number);
                        free(macro_callee);
                        cg_free_cstr_array(macro_args, macro_arg_count);
                        goto done;
                    }
                    if (struct_output.pending_param_require) {
                        cg_set_error(error, error_size,
                                  "line %zu: @require must precede param",
                                  line_number);
                        free(macro_callee);
                        cg_free_cstr_array(macro_args, macro_arg_count);
                        goto done;
                    }
                    macro_template = find_struct_template(
                        struct_templates, struct_template_count, macro_callee);
                    if (!macro_template) {
                        cg_set_error(error, error_size,
                                  "line %zu: unknown field macro: %s",
                                  line_number, macro_callee);
                        free(macro_callee);
                        cg_free_cstr_array(macro_args, macro_arg_count);
                        goto done;
                    }
                    if (macro_arg_count != macro_template->param_count) {
                        cg_set_error(error, error_size,
                                  "line %zu: field macro %s expects %zu "
                                  "argument(s)",
                                  line_number, macro_callee,
                                  macro_template->param_count);
                        free(macro_callee);
                        cg_free_cstr_array(macro_args, macro_arg_count);
                        goto done;
                    }
                    resolved_args = calloc(macro_arg_count, sizeof(*resolved_args));
                    if (!resolved_args) {
                        free(macro_callee);
                        cg_free_cstr_array(macro_args, macro_arg_count);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    for (size_t i = 0; i < macro_arg_count; i++) {
                        resolved_args[i] = resolve_struct_type_argument(
                            macro_args[i], line_number, symbols, symbol_count,
                            &module, &header_deps, &header_deps_count,
                            &header_deps_capacity, error, error_size);
                        if (!resolved_args[i]) {
                            free(macro_callee);
                            cg_free_cstr_array(macro_args, macro_arg_count);
                            cg_free_cstr_array(resolved_args, i);
                            goto done;
                        }
                    }
                    if (register_struct_template_fields(&struct_output,
                                                        macro_template, error,
                                                        error_size) != 0) {
                        free(macro_callee);
                        cg_free_cstr_array(macro_args, macro_arg_count);
                        cg_free_cstr_array(resolved_args, macro_arg_count);
                        goto done;
                    }
                    if (macro_expand) {
                        if (expand_struct_macro_fields(
                                &struct_output, macro_template, resolved_args,
                                struct_output.all_mutable, error,
                                error_size) != 0) {
                            free(macro_callee);
                            cg_free_cstr_array(macro_args, macro_arg_count);
                            cg_free_cstr_array(resolved_args, macro_arg_count);
                            goto done;
                        }
                    } else {
                        char *macro_expr = build_macro_call_expression(
                            macro_template->c_name, resolved_args,
                            macro_arg_count);

                        if (!macro_expr) {
                            free(macro_callee);
                            cg_free_cstr_array(macro_args, macro_arg_count);
                            cg_free_cstr_array(resolved_args, macro_arg_count);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        if (cg_module_require_include(
                                &module, macro_template->header, &header_deps,
                                &header_deps_count,
                                &header_deps_capacity) != 0 ||
                            append_struct_macro_line(&struct_output,
                                                     macro_expr) != 0) {
                            free(macro_expr);
                            free(macro_callee);
                            cg_free_cstr_array(macro_args, macro_arg_count);
                            cg_free_cstr_array(resolved_args, macro_arg_count);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        free(macro_expr);
                    }
                    free(macro_callee);
                    cg_free_cstr_array(macro_args, macro_arg_count);
                    cg_free_cstr_array(resolved_args, macro_arg_count);
                    struct_output.field_expand = false;
                    struct_output.field_pointer = false;
                    continue;
                }
            }
            if (indent == struct_output.indent + 4) {
                char *fn_name = NULL;
                FieldType fn_return_type = {0};
                bool method_mutable = attribute_mutable;

                if (cg_parse_fn(line + indent, &fn_name, &fn_return_type)) {
                    if (attribute_public || attribute_private ||
                        attribute_extern || attribute_opaque ||
                        attribute_define || attribute_expand ||
                        attribute_internal) {
                        free(fn_name);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size,
                                  "line %zu: unsupported attribute on "
                                  "struct fn",
                                  line_number);
                        goto done;
                    }
                    if (attribute_pointer) {
                        if (!fn_return_type.name) {
                            free(fn_name);
                            cg_free_field_type(&fn_return_type);
                            cg_set_error(error, error_size,
                                      "line %zu: @pointer requires a typed fn",
                                      line_number);
                            goto done;
                        }
                        fn_return_type.is_ptr = true;
                    }
                    if (attribute_initializer && struct_output.param_count > 0) {
                        free(fn_name);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size,
                                  "line %zu: @initializer is not allowed on "
                                  "parameterized struct",
                                  line_number);
                        goto done;
                    }
                    if (begin_struct_method(
                            &struct_output, blocks, block_count, fn_name,
                            &fn_return_type, method_mutable,
                            attribute_initializer, doc_attributes,
                            &symbols, &symbol_count, &symbol_capacity,
                            &header_deps, &header_deps_count,
                            &header_deps_capacity, &function_output, indent,
                            line_number, error, error_size) != 0) {
                        free(fn_name);
                        cg_free_field_type(&fn_return_type);
                        goto done;
                    }
                    free(fn_name);
                    cg_free_field_type(&fn_return_type);
                    doc_attributes = (DocAttributes) {0};
                    reset_compiler_attributes(&attribute_count, &attribute_noscope,
                                   &attribute_public, &attribute_private,
                                   &attribute_extern, &attribute_opaque,
                                   &attribute_internal, &attribute_define,
                                   &attribute_mutable, &attribute_expand,
                                   &attribute_pointer, &attribute_initializer);
                    continue;
                }
                free(fn_name);
                cg_free_field_type(&fn_return_type);
            }
            if (indent != struct_output.indent + 4 ||
                !cg_parse_field(line + indent, &field_name, &field_type)) {
                cg_set_error(error, error_size,
                          "line %zu: expected field inside struct",
                          line_number);
                goto done;
            }
            {
                ParamRequire require = PARAM_REQUIRE_ANY;
                char *field_doc = NULL;

                if (struct_output.pending_param_require) {
                    require = struct_output.pending_param_require_kind;
                    struct_output.pending_param_require = false;
                }
                if (field_type.is_param_ref) {
                    if (ensure_struct_meta_param(
                            &struct_output, field_type.name, require,
                            &doc_attributes, line_number, error,
                            error_size) != 0) {
                        free(field_name);
                        cg_free_field_type(&field_type);
                        goto done;
                    }
                    cg_clear_doc_attributes(&doc_attributes);
                } else if (doc_attributes.entry_count > 0) {
                    field_doc = doc_attributes_to_text(&doc_attributes);
                    cg_clear_doc_attributes(&doc_attributes);
                    if (!field_doc) {
                        cg_set_error(error, error_size, "out of memory");
                        free(field_name);
                        cg_free_field_type(&field_type);
                        goto done;
                    }
                }
                if (struct_output.field_pointer) {
                    field_type.is_ptr = true;
                }
                {
                    const char *template_type = cg_param_c_type(
                        (const char **) struct_output.params,
                        struct_output.param_variadic,
                        struct_output.param_count, field_type.name);

                    if (template_type) {
                        if (check_param_type_use(
                                (const char **) struct_output.params,
                                struct_output.param_requires,
                                struct_output.param_count, field_type.name,
                                line_number, error, error_size) != 0) {
                            free(field_name);
                            free(field_doc);
                            cg_free_field_type(&field_type);
                            goto done;
                        }
                        c_type = template_type;
                    } else {
                        field_symbol = cg_find_symbol(symbols, symbol_count,
                                                   field_type.name);
                        if (!field_symbol) {
                            cg_set_error(error, error_size,
                                      "line %zu: unknown field type: %s",
                                      line_number, field_type.name);
                            free(field_name);
                            free(field_doc);
                            cg_free_field_type(&field_type);
                            goto done;
                        }
                        c_type = cg_binding_value(field_symbol,
                                               field_type.is_ptr &&
                                               struct_output.field_expand);
                        if (cg_module_require_include(
                                &module, field_symbol->header, &header_deps,
                                &header_deps_count, &header_deps_capacity) !=
                            0) {
                            free(field_name);
                            free(field_doc);
                            cg_free_field_type(&field_type);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                    }
                }
                if (struct_output.field_count == struct_output.field_capacity) {
                    size_t capacity = struct_output.field_capacity
                                          ? struct_output.field_capacity * 2
                                          : 4;
                    StructField *grown = realloc(
                        struct_output.fields,
                        capacity * sizeof(*struct_output.fields));

                    if (!grown) {
                        free(field_name);
                        free(field_doc);
                        cg_free_field_type(&field_type);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    struct_output.fields = grown;
                    struct_output.field_capacity = capacity;
                }
                {
                    char *resolved_type = field_type.is_ptr
                                              ? cg_make_pointer_type(c_type)
                                              : strdup(c_type);

                    if (!resolved_type) {
                        free(field_name);
                        free(field_doc);
                        cg_free_field_type(&field_type);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    cg_free_field_type(&field_type);
                    field_type.name = resolved_type;
                }
                {
                    bool field_is_mutable =
                        struct_output.all_mutable || attribute_mutable;

                    if (register_struct_known_field(&struct_output, field_name,
                                                    field_is_mutable, error,
                                                    error_size) != 0) {
                        free(field_name);
                        free(field_type.name);
                        free(field_doc);
                        goto done;
                    }
                    struct_output.fields[struct_output.field_count++] =
                        (StructField) {
                            field_name, field_type.name, field_is_mutable,
                            field_doc
                        };
                }
            }
            attribute_mutable = false;
            struct_output.field_expand = false;
            struct_output.field_pointer = false;
            continue;
        }
        if (if_depth > 0) {
            const char *line_text = line + indent;
            char *pp_condition = NULL;

            if (indent == if_frames[if_depth - 1].indent &&
                cg_parse_elif_block(line_text, &pp_condition)) {
                char *transformed = NULL;

                if (function_output.active || struct_output.active ||
                    enum_output.active) {
                    free(pp_condition);
                    cg_set_error(error, error_size,
                                 "line %zu: elif is not allowed here",
                                 line_number);
                    goto done;
                }
                if (!if_frames[if_depth - 1].branch_open) {
                    free(pp_condition);
                    cg_set_error(error, error_size,
                                 "line %zu: elif after else", line_number);
                    goto done;
                }
                transformed = transform_pp_condition(
                    pp_condition, symbols, symbol_count, &module,
                    &header_deps, &header_deps_count,
                    &header_deps_capacity, error, error_size);
                free(pp_condition);
                if (!transformed) {
                    goto done;
                }
                if (module.relative_header &&
                    cg_should_emit_c(blocks, block_count, attribute_internal,
                                     attribute_public) &&
                    emit_pp_branch(&module, "elif", transformed, error,
                                   error_size) != 0) {
                    free(transformed);
                    goto done;
                }
                free(transformed);
                if_frames[if_depth - 1].branch_open = true;
                continue;
            }
            if (indent == if_frames[if_depth - 1].indent &&
                cg_parse_else_block(line_text)) {
                if (function_output.active || struct_output.active ||
                    enum_output.active) {
                    cg_set_error(error, error_size,
                                 "line %zu: else is not allowed here",
                                 line_number);
                    goto done;
                }
                if (!if_frames[if_depth - 1].branch_open) {
                    cg_set_error(error, error_size,
                                 "line %zu: duplicate else", line_number);
                    goto done;
                }
                if (module.relative_header &&
                    cg_should_emit_c(blocks, block_count, attribute_internal,
                                     attribute_public) &&
                    cg_emit_preprocessor(&module, "else", error,
                                         error_size) != 0) {
                    goto done;
                }
                if_frames[if_depth - 1].branch_open = false;
                continue;
            }
            if (indent <= if_frames[if_depth - 1].indent &&
                cg_close_if_frames(&if_frames, &if_depth, indent, &module,
                                   error, error_size) != 0) {
                goto done;
            }
        }
        while (block_count && indent <= blocks[block_count - 1].indent) {
            if (cg_pop_block(blocks, &block_count, &module, header_deps,
                          header_deps_count, format_style, error,
                          error_size) != 0) {
                goto done;
            }
        }
        text = line + indent;

        {
            BlockAttributeKind kind;
            char *inline_value = NULL;

            if (cg_parse_inline_block_attribute(text, &kind, &inline_value)) {
                if (attribute_count && indent != attribute_indent) {
                    cg_set_error(error, error_size,
                              "line %zu: attributes must share indentation",
                              line_number);
                    free(inline_value);
                    goto done;
                }
                attribute_indent = indent;
                attribute_count++;
                if (kind == BLOCK_ATTR_DOC) {
                    DocEntry entry = {DOC_ENTRY_TEXT, inline_value};

                    if (cg_add_doc_entry(&doc_attributes, &entry) != 0) {
                        cg_free_doc_entry(&entry);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                } else if (cg_add_include_attribute(&include_attributes,
                                                    inline_value) != 0) {
                    cg_set_error(error, error_size, "out of memory");
                    goto done;
                }
                continue;
            }
            {
                const char *call_name;
                size_t call_name_length;
                char **call_args = NULL;
                size_t call_arg_count = 0;

                if (cg_parse_attribute_call(text, &call_name, &call_name_length,
                                          &call_args, &call_arg_count)) {
                    if (call_name_length == strlen("initializer") &&
                        memcmp(call_name, "initializer", call_name_length) ==
                            0) {
                        cg_free_cstr_array(call_args, call_arg_count);
                        cg_set_error(error, error_size,
                                     "line %zu: @initializer does not take "
                                     "arguments",
                                     line_number);
                        goto done;
                    }
                    cg_free_cstr_array(call_args, call_arg_count);
                }
            }
            if (cg_parse_block_attribute_opener(text, &kind)) {
                if (attribute_count && indent != attribute_indent) {
                    cg_set_error(error, error_size,
                              "line %zu: attributes must share indentation",
                              line_number);
                    goto done;
                }
                attribute_indent = indent;
                attribute_count++;
                block_attribute = kind;
                block_attribute_indent = indent;
                block_attribute_values = 0;
                continue;
            }
        }
        if (cg_parse_attribute(text, &attribute_name, &attribute_name_length)) {
            if (attribute_count && indent != attribute_indent) {
                cg_set_error(error, error_size,
                          "line %zu: attributes must share indentation",
                          line_number);
                goto done;
            }
            attribute_indent = indent;
            attribute_count++;
            if (attribute_name_length == strlen("noscope") &&
                memcmp(attribute_name, "noscope",
                       attribute_name_length) == 0) {
                attribute_noscope = true;
            } else if (attribute_name_length == strlen("public") &&
                       memcmp(attribute_name, "public",
                              attribute_name_length) == 0) {
                attribute_public = true;
            } else if (attribute_name_length == strlen("private") &&
                       memcmp(attribute_name, "private",
                              attribute_name_length) == 0) {
                attribute_private = true;
            } else if (attribute_name_length == strlen("extern") &&
                       memcmp(attribute_name, "extern",
                              attribute_name_length) == 0) {
                attribute_extern = true;
            } else if (attribute_name_length == strlen("opaque") &&
                       memcmp(attribute_name, "opaque",
                              attribute_name_length) == 0) {
                attribute_opaque = true;
            } else if (attribute_name_length == strlen("internal") &&
                       memcmp(attribute_name, "internal",
                              attribute_name_length) == 0) {
                attribute_internal = true;
            } else if (attribute_name_length == strlen("define") &&
                       memcmp(attribute_name, "define",
                              attribute_name_length) == 0) {
                attribute_define = true;
            } else if (attribute_name_length == strlen("mutable") &&
                       memcmp(attribute_name, "mutable",
                              attribute_name_length) == 0) {
                if (attribute_mutable) {
                    cg_set_error(error, error_size,
                              "line %zu: @mutable specified more than once",
                              line_number);
                    goto done;
                }
                attribute_mutable = true;
            } else if (attribute_name_length == strlen("expand") &&
                       memcmp(attribute_name, "expand",
                              attribute_name_length) == 0) {
                if (attribute_expand) {
                    cg_set_error(error, error_size,
                              "line %zu: @expand specified more than once",
                              line_number);
                    goto done;
                }
                attribute_expand = true;
            } else if (attribute_name_length == strlen("pointer") &&
                       memcmp(attribute_name, "pointer",
                              attribute_name_length) == 0) {
                if (attribute_pointer) {
                    cg_set_error(error, error_size,
                              "line %zu: @pointer specified more than once",
                              line_number);
                    goto done;
                }
                attribute_pointer = true;
            } else if (attribute_name_length == strlen("initializer") &&
                       memcmp(attribute_name, "initializer",
                              attribute_name_length) == 0) {
                if (attribute_initializer) {
                    cg_set_error(error, error_size,
                              "line %zu: @initializer specified more than once",
                              line_number);
                    goto done;
                }
                attribute_initializer = true;
            } else {
                cg_set_error(error, error_size,
                          "line %zu: unknown attribute @%.*s",
                          line_number, (int) attribute_name_length,
                          attribute_name);
                goto done;
            }
            continue;
        }
        if (text[0] == '@') {
            cg_set_error(error, error_size,
                      "line %zu: invalid attribute syntax", line_number);
            goto done;
        }
        if (attribute_count && indent != attribute_indent) {
            cg_set_error(error, error_size,
                      "line %zu: attributes must precede an object "
                      "at the same indentation",
                      line_number);
            goto done;
        }

        {
            char *pp_condition = NULL;

            if (cg_parse_if_block(text, &pp_condition)) {
                char *transformed = NULL;
                bool if_emit;
                bool if_define_mode = attribute_define;

                if (function_output.active || struct_output.active ||
                    enum_output.active) {
                    free(pp_condition);
                    cg_set_error(error, error_size,
                                 "line %zu: if is not allowed here",
                                 line_number);
                    goto done;
                }
                if (!module.relative_header || !block_count ||
                    indent != module_body_indent(blocks, block_count, if_frames,
                                                 if_depth) + 4) {
                    free(pp_condition);
                    cg_set_error(error, error_size,
                                 "line %zu: if must be inside a module",
                                 line_number);
                    goto done;
                }
                if (attribute_public || attribute_private || attribute_mutable ||
                    attribute_extern || attribute_opaque || attribute_expand ||
                    attribute_pointer || attribute_noscope || attribute_internal ||
                    doc_attributes.entry_count > 0 ||
                    include_attributes.count > 0) {
                    free(pp_condition);
                    cg_set_error(error, error_size,
                                 "line %zu: unsupported attribute on if",
                                 line_number);
                    goto done;
                }
                transformed = transform_pp_condition(
                    pp_condition, symbols, symbol_count, &module,
                    &header_deps, &header_deps_count,
                    &header_deps_capacity, error, error_size);
                free(pp_condition);
                if (!transformed) {
                    goto done;
                }
                if_emit = cg_should_emit_c(blocks, block_count,
                                           attribute_internal,
                                           attribute_public);
                if (if_emit &&
                    emit_pp_branch(&module, "if", transformed, error,
                                   error_size) != 0) {
                    free(transformed);
                    goto done;
                }
                free(transformed);
                if (push_if_frame(&if_frames, &if_depth, &if_capacity, indent,
                                  if_define_mode, error, error_size) != 0) {
                    goto done;
                }
                attribute_count = 0;
                attribute_define = false;
                continue;
            }
        }

        if (cg_parse_named_block(text, "package", &name)) {
            kind = BLOCK_PACKAGE;
        } else if (cg_parse_named_block(text, "module", &name)) {
            kind = BLOCK_MODULE;
        } else if (cg_parse_named_block(text, "scope", &name)) {
            kind = BLOCK_SCOPE;
        } else {
            const char *expression = NULL;
            size_t expression_length = 0;
            char *symbol;
            char *base = NULL;
            char *alias_reference = NULL;
            ExprArg *expr_args = NULL;
            size_t expr_arg_count = 0;

            if (cg_parse_named_block(text, "struct", &name)) {
                if (!module.relative_header || !block_count ||
                    blocks[block_count - 1].kind != BLOCK_MODULE ||
                    indent != blocks[block_count - 1].indent + 4) {
                    free(name);
                    cg_set_error(error, error_size,
                              "line %zu: struct must be inside a module",
                              line_number);
                    goto done;
                }
                if (!cg_is_module_export_name(name)) {
                    free(name);
                    cg_set_error(error, error_size,
                              "line %zu: parameterized struct must be struct module:",
                              line_number);
                    goto done;
                }
                if (attribute_public && attribute_private) {
                    free(name);
                    cg_set_error(error, error_size,
                              "line %zu: @public and @private are incompatible",
                              line_number);
                    goto done;
                }
                if (attribute_private || attribute_extern ||
                    attribute_opaque || attribute_define ||
                    attribute_expand || attribute_pointer) {
                    free(name);
                    cg_set_error(error, error_size,
                              "line %zu: unsupported attribute on parameterized struct",
                              line_number);
                    goto done;
                }
                if (cg_note_primary_export(blocks, block_count, name,
                                           line_number, error,
                                           error_size) != 0) {
                    free(name);
                    goto done;
                }
                symbol = cg_build_export_symbol(blocks, block_count, name,
                                                false);
                if (!symbol) {
                    free(name);
                    cg_set_error(error, error_size, "out of memory");
                    goto done;
                }
                {
                    bool struct_emit = cg_should_emit_c(blocks, block_count,
                                                     attribute_internal,
                                                     attribute_public);

                    if (flush_includes(&module, &include_attributes, &header_deps,
                                       &header_deps_count, &header_deps_capacity,
                                       struct_emit, false, error,
                                       error_size) != 0) {
                        free(symbol);
                        free(name);
                        goto done;
                    }
                    if (struct_emit) {
                        if (cg_ensure_module_header(&module, error,
                                                 error_size) != 0) {
                            free(symbol);
                            free(name);
                            goto done;
                        }
                        if (module.header_has_declaration) {
                            cg_flush_module_blank_lines(&module);
                        } else {
                            module.pending_blank_lines = 0;
                        }
                    }
                    {
                        char *struct_dsl_name = block_count ?
                            cg_build_dsl_name(blocks, block_count - 1,
                                              blocks[block_count - 1].name) :
                            NULL;

                        struct_output = (StructOutput) {
                            .active = true,
                            .indent = indent,
                            .c_name = symbol,
                            .dsl_name = struct_dsl_name,
                            .header = module.relative_header
                                           ? strdup(module.relative_header)
                                           : NULL,
                            .module = &module,
                            .all_mutable = attribute_mutable,
                            .emit = struct_emit,
                            .docs = doc_attributes,
                        };
                        doc_attributes = (DocAttributes) {0};
                    }
                    if (!struct_output.dsl_name || !struct_output.header) {
                        cg_clear_doc_attributes(&struct_output.docs);
                        free(struct_output.dsl_name);
                        free(struct_output.header);
                        free(struct_output.c_name);
                        struct_output = (StructOutput) {0};
                        free(name);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    if (struct_emit) {
                        if (!cg_is_module_export_name(name)) {
                            if (cg_emit_doc_comment(&module, NULL,
                                                 &struct_output.docs) != 0) {
                                free(name);
                                cg_set_error(error, error_size,
                                           "out of memory");
                                goto done;
                            }
                            cg_clear_doc_attributes(&struct_output.docs);
                        }
                        module.header_has_declaration = true;
                    }
                }
                free(name);
                reset_compiler_attributes(&attribute_count, &attribute_noscope,
                                           &attribute_public, &attribute_private,
                                           &attribute_extern, &attribute_opaque,
                                           &attribute_internal, &attribute_define,
                                           &attribute_mutable, &attribute_expand,
                                           &attribute_pointer, &attribute_initializer);
                continue;
            }

            if (cg_parse_enum(text, &name, &base)) {
                Symbol *base_symbol;

                if (attribute_mutable) {
                    free(name);
                    free(base);
                    cg_set_error(error, error_size,
                              "line %zu: @mutable does not apply to enum",
                              line_number);
                    goto done;
                }
                if (attribute_expand) {
                    free(name);
                    free(base);
                    cg_set_error(error, error_size,
                              "line %zu: @expand applies only to type or field",
                              line_number);
                    goto done;
                }
                if (attribute_pointer) {
                    free(name);
                    free(base);
                    cg_set_error(error, error_size,
                              "line %zu: @pointer applies only to type, field, "
                              "let, param, or fn return type",
                              line_number);
                    goto done;
                }
                if (!module.relative_header || !block_count ||
                    indent != blocks[block_count - 1].indent + 4) {
                    free(name);
                    free(base);
                    cg_set_error(error, error_size,
                              "line %zu: enum must be inside a module",
                              line_number);
                    goto done;
                }
                base_symbol = cg_find_symbol(symbols, symbol_count, base);
                if (!base_symbol) {
                    cg_set_error(error, error_size,
                              "line %zu: unknown enum base type: %s",
                              line_number, base);
                    free(name);
                    free(base);
                    goto done;
                }
                if (attribute_public && attribute_private) {
                    free(name);
                    free(base);
                    cg_set_error(error, error_size,
                              "line %zu: @public and @private are incompatible",
                              line_number);
                    goto done;
                }
                if (cg_note_primary_export(blocks, block_count, name,
                                           line_number, error,
                                           error_size) != 0) {
                    free(name);
                    free(base);
                    goto done;
                }
                symbol = cg_build_export_symbol(blocks, block_count, name,
                                                true);
                if (!symbol) {
                    free(name);
                    free(base);
                    cg_set_error(error, error_size, "out of memory");
                    goto done;
                }
                bool use_source = attribute_private;
                bool emit = cg_should_emit_c(blocks, block_count,
                                          attribute_internal, attribute_public);

                if (emit && !cg_compile_analyze_only) {
                    if (attribute_private) {
                        if (cg_ensure_module_source(&module, error, error_size) != 0) {
                            free(symbol);
                            free(name);
                            free(base);
                            goto done;
                        }
                        if (!base_symbol->is_internal &&
                            strcmp(base_symbol->header,
                                    module.relative_header) != 0) {
                            fprintf(module.source_file, "#include \"%s\"\n\n",
                                    base_symbol->header);
                        }
                        if (cg_emit_doc_comment(&module, module.source_file,
                                             &doc_attributes) != 0) {
                            free(symbol);
                            free(name);
                            free(base);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        fprintf(module.source_file, "typedef %s %s;\n\n",
                                base_symbol->c_name, symbol);
                    } else {
                        if (cg_ensure_module_header(&module, error,
                                                 error_size) != 0) {
                            free(symbol);
                            free(name);
                            free(base);
                            goto done;
                        }
                        module.header_has_declaration = true;
                        if (!base_symbol->is_internal &&
                            cg_module_require_include(&module, base_symbol->header,
                                                   &header_deps,
                                                   &header_deps_count,
                                                   &header_deps_capacity) != 0) {
                            free(symbol);
                            free(name);
                            free(base);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        cg_flush_module_blank_lines(&module);
                        if (cg_emit_doc_comment(&module, NULL,
                                             &doc_attributes) != 0) {
                            free(symbol);
                            free(name);
                            free(base);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        if (cg_module_body_printf(&module, "typedef %s %s;\n\n",
                                               base_symbol->c_name,
                                               symbol) != 0) {
                            free(symbol);
                            free(name);
                            free(base);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                    }
                }
                char *type_name = strdup(symbol);
                if (!type_name) {
                    free(symbol);
                    free(name);
                    free(base);
                    cg_set_error(error, error_size, "out of memory");
                    goto done;
                }
                size_t symbol_length = strlen(symbol);
                if (symbol_length >= 2 &&
                    strcmp(symbol + symbol_length - 2, "_t") == 0) {
                    symbol[symbol_length - 2] = '\0';
                }
                enum_output = (EnumOutput) {
                    true, indent, symbol, type_name, &module, use_source,
                    attribute_define, emit, 0, NULL, NULL
                };
                enum_output.local_name = cg_resolve_export_name(
                    blocks, block_count, name);
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
                if (!enum_output.local_name || !enum_output.dsl_name) {
                    free(enum_output.local_name);
                    free(enum_output.dsl_name);
                    enum_output.local_name = NULL;
                    enum_output.dsl_name = NULL;
                    cg_close_enum(&enum_output);
                    free(symbol);
                    free(name);
                    free(base);
                    cg_set_error(error, error_size, "out of memory");
                    goto done;
                }
                {
                    char *enum_dsl = strdup(enum_output.dsl_name);
                    char *enum_base_dsl = strdup(base);

                    if (!enum_dsl || !enum_base_dsl ||
                        cg_add_symbol_ex(&symbols, &symbol_count, &symbol_capacity,
                                         enum_dsl, type_name,
                                         module.relative_header,
                                         base_symbol->c_name, false, !emit,
                                         false, SYMBOL_KIND_TYPE,
                                         enum_base_dsl) != 0) {
                        free(enum_dsl);
                        free(enum_base_dsl);
                        cg_close_enum(&enum_output);
                        free(symbol);
                        free(name);
                        free(base);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    if (cg_is_module_export_name(enum_output.local_name) &&
                        cg_add_primary_export_alias(
                            blocks, block_count, &symbols, &symbol_count,
                            &symbol_capacity) != 0) {
                        cg_close_enum(&enum_output);
                        free(symbol);
                        free(name);
                        free(base);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                }
                free(name);
                free(base);
                if (flush_includes(&module, &include_attributes, &header_deps,
                                   &header_deps_count, &header_deps_capacity,
                                   emit, use_source, error, error_size) != 0) {
                    cg_close_enum(&enum_output);
                    goto done;
                }
                reset_compiler_attributes(&attribute_count, &attribute_noscope,
                                           &attribute_public, &attribute_private,
                                           &attribute_extern, &attribute_opaque,
                                           &attribute_internal, &attribute_define,
                                           &attribute_mutable, &attribute_expand,
                                           &attribute_pointer, &attribute_initializer);
                cg_clear_doc_attributes(&doc_attributes);
                continue;
            }

            {
                char *let_name = NULL;
                FieldType let_type = {0};
                char *let_value = NULL;
                char *let_symbol = NULL;
                char *let_dsl = NULL;
                const char *let_c_type = NULL;
                bool let_use_source;
                bool let_emit;

                if (cg_parse_let(text, &let_name, &let_type, &let_value)) {
                    bool let_use_define = if_chain_define_mode(
                        if_frames, if_depth, indent, attribute_define);

                    if (!module.relative_header) {
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        cg_set_error(error, error_size,
                                  "line %zu: let must be inside a module",
                                  line_number);
                        goto done;
                    }
                    if (!block_count ||
                        indent != module_body_indent(blocks, block_count,
                                                     if_frames, if_depth) + 4) {
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        cg_set_error(error, error_size,
                                  "line %zu: invalid let indentation",
                                  line_number);
                        goto done;
                    }
                    if (attribute_public && attribute_private) {
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        cg_set_error(error, error_size,
                                  "line %zu: @public and @private are incompatible",
                                  line_number);
                        goto done;
                    }
                    if (attribute_internal &&
                        (attribute_public || attribute_private ||
                         attribute_define || attribute_extern ||
                         attribute_opaque)) {
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        cg_set_error(error, error_size,
                                  "line %zu: @internal is incompatible with "
                                  "visibility, linkage, or @define attributes",
                                  line_number);
                        goto done;
                    }
                    if (cg_note_primary_export(blocks, block_count, let_name,
                                               line_number, error,
                                               error_size) != 0) {
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        goto done;
                    }
                    if (!let_type.name) {
                        char *export_name;

                        if (attribute_mutable || attribute_extern ||
                            attribute_opaque || attribute_expand) {
                            free(let_name);
                            free(let_value);
                            cg_set_error(error, error_size,
                                      "line %zu: untyped let is a macro and "
                                      "does not support storage attributes",
                                      line_number);
                            goto done;
                        }
                        export_name = cg_resolve_export_name(
                            blocks, block_count, let_name);
                        let_symbol = cg_build_export_symbol(
                            blocks, block_count, let_name, false);
                        let_dsl = export_name ? cg_build_dsl_name(
                            blocks, block_count, export_name) : NULL;
                        free(export_name);
                        if (!let_symbol || !let_dsl) {
                            free(let_symbol);
                            free(let_dsl);
                            free(let_name);
                            free(let_value);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        if (let_value) {
                            char *transformed = transform_function_expression(
                                let_value, NULL, symbols, symbol_count,
                                &module, &header_deps, &header_deps_count,
                                &header_deps_capacity, line_number, error,
                                error_size);

                            if (!transformed) {
                                free(let_symbol);
                                free(let_dsl);
                                free(let_name);
                                free(let_value);
                                goto done;
                            }
                            free(let_value);
                            let_value = transformed;
                        }
                        let_use_source = attribute_private;
                        let_emit = cg_should_emit_c(blocks, block_count,
                                                   attribute_internal,
                                                   attribute_public);
                        if (let_emit &&
                            cg_emit_let(&module, let_use_source, true, true,
                                        false, false, false, &doc_attributes,
                                        NULL, let_symbol, let_value, error,
                                        error_size) != 0) {
                            free(let_symbol);
                            free(let_dsl);
                            free(let_name);
                            free(let_value);
                            goto done;
                        }
                        if (cg_add_symbol_ex(&symbols, &symbol_count,
                                             &symbol_capacity, let_dsl,
                                             let_symbol, module.relative_header,
                                             let_value, true, !let_emit, false,
                                             SYMBOL_KIND_MACRO, NULL) != 0) {
                            free(let_symbol);
                            free(let_dsl);
                            free(let_name);
                            free(let_value);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        if (cg_is_module_export_name(let_name) &&
                            cg_add_primary_export_alias(
                                blocks, block_count, &symbols, &symbol_count,
                                &symbol_capacity) != 0) {
                            free(let_symbol);
                            free(let_name);
                            free(let_value);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        free(let_symbol);
                        free(let_name);
                        free(let_value);
                        if (flush_includes(
                                &module, &include_attributes, &header_deps,
                                &header_deps_count, &header_deps_capacity,
                                let_emit, let_use_source, error,
                                error_size) != 0) {
                            goto done;
                        }
                        reset_compiler_attributes(&attribute_count, &attribute_noscope,
                                       &attribute_public, &attribute_private,
                                       &attribute_extern, &attribute_opaque,
                                       &attribute_internal, &attribute_define,
                                       &attribute_mutable, &attribute_expand,
                                       &attribute_pointer, &attribute_initializer);
                        cg_clear_doc_attributes(&doc_attributes);
                        continue;
                    }
                    if (!let_value) {
                        free(let_name);
                        cg_free_field_type(&let_type);
                        cg_set_error(error, error_size,
                                     "line %zu: typed let requires a value",
                                     line_number);
                        goto done;
                    }
                    if (attribute_pointer) {
                        let_type.is_ptr = true;
                    }
                    let_c_type = resolve_c_field_type(
                        &let_type, symbols, symbol_count, &module,
                        &header_deps, &header_deps_count,
                        &header_deps_capacity, line_number, error,
                        error_size);
                    if (!let_c_type) {
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        goto done;
                    }
                    {
                        char *export_name = cg_resolve_export_name(
                            blocks, block_count, let_name);

                        if (!export_name) {
                            free(let_name);
                            cg_free_field_type(&let_type);
                            free(let_value);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        let_symbol = cg_build_export_symbol(
                            blocks, block_count, let_name, false);
                        let_dsl = cg_build_dsl_name(blocks, block_count,
                                                    export_name);
                        free(export_name);
                    }
                    if (!let_symbol || !let_dsl) {
                        free(let_symbol);
                        free(let_dsl);
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    {
                        char *transformed = transform_function_expression(
                            let_value, NULL, symbols, symbol_count, &module,
                            &header_deps, &header_deps_count,
                            &header_deps_capacity, line_number, error,
                            error_size);

                        if (!transformed) {
                            free(let_symbol);
                            free(let_dsl);
                            free(let_name);
                            cg_free_field_type(&let_type);
                            free(let_value);
                            goto done;
                        }
                        free(let_value);
                        let_value = transformed;
                    }
                    let_use_source = attribute_private;
                    let_emit = cg_should_emit_c(blocks, block_count,
                                             attribute_internal,
                                             attribute_public);
                    if (let_emit &&
                        cg_emit_let(&module, let_use_source, let_use_define,
                                    true, attribute_mutable, attribute_extern,
                                    attribute_opaque, &doc_attributes,
                                    let_c_type, let_symbol, let_value, error,
                                    error_size) != 0) {
                        free(let_symbol);
                        free(let_dsl);
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        goto done;
                    }
                    if (cg_add_symbol_ex(&symbols, &symbol_count, &symbol_capacity,
                                         let_dsl, let_symbol,
                                         module.relative_header, NULL,
                                         let_use_define, !let_emit,
                                         attribute_mutable, SYMBOL_KIND_VALUE,
                                         copy_field_type_dsl(&let_type)) != 0) {
                        free(let_symbol);
                        free(let_dsl);
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    if (cg_is_module_export_name(let_name) &&
                        cg_add_primary_export_alias(
                            blocks, block_count, &symbols, &symbol_count,
                            &symbol_capacity) != 0) {
                        free(let_symbol);
                        free(let_name);
                        cg_free_field_type(&let_type);
                        free(let_value);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    free(let_symbol);
                    free(let_name);
                    cg_free_field_type(&let_type);
                    free(let_value);
                    if (flush_includes(&module, &include_attributes, &header_deps,
                                       &header_deps_count, &header_deps_capacity,
                                       let_emit, let_use_source, error,
                                       error_size) != 0) {
                        goto done;
                    }
                    reset_compiler_attributes(&attribute_count, &attribute_noscope,
                                   &attribute_public, &attribute_private,
                                   &attribute_extern, &attribute_opaque,
                                   &attribute_internal, &attribute_define,
                                   &attribute_mutable, &attribute_expand,
                                   &attribute_pointer, &attribute_initializer);
                    cg_clear_doc_attributes(&doc_attributes);
                    continue;
                }
            }

            {
                char *fn_name = NULL;
                FieldType fn_return_type = {0};
                char *fn_symbol = NULL;
                char *fn_dsl = NULL;
                char *fn_c_return_type = NULL;
                bool fn_emit;

                if (cg_parse_fn(text, &fn_name, &fn_return_type)) {
                    if (!module.relative_header) {
                        free(fn_name);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size,
                                  "line %zu: fn must be inside a module",
                                  line_number);
                        goto done;
                    }
                    if (!block_count ||
                        indent != blocks[block_count - 1].indent + 4) {
                        free(fn_name);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size,
                                  "line %zu: invalid fn indentation",
                                  line_number);
                        goto done;
                    }
                    if (attribute_public && attribute_private) {
                        free(fn_name);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size,
                                  "line %zu: @public and @private are incompatible",
                                  line_number);
                        goto done;
                    }
                    if (attribute_define || attribute_mutable ||
                        attribute_extern || attribute_opaque ||
                        attribute_expand || attribute_initializer) {
                        free(fn_name);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size,
                                  "line %zu: unsupported attribute on fn",
                                  line_number);
                        goto done;
                    }
                    if (attribute_internal &&
                        (attribute_public || attribute_private)) {
                        free(fn_name);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size,
                                  "line %zu: @internal is incompatible with "
                                  "visibility attributes",
                                  line_number);
                        goto done;
                    }
                    if (cg_note_primary_export(blocks, block_count, fn_name,
                                               line_number, error,
                                               error_size) != 0) {
                        free(fn_name);
                        cg_free_field_type(&fn_return_type);
                        goto done;
                    }
                    if (attribute_pointer) {
                        if (!fn_return_type.name) {
                            free(fn_name);
                            cg_free_field_type(&fn_return_type);
                            cg_set_error(error, error_size,
                                      "line %zu: @pointer requires a typed fn",
                                      line_number);
                            goto done;
                        }
                        fn_return_type.is_ptr = true;
                    }
                    if (fn_return_type.name) {
                        fn_c_return_type = resolve_c_field_type(
                            &fn_return_type, symbols, symbol_count, &module,
                            &header_deps, &header_deps_count,
                            &header_deps_capacity, line_number, error,
                            error_size);
                        if (!fn_c_return_type) {
                            free(fn_name);
                            cg_free_field_type(&fn_return_type);
                            goto done;
                        }
                    }
                    {
                        char *export_name = cg_resolve_export_name(
                            blocks, block_count, fn_name);

                        if (!export_name) {
                            free(fn_name);
                            free(fn_c_return_type);
                            cg_free_field_type(&fn_return_type);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                        fn_symbol = cg_build_export_symbol(
                            blocks, block_count, fn_name, false);
                        fn_dsl = cg_build_dsl_name(blocks, block_count,
                                                   export_name);
                        free(export_name);
                    }
                    if (!fn_symbol || !fn_dsl) {
                        free(fn_symbol);
                        free(fn_dsl);
                        free(fn_name);
                        free(fn_c_return_type);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    fn_emit = cg_should_emit_c(blocks, block_count,
                                               attribute_internal,
                                               attribute_public);
                    if (flush_includes(&module, &include_attributes,
                                       &header_deps, &header_deps_count,
                                       &header_deps_capacity, fn_emit,
                                       attribute_private, error,
                                       error_size) != 0) {
                        free(fn_symbol);
                        free(fn_dsl);
                        free(fn_name);
                        free(fn_c_return_type);
                        cg_free_field_type(&fn_return_type);
                        goto done;
                    }
                    if (cg_add_symbol_ex(&symbols, &symbol_count, &symbol_capacity,
                                         fn_dsl, fn_symbol,
                                         module.relative_header, NULL, false,
                                         !fn_emit, false, SYMBOL_KIND_FN,
                                         copy_field_type_dsl(&fn_return_type)) !=
                        0) {
                        free(fn_symbol);
                        free(fn_name);
                        free(fn_c_return_type);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    if (cg_is_module_export_name(fn_name) &&
                        cg_add_primary_export_alias(
                            blocks, block_count, &symbols, &symbol_count,
                            &symbol_capacity) != 0) {
                        free(fn_symbol);
                        free(fn_name);
                        free(fn_c_return_type);
                        cg_free_field_type(&fn_return_type);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    function_output = (FunctionOutput) {
                        .active = true,
                        .indent = indent,
                        .c_name = fn_symbol,
                        .return_type = fn_c_return_type,
                        .module = &module,
                        .docs = doc_attributes,
                        .use_source = attribute_private,
                        .emit = fn_emit,
                        .params = NULL,
                        .param_variadic = NULL,
                        .param_count = 0,
                    };
                    fn_symbol = NULL;
                    fn_c_return_type = NULL;
                    doc_attributes = (DocAttributes) {0};
                    free(fn_name);
                    cg_free_field_type(&fn_return_type);
                    reset_compiler_attributes(&attribute_count, &attribute_noscope,
                                   &attribute_public, &attribute_private,
                                   &attribute_extern, &attribute_opaque,
                                   &attribute_internal, &attribute_define,
                                   &attribute_mutable, &attribute_expand,
                                   &attribute_pointer, &attribute_initializer);
                    continue;
                }
            }

            if (!cg_parse_type(text, &name, &expression, &expression_length,
                            &alias_reference, &expr_args, &expr_arg_count)) {
                cg_set_error(error, error_size,
                          "line %zu: expected package, module, scope, let, fn, "
                          "if, or type",
                          line_number);
                goto done;
            }
            {
                bool use_define = if_chain_define_mode(
                    if_frames, if_depth, indent, attribute_define);

            if (!module.relative_header) {
                free(name);
                free(alias_reference);
                cg_free_expr_args(expr_args, expr_arg_count);
                cg_set_error(error, error_size,
                          "line %zu: type must be inside a module",
                          line_number);
                goto done;
            }
            if (attribute_mutable) {
                free(name);
                free(alias_reference);
                cg_free_expr_args(expr_args, expr_arg_count);
                cg_set_error(error, error_size,
                          "line %zu: @mutable does not apply to type",
                          line_number);
                goto done;
            }
            if (!block_count ||
                indent != module_body_indent(blocks, block_count, if_frames,
                                             if_depth) + 4) {
                free(name);
                free(alias_reference);
                cg_free_expr_args(expr_args, expr_arg_count);
                cg_set_error(error, error_size,
                          "line %zu: invalid type indentation", line_number);
                goto done;
            }
            if (cg_note_primary_export(blocks, block_count, name, line_number,
                                       error, error_size) != 0) {
                free(name);
                free(alias_reference);
                cg_free_expr_args(expr_args, expr_arg_count);
                goto done;
            }
            if (attribute_public && attribute_private) {
                free(name);
                free(alias_reference);
                cg_free_expr_args(expr_args, expr_arg_count);
                cg_set_error(error, error_size,
                          "line %zu: @public and @private are incompatible",
                          line_number);
                goto done;
            }
            if (attribute_extern || attribute_opaque) {
                free(name);
                free(alias_reference);
                cg_free_expr_args(expr_args, expr_arg_count);
                cg_set_error(error, error_size,
                          "line %zu: @extern and @opaque do not apply to type",
                          line_number);
                goto done;
            }
            if (attribute_internal &&
                (attribute_public || attribute_private || attribute_define)) {
                free(name);
                free(alias_reference);
                cg_free_expr_args(expr_args, expr_arg_count);
                cg_set_error(error, error_size,
                          "line %zu: @internal is incompatible with visibility "
                          "or @define attributes",
                          line_number);
                goto done;
            }
            symbol = cg_build_export_symbol(blocks, block_count, name,
                                            !use_define);
            if (!symbol) {
                free(name);
                free(alias_reference);
                cg_free_expr_args(expr_args, expr_arg_count);
                cg_set_error(error, error_size, "out of memory");
                goto done;
            }
            {
                const char *alias_type = NULL;
                char *resolved_expression = expression
                                                ? (char *) expression : NULL;
                Symbol *referenced_symbol = NULL;

                if (alias_reference) {
                    referenced_symbol = cg_find_symbol(
                        symbols, symbol_count, alias_reference);
                    if (!referenced_symbol) {
                        free(symbol);
                        free(name);
                        cg_set_error(error, error_size,
                                  "line %zu: unknown type reference: %s",
                                  line_number, alias_reference);
                        free(alias_reference);
                        cg_free_expr_args(expr_args, expr_arg_count);
                        goto done;
                    }
                    alias_type = referenced_symbol->c_name;
                    if (attribute_pointer) {
                        FieldType pointer_type = {
                            .name = alias_reference,
                            .is_ptr = true,
                        };

                        resolved_expression = resolve_c_field_type(
                            &pointer_type, symbols, symbol_count, &module,
                            &header_deps, &header_deps_count,
                            &header_deps_capacity, line_number, error,
                            error_size);
                        free(alias_reference);
                        alias_reference = NULL;
                        if (!resolved_expression) {
                            free(symbol);
                            free(name);
                            cg_free_expr_args(expr_args, expr_arg_count);
                            goto done;
                        }
                        alias_type = NULL;
                        expression = resolved_expression;
                        expression_length = strlen(resolved_expression);
                    }
                } else if (attribute_pointer) {
                    free(symbol);
                    free(name);
                    free(alias_reference);
                    cg_free_expr_args(expr_args, expr_arg_count);
                    cg_set_error(error, error_size,
                              "line %zu: @pointer type requires "
                              "`type name as <reference>`",
                              line_number);
                    goto done;
                }
                bool type_emit = cg_should_emit_c(blocks, block_count,
                                                 attribute_internal,
                                                 attribute_public);

                if (flush_includes(&module, &include_attributes, &header_deps,
                                   &header_deps_count, &header_deps_capacity,
                                   type_emit, attribute_private, error,
                                   error_size) != 0) {
                    free(symbol);
                    free(name);
                    free(alias_reference);
                    cg_free_expr_args(expr_args, expr_arg_count);
                    free(resolved_expression);
                    goto done;
                }
                if (type_emit && !cg_compile_analyze_only) {
                if (attribute_private) {
                    if (cg_ensure_module_source(&module, error, error_size) != 0) {
                        free(symbol);
                        free(name);
                        free(alias_reference);
                        cg_free_expr_args(expr_args, expr_arg_count);
                        free(resolved_expression);
                        goto done;
                    }
                    if (module.source_has_declaration) {
                        while (module.pending_blank_lines > 0) {
                            fputc('\n', module.source_file);
                            module.pending_blank_lines--;
                        }
                    } else {
                        module.pending_blank_lines = 0;
                    }
                    if (cg_emit_doc_comment(&module, module.source_file,
                                         &doc_attributes) != 0) {
                        free(symbol);
                        free(name);
                        free(alias_reference);
                        cg_free_expr_args(expr_args, expr_arg_count);
                        free(resolved_expression);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    if (use_define) {
                        fprintf(module.source_file, "#define %s ", symbol);
                        if (alias_type) {
                            fprintf(module.source_file, "%s", alias_type);
                        } else {
                            fprintf(module.source_file, "%.*s",
                                    (int) expression_length, expression);
                        }
                        fputc('\n', module.source_file);
                    } else {
                        fprintf(module.source_file, "typedef ");
                        if (alias_type) {
                            fprintf(module.source_file, "%s", alias_type);
                        } else {
                            fprintf(module.source_file, "%.*s",
                                    (int) expression_length, expression);
                        }
                        fprintf(module.source_file, " %s;\n", symbol);
                    }
                    module.source_has_declaration = true;
                } else {
                    size_t i;

                    if (cg_ensure_module_header(&module, error, error_size) != 0) {
                        free(symbol);
                        free(name);
                        free(alias_reference);
                        cg_free_expr_args(expr_args, expr_arg_count);
                        free(resolved_expression);
                        goto done;
                    }
                    if (module.header_has_declaration) {
                        cg_flush_module_blank_lines(&module);
                    } else {
                        module.pending_blank_lines = 0;
                    }
                    if (referenced_symbol &&
                        !referenced_symbol->is_internal &&
                        cg_module_require_include(
                            &module, referenced_symbol->header,
                            &header_deps, &header_deps_count,
                            &header_deps_capacity) != 0) {
                        free(symbol);
                        free(name);
                        free(alias_reference);
                        cg_free_expr_args(expr_args, expr_arg_count);
                        free(resolved_expression);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    for (i = 0; i < expr_arg_count; i++) {
                        Symbol *arg_symbol = cg_find_symbol(
                            symbols, symbol_count, expr_args[i].dsl_name);

                        if (arg_symbol && !arg_symbol->is_internal &&
                            cg_module_require_include(
                                &module, arg_symbol->header,
                                &header_deps, &header_deps_count,
                                &header_deps_capacity) != 0) {
                            free(symbol);
                            free(name);
                            free(alias_reference);
                            cg_free_expr_args(expr_args, expr_arg_count);
                            free(resolved_expression);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                    }
                    if (cg_emit_doc_comment(&module, NULL,
                                         &doc_attributes) != 0) {
                        free(symbol);
                        free(name);
                        free(alias_reference);
                        cg_free_expr_args(expr_args, expr_arg_count);
                        free(resolved_expression);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    if (use_define) {
                        if (cg_module_body_printf(&module, "#define %s ", symbol) != 0 ||
                            (alias_type ?
                             cg_module_body_printf(&module, "%s", alias_type) :
                             cg_module_body_append(&module, expression,
                                                expression_length)) != 0 ||
                            cg_module_body_printf(&module, "\n") != 0) {
                            free(symbol);
                            free(name);
                            free(alias_reference);
                            cg_free_expr_args(expr_args, expr_arg_count);
                            free(resolved_expression);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                    } else {
                        if (cg_module_body_printf(&module, "typedef ") != 0 ||
                            (alias_type ?
                             cg_module_body_printf(&module, "%s", alias_type) :
                             cg_module_body_append(&module, expression,
                                                expression_length)) != 0 ||
                            cg_module_body_printf(&module, " %s;\n", symbol) != 0) {
                            free(symbol);
                            free(name);
                            free(alias_reference);
                            cg_free_expr_args(expr_args, expr_arg_count);
                            free(resolved_expression);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                    }
                    module.header_has_declaration = true;
                }
                }
                {
                    char *stored_expr = NULL;
                    char *export_name = cg_resolve_export_name(
                        blocks, block_count, name);
                    char *dsl_name = export_name ?
                        cg_build_dsl_name(blocks, block_count, export_name) :
                        NULL;

                    if (expression && !alias_reference) {
                        stored_expr = cg_copy_text(expression, expression_length);
                        if (!stored_expr) {
                            free(export_name);
                            free(dsl_name);
                            free(symbol);
                            free(name);
                            free(alias_reference);
                            cg_free_expr_args(expr_args, expr_arg_count);
                            free(resolved_expression);
                            cg_set_error(error, error_size, "out of memory");
                            goto done;
                        }
                    }
                    if (!export_name || !dsl_name ||
                        cg_add_symbol_ex(&symbols, &symbol_count, &symbol_capacity,
                                         dsl_name, symbol, module.relative_header,
                                         stored_expr, use_define,
                                         !cg_should_emit_c(blocks, block_count,
                                                        attribute_internal,
                                                        attribute_public),
                                         false, SYMBOL_KIND_TYPE,
                                         alias_reference ?
                                             strdup(alias_reference) : NULL) !=
                            0) {
                        free(stored_expr);
                        free(export_name);
                        free(dsl_name);
                        free(symbol);
                        free(name);
                        free(alias_reference);
                        cg_free_expr_args(expr_args, expr_arg_count);
                        free(resolved_expression);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    free(stored_expr);
                    free(export_name);
                    if (cg_is_module_export_name(name) &&
                        cg_add_primary_export_alias(
                            blocks, block_count, &symbols, &symbol_count,
                            &symbol_capacity) != 0) {
                        free(symbol);
                        free(name);
                        free(alias_reference);
                        cg_free_expr_args(expr_args, expr_arg_count);
                        free(resolved_expression);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    free(resolved_expression);
                }
            }
            free(symbol);
            free(name);
            free(alias_reference);
            cg_free_expr_args(expr_args, expr_arg_count);
            reset_compiler_attributes(&attribute_count, &attribute_noscope,
                                   &attribute_public, &attribute_private,
                                   &attribute_extern, &attribute_opaque,
                                   &attribute_internal, &attribute_define,
                                   &attribute_mutable, &attribute_expand,
                                   &attribute_pointer, &attribute_initializer);
            cg_clear_doc_attributes(&doc_attributes);
            continue;
            }
        }

        if ((!block_count && indent != 0) ||
            (block_count && indent != blocks[block_count - 1].indent + 4)) {
            free(name);
            cg_set_error(error, error_size,
                      "line %zu: invalid block indentation", line_number);
            goto done;
        }
        if (attribute_mutable) {
            free(name);
            cg_set_error(error, error_size,
                      "line %zu: @mutable applies only to struct, field, or let",
                      line_number);
            goto done;
        }
        if (attribute_expand) {
            free(name);
            cg_set_error(error, error_size,
                      "line %zu: @expand applies only to type or field",
                      line_number);
            goto done;
        }
        if (attribute_pointer) {
            free(name);
            cg_set_error(error, error_size,
                      "line %zu: @pointer applies only to type, field, let, "
                      "param, or fn return type",
                      line_number);
            goto done;
        }
        if (kind == BLOCK_MODULE) {
            for (size_t i = 0; i < block_count; i++) {
                if (blocks[i].kind == BLOCK_MODULE) {
                    free(name);
                    cg_set_error(error, error_size,
                              "line %zu: modules cannot be nested",
                              line_number);
                    goto done;
                }
            }
        } else if (kind == BLOCK_SCOPE && doc_attributes.entry_count > 0) {
            free(name);
            cg_set_error(error, error_size,
                      "line %zu: @doc does not apply to scope",
                      line_number);
            goto done;
        }
        if (include_attributes.count > 0 &&
            (kind == BLOCK_PACKAGE || kind == BLOCK_SCOPE)) {
            free(name);
            cg_clear_include_attributes(&include_attributes);
            cg_set_error(error, error_size,
                      "line %zu: @include applies only to module or declarations",
                      line_number);
            goto done;
        }
        if ((kind == BLOCK_PACKAGE || kind == BLOCK_MODULE) &&
            attribute_internal &&
            (attribute_public || attribute_private || attribute_define)) {
            free(name);
            cg_set_error(error, error_size,
                      "line %zu: @internal is incompatible with visibility "
                      "or @define attributes",
                      line_number);
            goto done;
        }
        if (clean_output && !cg_compile_analyze_only &&
            kind == BLOCK_PACKAGE && block_count == 0) {
            bool already_cleaned = false;

            for (size_t i = 0; i < cleaned_package_count; i++) {
                if (strcmp(cleaned_packages[i], name) == 0) {
                    already_cleaned = true;
                    break;
                }
            }
            if (!already_cleaned) {
                char *remembered_name;

                if (cgem_clean_output_package(include_path, source_path, name,
                                              error, error_size) != 0) {
                    free(name);
                    goto done;
                }
                remembered_name = strdup(name);
                if (!remembered_name) {
                    free(name);
                    cg_set_error(error, error_size, "out of memory");
                    goto done;
                }
                if (cleaned_package_count == cleaned_package_capacity) {
                    size_t capacity = cleaned_package_capacity
                                          ? cleaned_package_capacity * 2
                                          : 4;
                    char **grown = realloc(cleaned_packages,
                                           capacity * sizeof(*grown));

                    if (!grown) {
                        free(remembered_name);
                        free(name);
                        cg_set_error(error, error_size, "out of memory");
                        goto done;
                    }
                    cleaned_packages = grown;
                    cleaned_package_capacity = capacity;
                }
                cleaned_packages[cleaned_package_count++] = remembered_name;
            }
        }
        if (block_count == block_capacity) {
            size_t capacity = block_capacity ? block_capacity * 2 : 8;
            Block *grown = realloc(blocks, capacity * sizeof(*blocks));
            if (!grown) {
                free(name);
                cg_set_error(error, error_size, "out of memory");
                goto done;
            }
            blocks = grown;
            block_capacity = capacity;
        }
        blocks[block_count] = (Block) {
            kind, indent, name, attribute_noscope,
            attribute_internal || cg_blocks_are_internal(blocks, block_count),
            false, {0}, NULL
        };
        if (kind == BLOCK_PACKAGE) {
            char *directory = cg_build_package_path(include_path, blocks,
                                                 block_count + 1);

            if (!directory) {
                free(name);
                cg_set_error(error, error_size, "out of memory");
                goto done;
            }
            blocks[block_count].docs = doc_attributes;
            blocks[block_count].readme_path =
                cg_join_path(directory, "README.md");
            doc_attributes = (DocAttributes) {0};
            free(directory);
            if (!blocks[block_count].readme_path) {
                free(name);
                cg_set_error(error, error_size, "out of memory");
                goto done;
            }
        }
        block_count++;
        if (kind == BLOCK_MODULE &&
            cg_open_module(&module, include_path, source_path,
                        blocks, block_count, error, error_size) != 0) {
            goto done;
        }
        if (kind == BLOCK_MODULE) {
            module.module_docs = doc_attributes;
            doc_attributes = (DocAttributes) {0};
            if (flush_includes(&module, &include_attributes, &header_deps,
                               &header_deps_count, &header_deps_capacity,
                               true, blocks[block_count - 1].internal,
                               error, error_size) != 0) {
                goto done;
            }
        } else if (kind != BLOCK_PACKAGE) {
            cg_clear_doc_attributes(&doc_attributes);
        }
        reset_compiler_attributes(&attribute_count, &attribute_noscope,
                               &attribute_public, &attribute_private,
                               &attribute_extern, &attribute_opaque,
                               &attribute_internal, &attribute_define,
                               &attribute_mutable, &attribute_expand,
                               &attribute_pointer, &attribute_initializer);
    }
    if (ferror(input)) {
        cg_set_error(error, error_size, "failed to read input");
        goto done;
    }
    if (block_attribute != BLOCK_ATTR_NONE) {
        if (block_attribute_values == 0) {
            cg_set_error(error, error_size,
                      "line %zu: @%s: requires at least one string",
                      line_number,
                      block_attribute == BLOCK_ATTR_DOC ? "doc" : "include");
            goto done;
        }
    }
    if (attribute_count) {
        cg_set_error(error, error_size,
                  "attributes at end of input have no object");
        goto done;
    }
    if (attribute_mutable && struct_output.active) {
        cg_set_error(error, error_size,
                  "@mutable at end of input has no field or struct fn");
        goto done;
    }
    if (struct_output.field_expand) {
        cg_set_error(error, error_size,
                  "@expand at end of input has no field");
        goto done;
    }
    if (struct_output.field_pointer) {
        cg_set_error(error, error_size,
                  "@pointer at end of input has no field");
        goto done;
    }
    if (function_output.active &&
        finalize_function_body(&function_output, symbols, symbol_count, &module,
                               &header_deps, &header_deps_count,
                               &header_deps_capacity, line_number, error,
                               error_size) != 0) {
        goto done;
    }
    if (cg_close_function(
            &function_output,
            struct_output.active && function_output.is_method ? &struct_output
                                                             : NULL,
            error, error_size) != 0) {
        goto done;
    }
    if (struct_output.active) {
        if (finalize_struct(&struct_output, &struct_templates,
                              &struct_template_count, &struct_template_capacity,
                              &symbols, &symbol_count, &symbol_capacity, blocks,
                              block_count, error, error_size) != 0) {
            goto done;
        }
    }
    if (cg_close_struct(&struct_output, error, error_size) != 0) {
        cg_set_error(error, error_size, "out of memory");
        goto done;
    }
    if (if_depth > 0) {
        cg_set_error(error, error_size, "unclosed if");
        goto done;
    }
    result = 0;

done:
    reset_compile_constructors();
    cg_close_enum(&enum_output);
    cg_clear_function(&function_output);
    cg_clear_struct(&struct_output);
    for (size_t i = 0; i < struct_template_count; i++) {
        free_struct_template(&struct_templates[i]);
    }
    free(struct_templates);
    cg_free_names(cleaned_packages, cleaned_package_count);
    free(if_frames);
    if (cg_close_module(&module, header_deps, header_deps_count,
                     result == 0 ? format_style : NULL,
                     error, error_size) != 0 && result == 0) {
        result = -1;
    }
    while (block_count) {
        if (cg_pop_block(blocks, &block_count, &module, header_deps,
                      header_deps_count,
                      result == 0 ? format_style : NULL, error,
                      error_size) != 0 && result == 0) {
            result = -1;
        }
    }
    free(blocks);
    if (cg_analyze_semantic_out) {
        cgem_semantic_adopt_symbols(cg_analyze_semantic_out, symbols,
                                    symbol_count);
        symbol_count = 0;
        symbols = NULL;
    }
    for (size_t i = 0; i < symbol_count; i++) {
        free(symbols[i].dsl_name);
        free(symbols[i].c_name);
        free(symbols[i].header);
        free(symbols[i].c_expr);
        free(symbols[i].type_dsl_name);
    }
    free(symbols);
    cg_free_header_deps(header_deps, header_deps_count);
    cg_clear_doc_attributes(&doc_attributes);
    cg_clear_include_attributes(&include_attributes);
    if (warning && warning_size > 0) {
        warning[0] = '\0';
    }
    cg_diagnostic_format_legacy(diagnostics, warning, warning_size,
                                error, error_size);
    cg_diagnostic_set_active(NULL);
    if (!diagnostics_out) {
        cg_diagnostic_free(&local_diagnostics);
    }
    free(line);
    return result;
}

int cgem_analyze(FILE *input, const char *compiler,
                 DiagnosticList *diagnostics_out, CgemSemantic *semantic_out)
{
    char error[512];
    char warning[1024];
    int result;

    if (!input || !compiler || !diagnostics_out || !semantic_out) {
        return -1;
    }
    cg_compile_analyze_only = true;
    cg_analyze_semantic_out = semantic_out;
    result = cgem_compile(input, ".", ".", NULL, compiler, false, warning,
                          sizeof(warning), error, sizeof(error),
                          diagnostics_out);
    cg_compile_analyze_only = false;
    cg_analyze_semantic_out = NULL;
    if (result != 0 && diagnostics_out->count > 0) {
        return 0;
    }
    return result;
}
