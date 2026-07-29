#include "cgem/core/field.h"

#define CGEM_FIELD_TYPE "field"
#define CGEM_FIELD_VALUE_KEY "value"

cgem_field_t *cgem_field_new(const cgem_char_t *name,
                             cgem_attribute_value_t *value,
                             cgem_node_t *owner)
{
    cgem_node_t *node = cgem_object_new(name, CGEM_FIELD_TYPE, owner);
    cgem_attribute_t *attribute;

    if (!node) {
        cgem_attribute_value_free(value);
        return NULL;
    }
    attribute = cgem_attribute_new(CGEM_FIELD_VALUE_KEY, value);
    if (!attribute) {
        cgem_attribute_value_free(value);
        cgem_node_free(node);
        return NULL;
    }
    if (!cgem_attributes_add(cgem_node_get_attributes(node), attribute)) {
        cgem_attribute_free(attribute);
        cgem_node_free(node);
        return NULL;
    }
    cgem_node_set_allows_children(node, false);
    return node;
}

const cgem_attribute_value_t *cgem_field_get_value(const cgem_field_t *field)
{
    cgem_attributes_t *attributes;
    const cgem_attribute_t *attribute;

    if (!field) {
        return NULL;
    }
    attributes = cgem_node_get_attributes((cgem_node_t *) field);
    attribute = cgem_attributes_find(attributes, CGEM_FIELD_VALUE_KEY);
    if (!attribute) {
        return NULL;
    }
    return cgem_attribute_get_value(attribute);
}

cgem_bool_t cgem_field_set_value(cgem_field_t *field,
                                 cgem_attribute_value_t *value)
{
    cgem_attribute_t *attribute;

    if (!field) {
        cgem_attribute_value_free(value);
        return false;
    }
    attribute = cgem_attribute_new(CGEM_FIELD_VALUE_KEY, value);
    if (!attribute) {
        cgem_attribute_value_free(value);
        return false;
    }
    if (!cgem_attributes_add(cgem_node_get_attributes((cgem_node_t *) field),
                             attribute)) {
        cgem_attribute_free(attribute);
        return false;
    }
    return true;
}
