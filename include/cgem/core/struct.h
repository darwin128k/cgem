#ifndef CGEM_STRUCT_H
#define CGEM_STRUCT_H

#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

#include <stddef.h>

typedef cgem_object_t cgem_struct_t;

cgem_struct_t *cgem_struct_new(const cgem_char_t *name, cgem_node_t *owner);

/* Sum of cgem_attribute_value_get_size() over every direct "field" child;
 * always recomputed from current field contents, never stored. */
size_t cgem_struct_get_size(const cgem_struct_t *s);

#endif
