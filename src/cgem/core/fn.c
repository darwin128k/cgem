#include "cgem/core/fn.h"

#define CGEM_FN_TYPE "fn"

cgem_fn_t *cgem_fn_new(const cgem_char_t *name, cgem_node_t *owner)
{
    return cgem_object_new(name, CGEM_FN_TYPE, owner);
}
