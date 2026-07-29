#include "cgem/core/node.h"

#include <stdlib.h>

bool cgem_node_init(cgem_node_t *node, cgem_node_t *owner)
{
    node->attributes = cgem_attributes_new();
    if (!node->attributes) {
        return false;
    }
    node->owner = owner;
    node->children = NULL;
    node->count = 0;
    node->capacity = 0;
    return true;
}

void cgem_node_destroy(cgem_node_t *node)
{
    for (size_t i = 0; i < node->count; i++) {
        cgem_node_free(node->children[i]);
    }
    free(node->children);
    cgem_attributes_free(node->attributes);
}

cgem_node_t *cgem_node_new(cgem_node_t *owner)
{
    cgem_node_t *node = malloc(sizeof(*node));

    if (!node) {
        return NULL;
    }
    if (!cgem_node_init(node, owner)) {
        free(node);
        return NULL;
    }
    return node;
}

void cgem_node_free(cgem_node_t *node)
{
    if (!node) {
        return;
    }
    cgem_node_destroy(node);
    free(node);
}

cgem_attributes_t *cgem_node_get_attributes(cgem_node_t *node)
{
    return node ? node->attributes : NULL;
}

cgem_node_t *cgem_node_get_owner(const cgem_node_t *node)
{
    return node ? node->owner : NULL;
}

bool cgem_node_add(cgem_node_t *node, cgem_node_t *child)
{
    if (!node || !child) {
        return false;
    }
    if (node->count == node->capacity) {
        size_t capacity = node->capacity ? node->capacity * 2 : 4;
        cgem_node_t **children =
            realloc(node->children, capacity * sizeof(*children));

        if (!children) {
            return false;
        }
        node->children = children;
        node->capacity = capacity;
    }
    node->children[node->count++] = child;
    return true;
}

size_t cgem_node_get_count(const cgem_node_t *node)
{
    return node ? node->count : 0;
}

cgem_node_t *cgem_node_get(const cgem_node_t *node, size_t index)
{
    if (!node || index >= node->count) {
        return NULL;
    }
    return node->children[index];
}
