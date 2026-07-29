#ifndef CGEM_FIELD_H
#define CGEM_FIELD_H

#include "cgem/core/attribute.h"
#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

typedef cgem_object_t cgem_field_t;

/* value is unrestricted here on purpose: the DSL lexer/parser will require
 * it to be either NULL or an ATTR_VALUE_SYMBOL type reference (a field
 * without "as SomeType" is invalid DSL), but that is a grammar rule, not a
 * core one. Generators may still construct fields with arbitrary values
 * for their own internal bookkeeping outside the DSL. */
cgem_field_t *cgem_field_new(const cgem_char_t *name,
                             cgem_attribute_value_t *value,
                             cgem_node_t *owner);
const cgem_attribute_value_t *cgem_field_get_value(const cgem_field_t *field);
cgem_bool_t cgem_field_set_value(cgem_field_t *field,
                                 cgem_attribute_value_t *value);

#endif
