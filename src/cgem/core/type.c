#include "cgem/core/type.h"

#include "cgem/core/attribute.h"
#include "cgem/core/attributes.h"

#define CGEM_TYPE_KIND "type"
#define CGEM_TYPE_SIZE_KEY "size"

cgem_type_t *cgem_type_new(const cgem_char_t *name, size_t size,
                           cgem_node_t *owner)
{
    cgem_node_t *node = cgem_object_new(name, CGEM_TYPE_KIND, owner);
    cgem_attribute_t *attribute;

    if (!node) {
        return NULL;
    }
    attribute = cgem_attribute_new(
        CGEM_TYPE_SIZE_KEY, cgem_attribute_value_new_int((cgem_llong_t) size));
    if (!attribute) {
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

size_t cgem_type_get_size(const cgem_type_t *type)
{
    cgem_attributes_t *attributes;
    const cgem_attribute_t *attribute;

    if (!type) {
        return 0;
    }
    attributes = cgem_node_get_attributes((cgem_node_t *) type);
    attribute = cgem_attributes_find(attributes, CGEM_TYPE_SIZE_KEY);
    if (!attribute) {
        return 0;
    }
    return (size_t) cgem_attribute_value_get_int(
        cgem_attribute_get_value(attribute));
}
