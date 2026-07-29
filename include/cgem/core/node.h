#ifndef CGEM_NODE_H
#define CGEM_NODE_H

#include "cgem/core/array.h"
#include "cgem/core/attributes.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct cgem_node cgem_node_t;

struct cgem_node {
    cgem_attributes_t *attributes;
    cgem_node_t *owner;
    cgem_array_t children;
};

bool cgem_node_init(cgem_node_t *node, cgem_node_t *owner);
void cgem_node_destroy(cgem_node_t *node);

cgem_node_t *cgem_node_new(cgem_node_t *owner);
void cgem_node_free(cgem_node_t *node);

cgem_attributes_t *cgem_node_get_attributes(cgem_node_t *node);
cgem_node_t *cgem_node_get_owner(const cgem_node_t *node);

bool cgem_node_add(cgem_node_t *node, cgem_node_t *child);
size_t cgem_node_get_count(const cgem_node_t *node);
cgem_node_t *cgem_node_get(const cgem_node_t *node, size_t index);

#endif
