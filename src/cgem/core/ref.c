#include "cgem/core/ref.h"

#include "cgem/core/symbol.h"

#define CGEM_REF_TYPE "ref"

cgem_ref_t *cgem_ref_new(const cgem_char_t *path, cgem_node_t *owner)
{
    cgem_node_t *node = cgem_object_new(path, CGEM_REF_TYPE, owner);

    if (!node) {
        return NULL;
    }
    cgem_node_set_allows_children(node, false);
    return node;
}

const cgem_char_t *cgem_ref_get_path(const cgem_ref_t *ref)
{
    return cgem_symbol_get_name((const cgem_symbol_t *) ref);
}
