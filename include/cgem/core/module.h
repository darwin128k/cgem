#ifndef CGEM_MODULE_H
#define CGEM_MODULE_H

#include "cgem/core/primitive.h"
#include "cgem/core/scope.h"

typedef cgem_scope_t cgem_module_t;

cgem_module_t *cgem_module_new(const cgem_char_t *name, cgem_node_t *owner);

#endif
