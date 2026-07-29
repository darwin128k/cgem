#ifndef CGEM_OBJECT_H
#define CGEM_OBJECT_H

#include "cgem/core/primitive.h"
#include "cgem/core/symbol.h"

typedef cgem_symbol_t cgem_object_t;

cgem_object_t *cgem_object_new(const cgem_char_t *name, const cgem_char_t *type,
                               cgem_node_t *owner);
const cgem_char_t *cgem_object_get_type(const cgem_object_t *object);

#endif
