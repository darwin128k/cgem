#include "cgem/core/literal.h"

#include "cgem/core/attributes.h"

#define CGEM_LITERAL_TYPE "literal"
#define CGEM_LITERAL_VALUE_KEY "value"

cgem_literal_t *cgem_literal_new(cgem_attribute_value_t *value,
                                 cgem_node_t *owner)
{
    cgem_node_t *node = cgem_object_new("", CGEM_LITERAL_TYPE, owner);
    cgem_attribute_t *attribute;

    if (!node) {
        cgem_attribute_value_free(value);
        return NULL;
    }
    attribute = cgem_attribute_new(CGEM_LITERAL_VALUE_KEY, value);
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

const cgem_attribute_value_t *cgem_literal_get_value(
    const cgem_literal_t *literal)
{
    cgem_attributes_t *attributes;
    const cgem_attribute_t *attribute;

    if (!literal) {
        return NULL;
    }
    attributes = cgem_node_get_attributes((cgem_node_t *) literal);
    attribute = cgem_attributes_find(attributes, CGEM_LITERAL_VALUE_KEY);
    if (!attribute) {
        return NULL;
    }
    return cgem_attribute_get_value(attribute);
}
