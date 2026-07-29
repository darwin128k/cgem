#include "cgem/core/return.h"

#define CGEM_RETURN_TYPE "return"

cgem_return_t *cgem_return_new(cgem_node_t *value_expr, cgem_node_t *owner)
{
    cgem_node_t *node = cgem_object_new("", CGEM_RETURN_TYPE, owner);

    if (!node) {
        cgem_node_free(value_expr);
        return NULL;
    }
    if (value_expr && !cgem_node_add(node, value_expr)) {
        cgem_node_free(value_expr);
        cgem_node_free(node);
        return NULL;
    }
    return node;
}

cgem_node_t *cgem_return_get_value(const cgem_return_t *ret)
{
    if (!ret || cgem_node_get_count((const cgem_node_t *) ret) == 0) {
        return NULL;
    }
    return cgem_node_get((const cgem_node_t *) ret, 0);
}
