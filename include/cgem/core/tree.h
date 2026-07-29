#ifndef CGEM_TREE_H
#define CGEM_TREE_H

#include "cgem/core/cursor.h"
#include "cgem/core/node.h"

typedef struct cgem_tree cgem_tree_t;

cgem_tree_t *cgem_tree_new(void);
void cgem_tree_free(cgem_tree_t *tree);

cgem_node_t *cgem_tree_get_root(const cgem_tree_t *tree);
cgem_cursor_t *cgem_tree_new_cursor(const cgem_tree_t *tree);

#endif
