#include "cgem/core/package.h"

#define CGEM_PACKAGE_TYPE "package"

cgem_package_t *cgem_package_new(const cgem_char_t *name, cgem_node_t *owner)
{
    return cgem_package_new_with_type(name, CGEM_PACKAGE_TYPE, owner);
}

cgem_package_t *cgem_package_new_with_type(const cgem_char_t *name,
                                           const cgem_char_t *type,
                                           cgem_node_t *owner)
{
    return cgem_scope_new_with_type(name, type, owner);
}
