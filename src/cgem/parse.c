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

bool cg_parse_named_block(const char *text, const char *keyword,
                              char **name)
{
    size_t keyword_length = strlen(keyword);
    size_t at = keyword_length;
    size_t start;
    size_t end;

    if (strncmp(text, keyword, keyword_length) != 0 || text[at] != ' ') {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at++;
    while (cg_name_char((unsigned char) text[at])) {
        at++;
    }
    end = at;
    if (text[at++] != ':') {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] != '\0') {
        return false;
    }
    *name = cg_copy_text(text + start, end - start);
    return *name != NULL;
}

bool cg_parse_block(const char *text, const char *keyword)
{
    size_t keyword_length = strlen(keyword);
    size_t at = keyword_length;

    if (strncmp(text, keyword, keyword_length) != 0 ||
        text[at] != ':') {
        return false;
    }
    at++;
    while (text[at] == ' ') {
        at++;
    }
    return text[at] == '\0';
}

static bool cg_parse_conditional_block(const char *text, const char *keyword,
                                       char **condition)
{
    size_t keyword_length = strlen(keyword);
    size_t at = keyword_length;
    size_t start;
    size_t end;

    *condition = NULL;
    if (strncmp(text, keyword, keyword_length) != 0 || text[at] != ' ') {
        return false;
    }
    at++;
    while (text[at] == ' ') {
        at++;
    }
    start = at;
    while (text[at] != '\0' && text[at] != ':') {
        at++;
    }
    if (text[at] != ':') {
        return false;
    }
    end = at;
    while (end > start && text[end - 1] == ' ') {
        end--;
    }
    if (end == start) {
        return false;
    }
    at++;
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] != '\0') {
        return false;
    }
    *condition = cg_copy_text(text + start, end - start);
    return *condition != NULL;
}

bool cg_parse_if_block(const char *text, char **condition)
{
    return cg_parse_conditional_block(text, "if", condition);
}

bool cg_parse_elif_block(const char *text, char **condition)
{
    return cg_parse_conditional_block(text, "elif", condition);
}

bool cg_parse_else_block(const char *text)
{
    return cg_parse_block(text, "else");
}

bool cg_parse_struct_module(const char *text)
{
    char *name = NULL;
    bool ok;

    if (!cg_parse_named_block(text, "struct", &name)) {
        return false;
    }
    ok = cg_is_module_export_name(name);
    free(name);
    return ok;
}

void cg_free_expr_args(ExprArg *args, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free(args[i].dsl_name);
    }
    free(args);
}

bool cg_parse_type(const char *text, char **name,
                       const char **expression, size_t *expression_length,
                       char **reference, ExprArg **expr_args,
                       size_t *expr_arg_count)
{
    size_t at = strlen("type");
    size_t start;

    if (expr_args) {
        *expr_args = NULL;
        *expr_arg_count = 0;
    }
    if (expression) {
        *expression = NULL;
    }
    if (expression_length) {
        *expression_length = 0;
    }
    if (reference) {
        *reference = NULL;
    }
    *name = NULL;
    if (strncmp(text, "type", 4) != 0) {
        return false;
    }
    at = 4;
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at++;
    while (cg_name_char((unsigned char) text[at])) {
        at++;
    }
    *name = cg_copy_text(text + start, at - start);
    if (!*name) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (strncmp(text + at, "as ", 3) != 0) {
        goto fail;
    }
    at += 3;
    while (text[at] == ' ') {
        at++;
    }
    {
        size_t reference_start = at;

        if (!cg_name_start((unsigned char) text[at])) goto fail;
        while (cg_name_char((unsigned char) text[at]) || text[at] == '.') at++;
        if (text[at] != '\0') goto fail;
        if (reference) {
            *reference = cg_copy_text(text + reference_start, at - reference_start);
            if (!*reference) goto fail;
        }
    }
    return true;

fail:
    free(*name);
    if (reference) {
        free(*reference);
        *reference = NULL;
    }
    if (expr_args) {
        cg_free_expr_args(*expr_args, *expr_arg_count);
        *expr_args = NULL;
        *expr_arg_count = 0;
    }
    *name = NULL;
    if (reference) {
        *reference = NULL;
    }
    return false;
}

bool cg_parse_enum(const char *text, char **name, char **base)
{
    size_t at = strlen("enum");
    size_t start;

    if (strncmp(text, "enum", 4) != 0) {
        return false;
    }
    at = 4;
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at++;
    while (cg_name_char((unsigned char) text[at])) {
        at++;
    }
    *name = cg_copy_text(text + start, at - start);
    if (!*name) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (strncmp(text + at, "as ", 3) != 0) {
        goto fail;
    }
    at += 3;
    while (text[at] == ' ') {
        at++;
    }
    start = at;
    if (!cg_name_start((unsigned char) text[at])) goto fail;
    while (cg_name_char((unsigned char) text[at]) || text[at] == '.') at++;
    *base = cg_copy_text(text + start, at - start);
    if (!*base) goto fail;
    if (text[at++] != ':') goto fail;
    while (text[at] == ' ') at++;
    if (text[at] != '\0') goto fail;
    return true;

fail:
    free(*name);
    free(*base);
    *name = NULL;
    *base = NULL;
    return false;
}

bool cg_parse_case(const char *text, char **name, char **value)
{
    size_t at = strlen("case");
    size_t start;
    size_t end;
    size_t value_length;

    if (value) {
        *value = NULL;
    }
    if (strncmp(text, "case ", at + 1) != 0) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at++;
    while (cg_name_char((unsigned char) text[at])) {
        at++;
    }
    end = at;
    *name = cg_copy_text(text + start, end - start);
    if (!*name) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] == '\0') {
        return true;
    }
    if (text[at] != '=') {
        goto fail;
    }
    at++;
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] == '\0') {
        goto fail;
    }
    value_length = strlen(text + at);
    while (value_length > 0 && text[at + value_length - 1] == ' ') {
        value_length--;
    }
    if (value) {
        *value = cg_copy_text(text + at, value_length);
        if (!*value) {
            goto fail;
        }
    }
    return true;

fail:
    free(*name);
    *name = NULL;
    if (value) {
        free(*value);
        *value = NULL;
    }
    return false;
}

bool cg_parse_simple_name(const char *text, const char *keyword,
                              char **name)
{
    size_t at = strlen(keyword);
    size_t start;
    size_t end;

    if (strncmp(text, keyword, at) != 0 || text[at] != ' ') {
        return false;
    }
    while (text[at] == ' ') at++;
    if (!cg_name_start((unsigned char) text[at])) return false;
    start = at++;
    while (cg_name_char((unsigned char) text[at])) at++;
    end = at;
    while (text[at] == ' ') at++;
    if (text[at] != '\0') return false;
    *name = cg_copy_text(text + start, end - start);
    return *name != NULL;
}

bool cg_parse_param(const char *text, char **name, FieldType *type,
                    bool *is_meta, bool *is_variadic)
{
    size_t at = strlen("param");
    size_t start;

    *name = NULL;
    *type = (FieldType) {0};
    *is_meta = false;
    *is_variadic = false;
    if (strncmp(text, "param", at) != 0 || text[at] != ' ') {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at++;
    while (cg_name_char((unsigned char) text[at])) {
        at++;
    }
    *name = cg_copy_text(text + start, at - start);
    if (!*name) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] == '\0') {
        *is_meta = true;
        return true;
    }
    if (strncmp(text + at, "as ", 3) != 0) {
        goto fail;
    }
    at += 3;
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] == '.' && text[at + 1] == '.' && text[at + 2] == '.') {
        at += 3;
        *is_meta = true;
        *is_variadic = true;
    } else if (strncmp(text + at, "type", 4) == 0 &&
               !cg_name_char((unsigned char) text[at + 4])) {
        goto fail;
    } else {
        if (!cg_parse_as_field_type(text, &at, type)) {
            goto fail;
        }
    }
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] != '\0') {
        goto fail;
    }
    return true;

fail:
    free(*name);
    cg_free_field_type(type);
    *name = NULL;
    *is_meta = false;
    *is_variadic = false;
    return false;
}

bool cg_parse_require_attribute(const char *text, ParamRequire *require)
{
    size_t at = 0;

    *require = PARAM_REQUIRE_ANY;
    if (text[at++] != '@') {
        return false;
    }
    if (strncmp(text + at, "require", 7) != 0) {
        return false;
    }
    at += 7;
    while (text[at] == ' ') {
        at++;
    }
    if (strncmp(text + at, "type or value", 13) == 0) {
        at += 13;
        *require = PARAM_REQUIRE_TYPE_OR_VALUE;
    } else if (strncmp(text + at, "type", 4) == 0 &&
               (text[at + 4] == '\0' || text[at + 4] == ' ')) {
        at += 4;
        *require = PARAM_REQUIRE_TYPE;
    } else if (strncmp(text + at, "value", 5) == 0 &&
               (text[at + 5] == '\0' || text[at + 5] == ' ')) {
        at += 5;
        *require = PARAM_REQUIRE_VALUE;
    } else {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    return text[at] == '\0';
}

void cg_free_field_type(FieldType *type)
{
    free(type->name);
    *type = (FieldType) {0};
}

bool cg_parse_as_field_type(const char *text, size_t *at, FieldType *type)
{
    size_t start;

    *type = (FieldType) {0};
    while (text[*at] == ' ') {
        (*at)++;
    }
    if (strncmp(text + *at, "param ", 6) == 0) {
        (*at) += 6;
        while (text[*at] == ' ') {
            (*at)++;
        }
        if (!cg_name_start((unsigned char) text[*at])) {
            return false;
        }
        start = *at;
        (*at)++;
        while (cg_name_char((unsigned char) text[*at])) {
            (*at)++;
        }
        type->name = cg_copy_text(text + start, *at - start);
        type->is_param_ref = type->name != NULL;
        return type->name != NULL;
    }
    if (!cg_name_start((unsigned char) text[*at])) {
        return false;
    }
    start = *at;
    (*at)++;
    while (cg_name_char((unsigned char) text[*at]) || text[*at] == '.') {
        (*at)++;
    }
    type->name = cg_copy_text(text + start, *at - start);
    return type->name != NULL;
}

bool cg_parse_let(const char *text, char **name, FieldType *type, char **value)
{
    size_t at = strlen("let");
    size_t start;
    size_t value_length;

    *name = NULL;
    *type = (FieldType) {0};
    if (value) {
        *value = NULL;
    }
    if (strncmp(text, "let", 3) != 0) {
        return false;
    }
    at = 3;
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at++;
    while (cg_name_char((unsigned char) text[at])) {
        at++;
    }
    *name = cg_copy_text(text + start, at - start);
    if (!*name) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (strncmp(text + at, "as ", 3) == 0) {
        at += 3;
        if (!cg_parse_as_field_type(text, &at, type)) {
            goto fail;
        }
        while (text[at] == ' ') {
            at++;
        }
    }
    if (text[at] == '\0') {
        return true;
    }
    if (text[at] != '=') goto fail;
    at++;
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] == '\0') {
        goto fail;
    }
    value_length = strlen(text + at);
    while (value_length > 0 && text[at + value_length - 1] == ' ') {
        value_length--;
    }
    if (value) {
        *value = cg_copy_text(text + at, value_length);
        if (!*value) {
            goto fail;
        }
    }
    return true;

fail:
    free(*name);
    cg_free_field_type(type);
    if (value) {
        free(*value);
        *value = NULL;
    }
    *name = NULL;
    return false;
}

bool cg_parse_fn(const char *text, char **name, FieldType *return_type)
{
    size_t at = strlen("fn");
    size_t start;

    *name = NULL;
    *return_type = (FieldType) {0};
    if (strncmp(text, "fn", 2) != 0 || text[at] != ' ') {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at++;
    while (cg_name_char((unsigned char) text[at])) {
        at++;
    }
    *name = cg_copy_text(text + start, at - start);
    if (!*name) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (strncmp(text + at, "as ", 3) == 0) {
        at += 3;
        if (!cg_parse_as_field_type(text, &at, return_type)) {
            goto fail;
        }
        while (text[at] == ' ') {
            at++;
        }
    }
    if (text[at++] != ':') {
        goto fail;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] != '\0') {
        goto fail;
    }
    return true;

fail:
    free(*name);
    cg_free_field_type(return_type);
    *name = NULL;
    return false;
}

static const char *find_return_cast(const char *text, size_t length)
{
    const char *candidate = NULL;
    size_t depth = 0;
    bool in_string = false;
    bool escaped = false;

    for (size_t i = 0; i + 4 < length; i++) {
        if (in_string) {
            escaped = text[i] == '\\' && !escaped;
            if (text[i] == '"' && !escaped) {
                in_string = false;
            } else if (text[i] != '\\') {
                escaped = false;
            }
            continue;
        }
        if (text[i] == '"') {
            in_string = true;
            escaped = false;
            continue;
        }
        if (text[i] == '(') {
            depth++;
            continue;
        }
        if (text[i] == ')' && depth > 0) {
            depth--;
            continue;
        }
        if (depth == 0 && text[i] == ' ' && text[i + 1] == 'a' &&
            text[i + 2] == 's' &&
            text[i + 3] == ' ') {
            candidate = text + i;
        }
    }
    return candidate;
}

bool cg_parse_return(const char *text, char **expression, FieldType *cast_type)
{
    size_t at = strlen("return");
    size_t start;
    size_t length;
    const char *cast;

    *expression = NULL;
    *cast_type = (FieldType) {0};
    if (strncmp(text, "return", 6) != 0) {
        return false;
    }
    if (text[at] != '\0' && text[at] != ' ') {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] == '\0') {
        return true;
    }
    start = at;
    length = strlen(text + start);
    while (length > 0 && text[start + length - 1] == ' ') {
        length--;
    }
    cast = find_return_cast(text + start, length);
    if (cast) {
        size_t cast_at = (size_t) (cast - text) + 4;
        length = (size_t) (cast - (text + start));
        while (length > 0 && text[start + length - 1] == ' ') {
            length--;
        }
        if (length == 0) {
            return false;
        }
        if (!cg_parse_as_field_type(text, &cast_at, cast_type)) {
            return false;
        }
        while (text[cast_at] == ' ') {
            cast_at++;
        }
        if (text[cast_at] != '\0') {
            cg_free_field_type(cast_type);
            return false;
        }
    }
    *expression = cg_copy_text(text + start, length);
    return *expression != NULL;
}

bool cg_parse_dsl_reference(const char *text, char **name)
{
    size_t at = 0;
    size_t start;
    size_t end;

    *name = NULL;
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at++;
    while (cg_name_char((unsigned char) text[at]) || text[at] == '.') {
        at++;
    }
    end = at;
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] != '\0') {
        return false;
    }
    *name = cg_copy_text(text + start, end - start);
    return *name != NULL;
}

void cg_free_cstr_array(char **items, size_t count)
{
    if (!items) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);
}

bool cg_parse_paren_call(const char *expression, char **callee, char ***args,
                         size_t *arg_count)
{
    size_t at = 0;
    size_t start;
    size_t name_end;
    size_t end = strlen(expression);
    size_t depth = 0;
    char quote = '\0';
    bool escaped = false;
    const char *part;
    char **collected = NULL;
    size_t collected_count = 0;
    size_t collected_capacity = 0;

    *callee = NULL;
    *args = NULL;
    *arg_count = 0;
    while (at < end && expression[at] == ' ') {
        at++;
    }
    start = at;
    if (at >= end || !cg_name_start((unsigned char) expression[at])) {
        return false;
    }
    at++;
    while (at < end &&
           (cg_name_char((unsigned char) expression[at]) ||
            expression[at] == '.')) {
        at++;
    }
    name_end = at;
    while (at < end && expression[at] == ' ') {
        at++;
    }
    if (at >= end || expression[at] != '(') {
        return false;
    }
    while (end > at && expression[end - 1] == ' ') {
        end--;
    }
    if (end <= at + 1 || expression[end - 1] != ')') {
        return false;
    }
    *callee = cg_copy_text(expression + start, name_end - start);
    if (!*callee) {
        return false;
    }
    part = expression + at + 1;
    for (at = at + 1; at < end; at++) {
        char ch = expression[at];

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
            if (ch == ')' && at == end - 1 && depth == 0) {
                /* closing paren of the call */
            } else if (depth == 0) {
                goto fail;
            } else {
                depth--;
            }
        }
        if ((ch == ',' && depth == 0) ||
            (ch == ')' && at == end - 1 && depth == 0)) {
            const char *part_end = expression + at;
            char *argument;

            while (part < part_end && *part == ' ') {
                part++;
            }
            while (part_end > part && part_end[-1] == ' ') {
                part_end--;
            }
            if (part == part_end) {
                goto fail;
            }
            argument = cg_copy_text(part, (size_t) (part_end - part));
            if (!argument) {
                goto fail;
            }
            if (collected_count == collected_capacity) {
                size_t next = collected_capacity ? collected_capacity * 2 : 4;
                char **grown = realloc(collected, next * sizeof(*collected));

                if (!grown) {
                    free(argument);
                    goto fail;
                }
                collected = grown;
                collected_capacity = next;
            }
            collected[collected_count++] = argument;
            part = expression + at + 1;
        }
    }
    if (quote || depth != 0) {
        goto fail;
    }
    *args = collected;
    *arg_count = collected_count;
    return true;

fail:
    free(*callee);
    *callee = NULL;
    cg_free_cstr_array(collected, collected_count);
    return false;
}

bool cg_parse_field(const char *text, char **name, FieldType *type)
{
    size_t at = strlen("field");

    *name = NULL;
    *type = (FieldType) {0};
    if (strncmp(text, "field ", at + 1) != 0) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    {
        size_t start = at++;

        while (cg_name_char((unsigned char) text[at])) {
            at++;
        }
        *name = cg_copy_text(text + start, at - start);
    }
    if (!*name) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (strncmp(text + at, "as ", 3) != 0) {
        goto fail;
    }
    at += 3;
    if (!cg_parse_as_field_type(text, &at, type)) {
        goto fail;
    }
    while (text[at] == ' ') {
        at++;
    }
    if (text[at] != '\0') {
        goto fail;
    }
    return true;

fail:
    free(*name);
    cg_free_field_type(type);
    *name = NULL;
    return false;
}

bool cg_name_in_list(char **names, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

bool cg_parse_escaped_string(const char *text, size_t *at, char **value)
{
    size_t value_length = 0;
    char *copy;

    if (text[*at] != '"') {
        return false;
    }
    (*at)++;
    copy = malloc(strlen(text + *at) + 1);
    if (!copy) {
        return false;
    }
    while (text[*at] && text[*at] != '"') {
        if (text[*at] == '\\') {
            (*at)++;
            if (!text[*at]) {
                free(copy);
                return false;
            }
            if (text[*at] == 'n') {
                copy[value_length++] = '\n';
            } else if (text[*at] == 't') {
                copy[value_length++] = '\t';
            } else if (text[*at] == 'r') {
                copy[value_length++] = '\r';
            } else {
                copy[value_length++] = text[*at];
            }
            (*at)++;
        } else {
            copy[value_length++] = text[(*at)++];
        }
    }
    if (text[*at] != '"') {
        free(copy);
        return false;
    }
    (*at)++;
    copy[value_length] = '\0';
    *value = copy;
    return true;
}

static bool block_attribute_kind_for_name(const char *name, size_t name_length,
                                          BlockAttributeKind *kind)
{
    if (name_length == strlen("doc") &&
        memcmp(name, "doc", name_length) == 0) {
        *kind = BLOCK_ATTR_DOC;
        return true;
    }
    if (name_length == strlen("include") &&
        memcmp(name, "include", name_length) == 0) {
        *kind = BLOCK_ATTR_INCLUDE;
        return true;
    }
    return false;
}

static bool parse_attribute_paren_args(const char *text, size_t open_at,
                                       size_t end, char ***args,
                                       size_t *arg_count)
{
    size_t at = open_at + 1;
    size_t depth = 0;
    char quote = '\0';
    bool escaped = false;
    const char *part = text + at;
    char **collected = NULL;
    size_t collected_count = 0;
    size_t collected_capacity = 0;

    *args = NULL;
    *arg_count = 0;
    if (open_at >= end || text[open_at] != '(') {
        return false;
    }
    for (; at < end; at++) {
        char ch = text[at];

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
            if (ch == ')' && at == end - 1 && depth == 0) {
                /* closing paren of the attribute call */
            } else if (depth == 0) {
                goto fail;
            } else {
                depth--;
            }
        }
        if ((ch == ',' && depth == 0) ||
            (ch == ')' && at == end - 1 && depth == 0)) {
            const char *part_end = text + at;
            char *argument;

            while (part < part_end && *part == ' ') {
                part++;
            }
            while (part_end > part && part_end[-1] == ' ') {
                part_end--;
            }
            if (part == part_end) {
                goto fail;
            }
            argument = cg_copy_text(part, (size_t) (part_end - part));
            if (!argument) {
                goto fail;
            }
            if (collected_count == collected_capacity) {
                size_t next = collected_capacity ? collected_capacity * 2 : 4;
                char **grown = realloc(collected, next * sizeof(*collected));

                if (!grown) {
                    free(argument);
                    goto fail;
                }
                collected = grown;
                collected_capacity = next;
            }
            collected[collected_count++] = argument;
            part = text + at + 1;
        }
    }
    if (quote || depth != 0) {
        goto fail;
    }
    *args = collected;
    *arg_count = collected_count;
    return true;

fail:
    for (size_t i = 0; i < collected_count; i++) {
        free(collected[i]);
    }
    free(collected);
    return false;
}

bool cg_parse_attribute_call(const char *text, const char **name,
                             size_t *name_length, char ***args,
                             size_t *arg_count)
{
    size_t at = 0;
    size_t start;
    size_t end = strlen(text);

    *name = NULL;
    *name_length = 0;
    *args = NULL;
    *arg_count = 0;
    if (text[at++] != '@' || !cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at;
    while (cg_name_char((unsigned char) text[at])) {
        at++;
    }
    if (at == start) {
        return false;
    }
    while (at < end && text[at] == ' ') {
        at++;
    }
    if (at >= end || text[at] != '(') {
        return false;
    }
    while (end > at && text[end - 1] == ' ') {
        end--;
    }
    if (end <= at + 1 || text[end - 1] != ')') {
        return false;
    }
    if (!parse_attribute_paren_args(text, at, end, args, arg_count)) {
        return false;
    }
    *name = text + start;
    *name_length = at - start;
    while (text[end] == ' ') {
        end++;
    }
    if (text[end] != '\0') {
        cg_free_cstr_array(*args, *arg_count);
        *args = NULL;
        *arg_count = 0;
        return false;
    }
    return true;
}

bool cg_parse_self_method_call_block_opener(const char *text, char **method_name)
{
    static const char prefix[] = "self.";
    size_t at = 0;
    size_t start;
    size_t end = strlen(text);

    *method_name = NULL;
    while (at < end && text[at] == ' ') {
        at++;
    }
    if (end - at < sizeof(prefix) ||
        memcmp(text + at, prefix, sizeof(prefix) - 1) != 0) {
        return false;
    }
    at += sizeof(prefix) - 1;
    if (!cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at++;
    while (at < end && cg_name_char((unsigned char) text[at])) {
        at++;
    }
    while (at < end && text[at] == ' ') {
        at++;
    }
    if (at >= end || text[at] != ':') {
        return false;
    }
    {
        size_t colon_at = at;

        at++;
        while (at < end && text[at] == ' ') {
            at++;
        }
        if (at != end) {
            return false;
        }
        *method_name = cg_copy_text(text + start, colon_at - start);
    }
    return *method_name != NULL;
}

bool cg_parse_block_attribute_opener(const char *text, BlockAttributeKind *kind)
{
    size_t at = 0;
    size_t name_length;

    *kind = BLOCK_ATTR_NONE;
    if (text[at++] != '@' || !cg_name_start((unsigned char) text[at])) {
        return false;
    }
    name_length = 0;
    while (cg_name_char((unsigned char) text[at + name_length])) {
        name_length++;
    }
    if (!block_attribute_kind_for_name(text + at, name_length, kind)) {
        return false;
    }
    at += name_length;
    while (text[at] == ' ') {
        at++;
    }
    if (text[at++] != ':') {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    return text[at] == '\0';
}

bool cg_parse_block_attribute_string(const char *text, char **value)
{
    size_t at = 0;

    *value = NULL;
    while (text[at] == ' ') {
        at++;
    }
    if (!cg_parse_escaped_string(text, &at, value)) {
        return false;
    }
    while (text[at] == ' ') {
        at++;
    }
    return text[at] == '\0';
}

bool cg_parse_inline_block_attribute(const char *text, BlockAttributeKind *kind,
                                     char **value)
{
    const char *name;
    size_t name_length;
    char **args = NULL;
    size_t arg_count = 0;

    *kind = BLOCK_ATTR_NONE;
    *value = NULL;
    if (!cg_parse_attribute_call(text, &name, &name_length, &args,
                                 &arg_count)) {
        return false;
    }
    if (arg_count != 1) {
        cg_free_cstr_array(args, arg_count);
        return false;
    }
    if (!block_attribute_kind_for_name(name, name_length, kind)) {
        cg_free_cstr_array(args, arg_count);
        return false;
    }
    if (args[0][0] == '"' || args[0][0] == '\'') {
        size_t parse_at = 0;

        if (!cg_parse_escaped_string(args[0], &parse_at, value)) {
            cg_free_cstr_array(args, arg_count);
            return false;
        }
        while (args[0][parse_at] == ' ') {
            parse_at++;
        }
        if (args[0][parse_at] != '\0') {
            free(*value);
            *value = NULL;
            cg_free_cstr_array(args, arg_count);
            return false;
        }
    } else {
        *value = args[0];
    }
    free(args);
    return true;
}

bool cg_parse_attribute(const char *text, const char **name,
                            size_t *name_length)
{
    size_t at = 0;
    size_t start;

    if (text[at++] != '@' || !cg_name_start((unsigned char) text[at])) {
        return false;
    }
    start = at;
    while (cg_name_char((unsigned char) text[at])) {
        at++;
    }
    *name = text + start;
    *name_length = at - start;
    while (text[at] == ' ') {
        at++;
    }
    return text[at] == '\0';
}

ssize_t cg_read_input_line(FILE *input, char **line, size_t *capacity)
{
    char chunk[512];
    size_t total = 0;

    if (!*line) {
        *capacity = 256;
        *line = malloc(*capacity);
        if (!*line) {
            return -1;
        }
        (*line)[0] = '\0';
    }

    while (fgets(chunk, sizeof(chunk), input)) {
        size_t chunk_length = strlen(chunk);

        if (total + chunk_length + 1 > *capacity) {
            size_t needed = total + chunk_length + 1;
            char *resized = realloc(*line, needed * 2);

            if (!resized) {
                return -1;
            }
            *line = resized;
            *capacity = needed * 2;
        }
        memcpy(*line + total, chunk, chunk_length);
        total += chunk_length;
        if (chunk[chunk_length - 1] == '\n') {
            break;
        }
    }
    if (total == 0 && feof(input)) {
        return -1;
    }
    (*line)[total] = '\0';
    return (ssize_t) total;
}
