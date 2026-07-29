#ifndef CGEM_PACKAGE_H
#define CGEM_PACKAGE_H

#include "cgem/core/primitive.h"
#include "cgem/core/scope.h"

typedef cgem_scope_t cgem_package_t;

cgem_package_t *cgem_package_new(const cgem_char_t *name, cgem_node_t *owner);
cgem_package_t *cgem_package_new_with_type(const cgem_char_t *name,
                                           const cgem_char_t *type,
                                           cgem_node_t *owner);

#endif
