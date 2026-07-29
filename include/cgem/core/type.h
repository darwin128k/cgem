#ifndef CGEM_TYPE_H
#define CGEM_TYPE_H

#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

#include <stddef.h>

/* A raw target primitive materialized from a generator's registered type
 * keys (see cgem_generator_import_types) -- the bottom of the type
 * hierarchy: a named, sized leaf with no further structure. Richer types
 * (struct+field+fn, e.g. "uchar") are built on top of these by having a
 * field reference the raw type's name. */
typedef cgem_object_t cgem_type_t;

cgem_type_t *cgem_type_new(const cgem_char_t *name, size_t size,
                           cgem_node_t *owner);

/* The real byte size on whatever target the generator was configured for
 * when it materialized this type -- never a host assumption. */
size_t cgem_type_get_size(const cgem_type_t *type);

#endif
