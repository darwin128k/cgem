#include "cgem/core/module.h"

#define CGEM_MODULE_TYPE "module"

cgem_module_t *cgem_module_new(const char *name, cgem_node_t *owner)
{
    return cgem_scope_new_with_type(name, CGEM_MODULE_TYPE, owner);
}
