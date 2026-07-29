#ifndef CGEM_PARAM_H
#define CGEM_PARAM_H

#include "cgem/core/attribute.h"
#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

typedef cgem_object_t cgem_param_t;

/* Same shape as cgem_field_t (name + value, leaf) but its own DSL category
 * ("param" vs "field") so struct fields and function parameters are never
 * accidentally confused when walking a node's children. */
cgem_param_t *cgem_param_new(const cgem_char_t *name,
                             cgem_attribute_value_t *value,
                             cgem_node_t *owner);
const cgem_attribute_value_t *cgem_param_get_value(const cgem_param_t *param);
cgem_bool_t cgem_param_set_value(cgem_param_t *param,
                                 cgem_attribute_value_t *value);

#endif
