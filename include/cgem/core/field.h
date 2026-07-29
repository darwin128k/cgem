#ifndef CGEM_FIELD_H
#define CGEM_FIELD_H

#include "cgem/core/attribute.h"
#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

typedef cgem_object_t cgem_field_t;

cgem_field_t *cgem_field_new(const cgem_char_t *name,
                             cgem_attribute_value_t *value,
                             cgem_node_t *owner);
const cgem_attribute_value_t *cgem_field_get_value(const cgem_field_t *field);
cgem_bool_t cgem_field_set_value(cgem_field_t *field,
                                 cgem_attribute_value_t *value);

#endif
