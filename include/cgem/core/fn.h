#ifndef CGEM_FN_H
#define CGEM_FN_H

#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

typedef cgem_object_t cgem_fn_t;

cgem_fn_t *cgem_fn_new(const cgem_char_t *name, cgem_node_t *owner);

#endif
