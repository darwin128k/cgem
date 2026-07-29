#include "cgem/core/symbol.h"

#include "cgem/core/attribute.h"

#define CGEM_SYMBOL_NAME_KEY "name"

cgem_symbol_t *cgem_symbol_new(const cgem_char_t *name, cgem_node_t *owner)
{
    cgem_node_t *node = cgem_node_new(owner);
    cgem_attribute_value_t *value;
    cgem_attribute_t *attribute;

    if (!node) {
        return NULL;
    }
    value = cgem_attribute_value_new_string(name);
    if (!value) {
        cgem_node_free(node);
        return NULL;
    }
    attribute = cgem_attribute_new(CGEM_SYMBOL_NAME_KEY, value);
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

const cgem_char_t *cgem_symbol_get_name(const cgem_symbol_t *symbol)
{
    cgem_attributes_t *attributes;
    const cgem_attribute_t *attribute;

    if (!symbol) {
        return NULL;
    }
    attributes = cgem_node_get_attributes((cgem_node_t *) symbol);
    attribute = cgem_attributes_find(attributes, CGEM_SYMBOL_NAME_KEY);
    if (!attribute) {
        return NULL;
    }
    return cgem_attribute_value_get_string(cgem_attribute_get_value(attribute));
}
