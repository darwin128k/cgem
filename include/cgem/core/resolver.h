#ifndef CGEM_RESOLVER_H
#define CGEM_RESOLVER_H

#include "cgem/core/node.h"
#include "cgem/core/primitive.h"

/* Resolves a dotted DSL path (e.g. "lh.void.ptr") by walking children from
 * `root`, matching each segment against a child's name
 * (cgem_symbol_get_name). Returns NULL if any segment is not found -- a
 * dangling reference is a real error, not something core silently ignores. */
cgem_node_t *cgem_resolve(cgem_node_t *root, const cgem_char_t *path);

#endif
