#include "cgem/core/attribute.h"

#include "cgem/core/allocator.h"
#include <string.h>

struct cgem_attribute_value {
    cgem_attribute_value_kind_t kind;
    union {
        bool boolean;
        long long integer;
        double floating;
        char *string;
        struct {
            cgem_attribute_value_t *items;
            size_t count;
            size_t capacity;
        } list;
    };
};

struct cgem_attribute {
    char *key;
    cgem_attribute_value_t *value;
};

static char *copy_string(const char *text)
{
    size_t length = strlen(text);
    char *copy = cgem_alloc(length + 1);

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

cgem_attribute_value_t *cgem_attribute_value_new_bool(bool value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_BOOL);

    if (result) {
        result->boolean = value;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_int(long long value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_INT);

    if (result) {
        result->integer = value;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_float(double value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_FLOAT);

    if (result) {
        result->floating = value;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_string(const char *value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_STRING);

    if (!result) {
        return NULL;
    }
    result->string = copy_string(value);
    if (!result->string) {
        cgem_free(result);
        return NULL;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_symbol(const char *value)
{
    cgem_attribute_value_t *result = new_value(ATTR_VALUE_SYMBOL);

    if (!result) {
        return NULL;
    }
    result->string = copy_string(value);
    if (!result->string) {
        cgem_free(result);
        return NULL;
    }
    return result;
}

cgem_attribute_value_t *cgem_attribute_value_new_list(void)
{
    return new_value(ATTR_VALUE_LIST);
}

static void free_value_contents(cgem_attribute_value_t *value)
{
    switch (value->kind) {
    case ATTR_VALUE_STRING:
    case ATTR_VALUE_SYMBOL:
        cgem_free(value->string);
        break;
    case ATTR_VALUE_LIST:
        for (size_t i = 0; i < value->list.count; i++) {
            free_value_contents(&value->list.items[i]);
        }
        cgem_free(value->list.items);
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

bool cgem_attribute_value_list_append(cgem_attribute_value_t *list,
                                      cgem_attribute_value_t *item)
{
    if (!list || !item || list->kind != ATTR_VALUE_LIST) {
        return false;
    }
    if (list->list.count == list->list.capacity) {
        size_t capacity = list->list.capacity ? list->list.capacity * 2 : 4;
        cgem_attribute_value_t *items =
            cgem_realloc(list->list.items, capacity * sizeof(*items));

        if (!items) {
            return false;
        }
        list->list.items = items;
        list->list.capacity = capacity;
    }
    list->list.items[list->list.count++] = *item;
    cgem_free(item);
    return true;
}

cgem_attribute_value_kind_t cgem_attribute_value_get_kind(
    const cgem_attribute_value_t *value)
{
    return value ? value->kind : ATTR_VALUE_NULL;
}

bool cgem_attribute_value_get_bool(const cgem_attribute_value_t *value)
{
    return value && value->kind == ATTR_VALUE_BOOL ? value->boolean : false;
}

long long cgem_attribute_value_get_int(const cgem_attribute_value_t *value)
{
    return value && value->kind == ATTR_VALUE_INT ? value->integer : 0;
}

double cgem_attribute_value_get_float(const cgem_attribute_value_t *value)
{
    return value && value->kind == ATTR_VALUE_FLOAT ? value->floating : 0.0;
}

const char *cgem_attribute_value_get_string(const cgem_attribute_value_t *value)
{
    if (!value || (value->kind != ATTR_VALUE_STRING &&
                   value->kind != ATTR_VALUE_SYMBOL)) {
        return NULL;
    }
    return value->string;
}

size_t cgem_attribute_value_list_get_count(const cgem_attribute_value_t *value)
{
    return value && value->kind == ATTR_VALUE_LIST ? value->list.count : 0;
}

const cgem_attribute_value_t *cgem_attribute_value_list_get(
    const cgem_attribute_value_t *value, size_t index)
{
    if (!value || value->kind != ATTR_VALUE_LIST || index >= value->list.count) {
        return NULL;
    }
    return &value->list.items[index];
}

cgem_attribute_t *cgem_attribute_new(const char *key,
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

const char *cgem_attribute_get_key(const cgem_attribute_t *attribute)
{
    return attribute ? attribute->key : NULL;
}

const cgem_attribute_value_t *cgem_attribute_get_value(
    const cgem_attribute_t *attribute)
{
    return attribute ? attribute->value : NULL;
}
