#ifndef CGEM_FN_H
#define CGEM_FN_H

#include "cgem/core/object.h"
#include "cgem/core/param.h"
#include "cgem/core/primitive.h"

#include <stddef.h>

typedef cgem_object_t cgem_fn_t;

cgem_fn_t *cgem_fn_new(const cgem_char_t *name, cgem_node_t *owner);

/* Never stored on fn itself; recomputed by filtering direct "param"
 * children, same principle as cgem_struct_get_size(). */
size_t cgem_fn_get_param_count(const cgem_fn_t *fn);
cgem_param_t *cgem_fn_get_param(const cgem_fn_t *fn, size_t index);

#endif
