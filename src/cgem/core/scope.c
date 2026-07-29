#include "cgem/core/scope.h"

#define CGEM_SCOPE_TYPE "scope"

cgem_scope_t *cgem_scope_new(const cgem_char_t *name, cgem_node_t *owner)
{
    return cgem_object_new(name, CGEM_SCOPE_TYPE, owner);
}

cgem_scope_t *cgem_scope_new_with_type(const cgem_char_t *name,
                                       const cgem_char_t *type,
                                       cgem_node_t *owner)
{
    return cgem_object_new(name, type, owner);
}
