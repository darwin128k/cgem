#include "cgem/core/struct.h"

#include "cgem/core/field.h"

#include <string.h>

#define CGEM_STRUCT_TYPE "struct"
#define CGEM_STRUCT_FIELD_TYPE "field"

cgem_struct_t *cgem_struct_new(const cgem_char_t *name, cgem_node_t *owner)
{
    return cgem_object_new(name, CGEM_STRUCT_TYPE, owner);
}

size_t cgem_struct_get_size(const cgem_struct_t *s)
{
    size_t total = 0;
    size_t count;
    size_t i;

    if (!s) {
        return 0;
    }
    count = cgem_node_get_count((const cgem_node_t *) s);
    for (i = 0; i < count; i++) {
        cgem_node_t *child = cgem_node_get((const cgem_node_t *) s, i);
        const cgem_char_t *type = cgem_object_get_type(child);

        if (type && strcmp(type, CGEM_STRUCT_FIELD_TYPE) == 0) {
            total += cgem_attribute_value_get_size(cgem_field_get_value(child));
        }
    }
    return total;
}
