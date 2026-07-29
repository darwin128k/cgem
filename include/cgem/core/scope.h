#ifndef CGEM_SCOPE_H
#define CGEM_SCOPE_H

#include "cgem/core/object.h"

typedef cgem_object_t cgem_scope_t;

cgem_scope_t *cgem_scope_new(const char *name, cgem_node_t *owner);
cgem_scope_t *cgem_scope_new_with_type(const char *name, const char *type,
                                       cgem_node_t *owner);

#endif
