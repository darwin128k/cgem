#include "cgem/core/attributes.h"

#include <stdlib.h>
#include <string.h>

struct cgem_attributes {
    cgem_attribute_t **items;
    size_t count;
    size_t capacity;
};

cgem_attributes_t *cgem_attributes_new(void)
{
    return calloc(1, sizeof(cgem_attributes_t));
}

void cgem_attributes_free(cgem_attributes_t *attributes)
{
    if (!attributes) {
        return;
    }
    for (size_t i = 0; i < attributes->count; i++) {
        cgem_attribute_free(attributes->items[i]);
    }
    free(attributes->items);
    free(attributes);
}

bool cgem_attributes_add(cgem_attributes_t *attributes,
                         cgem_attribute_t *attribute)
{
    const char *key;

    if (!attributes || !attribute) {
        return false;
    }
    key = cgem_attribute_get_key(attribute);
    for (size_t i = 0; i < attributes->count; i++) {
        const char *existing_key = cgem_attribute_get_key(attributes->items[i]);

        if (existing_key && key && strcmp(existing_key, key) == 0) {
            cgem_attribute_free(attributes->items[i]);
            attributes->items[i] = attribute;
            return true;
        }
    }
    if (attributes->count == attributes->capacity) {
        size_t capacity = attributes->capacity ? attributes->capacity * 2 : 4;
        cgem_attribute_t **items =
            realloc(attributes->items, capacity * sizeof(*items));

        if (!items) {
            return false;
        }
        attributes->items = items;
        attributes->capacity = capacity;
    }
    attributes->items[attributes->count++] = attribute;
    return true;
}

size_t cgem_attributes_get_count(const cgem_attributes_t *attributes)
{
    return attributes ? attributes->count : 0;
}

const cgem_attribute_t *cgem_attributes_get(
    const cgem_attributes_t *attributes, size_t index)
{
    if (!attributes || index >= attributes->count) {
        return NULL;
    }
    return attributes->items[index];
}

const cgem_attribute_t *cgem_attributes_find(
    const cgem_attributes_t *attributes, const char *key)
{
    if (!attributes || !key) {
        return NULL;
    }
    for (size_t i = 0; i < attributes->count; i++) {
        const char *item_key = cgem_attribute_get_key(attributes->items[i]);

        if (item_key && strcmp(item_key, key) == 0) {
            return attributes->items[i];
        }
    }
    return NULL;
}

bool cgem_attributes_has(const cgem_attributes_t *attributes, const char *key)
{
    return cgem_attributes_find(attributes, key) != NULL;
}
