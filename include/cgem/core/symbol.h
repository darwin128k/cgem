#ifndef CGEM_SYMBOL_H
#define CGEM_SYMBOL_H

#include "cgem/core/node.h"

typedef cgem_node_t cgem_symbol_t;

cgem_symbol_t *cgem_symbol_new(const char *name, cgem_node_t *owner);
const char *cgem_symbol_get_name(const cgem_symbol_t *symbol);

#endif
