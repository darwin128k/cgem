#include "cgem/core/object.h"

#include "cgem/core/attribute.h"

#define CGEM_OBJECT_TYPE_KEY "type"

cgem_object_t *cgem_object_new(const cgem_char_t *name, const cgem_char_t *type,
                               cgem_node_t *owner)
{
    cgem_node_t *node = cgem_symbol_new(name, owner);
    cgem_attribute_value_t *value;
    cgem_attribute_t *attribute;

    if (!node) {
        return NULL;
    }
    value = cgem_attribute_value_new_string(type);
    if (!value) {
        cgem_node_free(node);
        return NULL;
    }
    attribute = cgem_attribute_new(CGEM_OBJECT_TYPE_KEY, value);
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
    return node;
}

const cgem_char_t *cgem_object_get_type(const cgem_object_t *object)
{
    cgem_attributes_t *attributes;
    const cgem_attribute_t *attribute;

    if (!object) {
        return NULL;
    }
    attributes = cgem_node_get_attributes((cgem_node_t *) object);
    attribute = cgem_attributes_find(attributes, CGEM_OBJECT_TYPE_KEY);
    if (!attribute) {
        return NULL;
    }
    return cgem_attribute_value_get_string(cgem_attribute_get_value(attribute));
}
