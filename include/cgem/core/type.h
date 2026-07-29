#ifndef CGEM_TYPE_H
#define CGEM_TYPE_H

#include "cgem/core/object.h"

typedef cgem_object_t cgem_type_t;

cgem_type_t *cgem_type_new(const char *name, const char *ref,
                           cgem_node_t *owner);
const char *cgem_type_get_ref(const cgem_type_t *type);

#endif
