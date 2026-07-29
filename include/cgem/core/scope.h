#ifndef CGEM_SCOPE_H
#define CGEM_SCOPE_H

#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

typedef cgem_object_t cgem_scope_t;

cgem_scope_t *cgem_scope_new(const cgem_char_t *name, cgem_node_t *owner);
cgem_scope_t *cgem_scope_new_with_type(const cgem_char_t *name,
                                       const cgem_char_t *type,
                                       cgem_node_t *owner);

#endif
