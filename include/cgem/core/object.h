#ifndef CGEM_OBJECT_H
#define CGEM_OBJECT_H

#include "cgem/core/symbol.h"

typedef cgem_symbol_t cgem_object_t;

cgem_object_t *cgem_object_new(const char *name, const char *type,
                               cgem_node_t *owner);
const char *cgem_object_get_type(const cgem_object_t *object);

#endif
