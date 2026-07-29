#ifndef CGEM_FN_H
#define CGEM_FN_H

#include "cgem/core/attribute.h"
#include "cgem/core/object.h"
#include "cgem/core/param.h"
#include "cgem/core/primitive.h"

#include <stddef.h>

typedef cgem_object_t cgem_fn_t;

/* return_type follows the same convention as field/param value: NULL means
 * void, a non-NULL value (normally ATTR_VALUE_SYMBOL, e.g. "c.int") means
 * "returns this type". Core stores whatever it's given; the DSL grammar is
 * what would restrict the shape. */
cgem_fn_t *cgem_fn_new(const cgem_char_t *name,
                       cgem_attribute_value_t *return_type,
                       cgem_node_t *owner);
const cgem_attribute_value_t *cgem_fn_get_return_type(const cgem_fn_t *fn);
cgem_bool_t cgem_fn_set_return_type(cgem_fn_t *fn,
                                    cgem_attribute_value_t *return_type);

/* Never stored on fn itself; recomputed by filtering direct "param"
 * children, same principle as cgem_struct_get_size(). */
size_t cgem_fn_get_param_count(const cgem_fn_t *fn);
cgem_param_t *cgem_fn_get_param(const cgem_fn_t *fn, size_t index);

/* Statements (currently just cgem_return_t, more kinds to come) are added
 * with the generic cgem_node_add(fn, statement) and executed in child
 * order. Recomputed by filtering out "param" children -- so any non-param
 * child is a statement, with no need to update this filter as new
 * statement kinds are added. */
size_t cgem_fn_get_statement_count(const cgem_fn_t *fn);
cgem_node_t *cgem_fn_get_statement(const cgem_fn_t *fn, size_t index);

#endif
