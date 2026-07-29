#include "cgem/core/tree.h"

#include "cgem/core/allocator.h"

struct cgem_tree {
    cgem_node_t *root;
};

cgem_tree_t *cgem_tree_new(void)
{
    cgem_tree_t *tree = cgem_alloc_zeroed(1, sizeof(*tree));

    if (!tree) {
        return NULL;
    }
    tree->root = cgem_node_new(NULL);
    if (!tree->root) {
        cgem_free(tree);
        return NULL;
    }
    return tree;
}

void cgem_tree_free(cgem_tree_t *tree)
{
    if (!tree) {
        return;
    }
    cgem_node_free(tree->root);
    cgem_free(tree);
}

cgem_node_t *cgem_tree_get_root(const cgem_tree_t *tree)
{
    return tree ? tree->root : NULL;
}

cgem_cursor_t *cgem_tree_new_cursor(const cgem_tree_t *tree)
{
    return tree ? cgem_cursor_new(tree->root) : NULL;
}
