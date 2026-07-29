#include "cgem/core/attribute.h"

#include "cgem/core/allocator.h"
#include "cgem/core/array.h"
#include "cgem/core/string.h"
#include <string.h>

struct cgem_attribute_value {
    cgem_attribute_value_kind_t kind;
    union {
        cgem_bool_t boolean;
        cgem_llong_t integer;
        cgem_double_t floating;
        cgem_string_t *string;
        cgem_array_t list;
    };
};

struct cgem_attribute {
    cgem_char_t *key;
    cgem_attribute_value_t *value;
};

static cgem_char_t *copy_string(const cgem_char_t *text)
{
    size_t length = strlen(text);
    cgem_char_t *copy = cgem_alloc(length + 1);

    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static cgem_attribute_value_t *new_value(cgem_attribute_value_kind_t kind)
{
    cgem_attribute_value_t *value = cgem_alloc_zeroed(1, sizeof(*value));

    if (!value) {
        return NULL;
    }
    value->kind = kind;
    return value;
}

cgem_attribute_value_t *cgem_attribute_value_new_null(void)
{
    return new_value(ATTR_VALUE_NULL);
}

cgem_attribute_value_t *cgem_attribute_value_new_bool(cgem_bool_t value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_BOOL);

    if (result) {
        result->boolean = value;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_int(cgem_llong_t value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_INT);

    if (result) {
        result->integer = value;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_float(cgem_double_t value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_FLOAT);

    if (result) {
        result->floating = value;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_string(
    const cgem_char_t *value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_STRING);

    if (!result) {
        return NULL;
    }
    result->string = cgem_string_new(value);
    if (!result->string) {
        cgem_free(result);
        return NULL;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_symbol(
    const cgem_char_t *value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_SYMBOL);

    if (!result) {
        return NULL;
    }
    result->string = cgem_string_new(value);
    if (!result->string) {
        cgem_free(result);
        return NULL;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_list(void)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_LIST);

    if (!result) {
        return NULL;
    }
    cgem_array_init(&result->list, 0, sizeof(cgem_attribute_value_t));
    return result;
}

static void free_value_contents(cgem_attribute_value_t *value)
{
    switch (value->kind) {
    case ATTR_VALUE_STRING:
    case ATTR_VALUE_SYMBOL:
        cgem_string_free(value->string);
        break;
    case ATTR_VALUE_LIST:
        for (size_t i = 0; i < cgem_array_size(&value->list); i++) {
            free_value_contents(cgem_array_at(&value->list, i));
        }
        cgem_array_deinit(&value->list);
        break;
    default:
        break;
    }
}

void cgem_attribute_value_free(cgem_attribute_value_t *value)
{
    if (!value) {
        return;
    }
    free_value_contents(value);
    cgem_free(value);
}

cgem_bool_t cgem_attribute_value_list_append(cgem_attribute_value_t *list,
                                             cgem_attribute_value_t *item)
{
    if (!list || !item || list->kind != ATTR_VALUE_LIST) {
        return false;
    }
    if (!cgem_array_push_back(&list->list, item)) {
        return false;
    }
    cgem_free(item);
    return true;
}

cgem_attribute_value_kind_t cgem_attribute_value_get_kind(
    const cgem_attribute_value_t *value)
{
    return value ? value->kind : ATTR_VALUE_NULL;
}

cgem_bool_t cgem_attribute_value_get_bool(const cgem_attribute_value_t *value)
{
    return value && value->kind == ATTR_VALUE_BOOL ? value->boolean : false;
}

cgem_llong_t cgem_attribute_value_get_int(const cgem_attribute_value_t *value)
{
    return value && value->kind == ATTR_VALUE_INT ? value->integer : 0;
}

cgem_double_t cgem_attribute_value_get_float(
    const cgem_attribute_value_t *value)
{
    return value && value->kind == ATTR_VALUE_FLOAT ? value->floating : 0.0;
}

const cgem_char_t *cgem_attribute_value_get_string(
    const cgem_attribute_value_t *value)
{
    if (!value || (value->kind != ATTR_VALUE_STRING &&
                   value->kind != ATTR_VALUE_SYMBOL)) {
        return NULL;
    }
    return cgem_string_get_data(value->string);
}

size_t cgem_attribute_value_list_get_count(const cgem_attribute_value_t *value)
{
    return value && value->kind == ATTR_VALUE_LIST
               ? cgem_array_size(&value->list)
               : 0;
}

size_t cgem_attribute_value_get_size(const cgem_attribute_value_t *value)
{
    if (!value) {
        return 0;
    }
    switch (value->kind) {
    case ATTR_VALUE_NULL:
        return 0;
    case ATTR_VALUE_BOOL:
        return sizeof(cgem_bool_t);
    case ATTR_VALUE_INT:
        return sizeof(cgem_llong_t);
    case ATTR_VALUE_FLOAT:
        return sizeof(cgem_double_t);
    case ATTR_VALUE_STRING:
    case ATTR_VALUE_SYMBOL:
        return cgem_string_get_length(value->string) + 1;
    case ATTR_VALUE_LIST: {
        size_t total = 0;

        for (size_t i = 0; i < cgem_array_size(&value->list); i++) {
            total += cgem_attribute_value_get_size(cgem_array_at(&value->list, i));
        }
        return total;
    }
    default:
        return 0;
    }
}

const cgem_attribute_value_t *cgem_attribute_value_list_get(
    const cgem_attribute_value_t *value, size_t index)
{
    if (!value || value->kind != ATTR_VALUE_LIST) {
        return NULL;
    }
    return cgem_array_at(&value->list, index);
}

cgem_attribute_t *cgem_attribute_new(const cgem_char_t *key,
                                     cgem_attribute_value_t *value)
{
    cgem_attribute_t *attribute = cgem_alloc_zeroed(1, sizeof(*attribute));

    if (!attribute) {
        return NULL;
    }
    attribute->key = copy_string(key);
    if (!attribute->key) {
        cgem_free(attribute);
        return NULL;
    }
    attribute->value = value;
    return attribute;
}

void cgem_attribute_free(cgem_attribute_t *attribute)
{
    if (!attribute) {
        return;
    }
    cgem_free(attribute->key);
    cgem_attribute_value_free(attribute->value);
    cgem_free(attribute);
}

const cgem_char_t *cgem_attribute_get_key(const cgem_attribute_t *attribute)
{
    return attribute ? attribute->key : NULL;
}

const cgem_attribute_value_t *cgem_attribute_get_value(
    const cgem_attribute_t *attribute)
{
    return attribute ? attribute->value : NULL;
}
