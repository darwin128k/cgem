#include "cgem/core/node.h"

#include "cgem/core/allocator.h"

cgem_bool_t cgem_node_init(cgem_node_t *node, cgem_node_t *owner)
{
    node->attributes = cgem_attributes_new();
    if (!node->attributes) {
        return false;
    }
    node->owner = owner;
    cgem_array_init(&node->children, 0, sizeof(cgem_node_t *));
    node->allows_children = true;
    return true;
}

void cgem_node_destroy(cgem_node_t *node)
{
    for (size_t i = 0; i < cgem_array_size(&node->children); i++) {
        cgem_node_t *child = *(cgem_node_t **) cgem_array_at(&node->children, i);

        cgem_node_free(child);
    }
    cgem_array_deinit(&node->children);
    cgem_attributes_free(node->attributes);
}

cgem_node_t *cgem_node_new(cgem_node_t *owner)
{
    cgem_node_t *node = cgem_alloc(sizeof(*node));

    if (!node) {
        return NULL;
    }
    if (!cgem_node_init(node, owner)) {
        cgem_free(node);
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
    cgem_free(node);
}

cgem_attributes_t *cgem_node_get_attributes(cgem_node_t *node)
{
    return node ? node->attributes : NULL;
}

cgem_node_t *cgem_node_get_owner(const cgem_node_t *node)
{
    return node ? node->owner : NULL;
}

void cgem_node_set_allows_children(cgem_node_t *node, cgem_bool_t allowed)
{
    if (node) {
        node->allows_children = allowed;
    }
}

cgem_bool_t cgem_node_get_allows_children(const cgem_node_t *node)
{
    return node ? node->allows_children : false;
}

cgem_bool_t cgem_node_add(cgem_node_t *node, cgem_node_t *child)
{
    if (!node || !child || !node->allows_children) {
        return false;
    }
    return cgem_array_push_back(&node->children, &child);
}

size_t cgem_node_get_count(const cgem_node_t *node)
{
    return node ? cgem_array_size(&node->children) : 0;
}

cgem_node_t *cgem_node_get(const cgem_node_t *node, size_t index)
{
    cgem_node_t **slot;

    if (!node) {
        return NULL;
    }
    slot = cgem_array_at(&node->children, index);
    return slot ? *slot : NULL;
}
