#ifndef CGEM_RETURN_H
#define CGEM_RETURN_H

#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

/* A return statement: wraps zero or one expression node (ref/literal/call).
 * NULL value_expr means a bare "return;" (void). The expression is a
 * child, not an attribute, because expressions can themselves be trees
 * (nested calls), unlike field/param values which are flat. */
typedef cgem_object_t cgem_return_t;

cgem_return_t *cgem_return_new(cgem_node_t *value_expr, cgem_node_t *owner);
cgem_node_t *cgem_return_get_value(const cgem_return_t *ret);

#endif
