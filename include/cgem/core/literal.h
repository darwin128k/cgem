#ifndef CGEM_LITERAL_H
#define CGEM_LITERAL_H

#include "cgem/core/attribute.h"
#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

/* A constant-value expression: a leaf carrying one attribute value (int,
 * float, string, bool -- whatever the DSL literal syntax produced). */
typedef cgem_object_t cgem_literal_t;

cgem_literal_t *cgem_literal_new(cgem_attribute_value_t *value,
                                 cgem_node_t *owner);
const cgem_attribute_value_t *cgem_literal_get_value(
    const cgem_literal_t *literal);

#endif
