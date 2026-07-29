#include "cgem/core/attributes.h"

#include "cgem/core/allocator.h"
#include <string.h>

#define CGEM_ATTRIBUTES_INITIAL_BUCKETS 8

typedef struct attr_bucket_node {
    cgem_attribute_t *attribute;
    size_t item_index;
    struct attr_bucket_node *next;
} attr_bucket_node_t;

struct cgem_attributes {
    cgem_attribute_t **items;
    size_t count;
    size_t capacity;

    attr_bucket_node_t **buckets;
    size_t bucket_count;
};

static size_t hash_key(const char *key)
{
    size_t hash = 5381;
    unsigned char c;

    while ((c = (unsigned char) *key++)) {
        hash = hash * 33 + c;
    }
    return hash;
}

static bool ensure_buckets(cgem_attributes_t *attributes)
{
    if (attributes->buckets) {
        return true;
    }
    attributes->buckets =
        cgem_alloc_zeroed(CGEM_ATTRIBUTES_INITIAL_BUCKETS, sizeof(*attributes->buckets));
    if (!attributes->buckets) {
        return false;
    }
    attributes->bucket_count = CGEM_ATTRIBUTES_INITIAL_BUCKETS;
    return true;
}

static bool rehash(cgem_attributes_t *attributes, size_t new_bucket_count)
{
    attr_bucket_node_t **new_buckets = cgem_alloc_zeroed(new_bucket_count, sizeof(*new_buckets));

    if (!new_buckets) {
        return false;
    }
    for (size_t i = 0; i < attributes->bucket_count; i++) {
        attr_bucket_node_t *node = attributes->buckets[i];

        while (node) {
            attr_bucket_node_t *next = node->next;
            size_t index =
                hash_key(cgem_attribute_get_key(node->attribute)) % new_bucket_count;

            node->next = new_buckets[index];
            new_buckets[index] = node;
            node = next;
        }
    }
    cgem_free(attributes->buckets);
    attributes->buckets = new_buckets;
    attributes->bucket_count = new_bucket_count;
    return true;
}

static attr_bucket_node_t *find_node(const cgem_attributes_t *attributes,
                                     const char *key)
{
    size_t index;
    attr_bucket_node_t *node;

    if (!attributes->buckets) {
        return NULL;
    }
    index = hash_key(key) % attributes->bucket_count;
    node = attributes->buckets[index];
    while (node) {
        const char *existing_key = cgem_attribute_get_key(node->attribute);

        if (existing_key && strcmp(existing_key, key) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

cgem_attributes_t *cgem_attributes_new(void)
{
    return cgem_alloc_zeroed(1, sizeof(cgem_attributes_t));
}

void cgem_attributes_free(cgem_attributes_t *attributes)
{
    if (!attributes) {
        return;
    }
    for (size_t i = 0; i < attributes->bucket_count; i++) {
        attr_bucket_node_t *node = attributes->buckets[i];

        while (node) {
            attr_bucket_node_t *next = node->next;

            cgem_free(node);
            node = next;
        }
    }
    cgem_free(attributes->buckets);
    for (size_t i = 0; i < attributes->count; i++) {
        cgem_attribute_free(attributes->items[i]);
    }
    cgem_free(attributes->items);
    cgem_free(attributes);
}

bool cgem_attributes_add(cgem_attributes_t *attributes,
                         cgem_attribute_t *attribute)
{
    const char *key;
    attr_bucket_node_t *existing;
    attr_bucket_node_t *node;
    size_t index;

    if (!attributes || !attribute) {
        return false;
    }
    if (!ensure_buckets(attributes)) {
        return false;
    }
    key = cgem_attribute_get_key(attribute);
    existing = find_node(attributes, key);
    if (existing) {
        cgem_attribute_free(attributes->items[existing->item_index]);
        attributes->items[existing->item_index] = attribute;
        existing->attribute = attribute;
        return true;
    }
    if (attributes->count == attributes->capacity) {
        size_t capacity = attributes->capacity ? attributes->capacity * 2 : 4;
        cgem_attribute_t **items =
            cgem_realloc(attributes->items, capacity * sizeof(*items));

        if (!items) {
            return false;
        }
        attributes->items = items;
        attributes->capacity = capacity;
    }
    if (attributes->count >= attributes->bucket_count) {
        if (!rehash(attributes, attributes->bucket_count * 2)) {
            return false;
        }
    }
    node = cgem_alloc(sizeof(*node));
    if (!node) {
        return false;
    }
    attributes->items[attributes->count] = attribute;
    node->attribute = attribute;
    node->item_index = attributes->count;
    index = hash_key(key) % attributes->bucket_count;
    node->next = attributes->buckets[index];
    attributes->buckets[index] = node;
    attributes->count++;
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
    attr_bucket_node_t *node;

    if (!attributes || !key) {
        return NULL;
    }
    node = find_node(attributes, key);
    return node ? node->attribute : NULL;
}

bool cgem_attributes_has(const cgem_attributes_t *attributes, const char *key)
{
    return cgem_attributes_find(attributes, key) != NULL;
}
