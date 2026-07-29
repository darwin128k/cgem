#ifndef CGEM_PACKAGE_H
#define CGEM_PACKAGE_H

#include "cgem/core/scope.h"

typedef cgem_scope_t cgem_package_t;

cgem_package_t *cgem_package_new(const char *name, cgem_node_t *owner);
cgem_package_t *cgem_package_new_with_type(const char *name, const char *type,
                                           cgem_node_t *owner);

#endif
