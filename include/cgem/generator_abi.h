#ifndef CGEM_GENERATOR_ABI_H
#define CGEM_GENERATOR_ABI_H

#include "cgem/core/node.h"
#include "cgem/core/writer.h"

#include <stdbool.h>
#include <stddef.h>

#define CGEM_GENERATOR_ABI_VERSION 1u
#define CGEM_GENERATOR_ENTRY_SYMBOL "cgem_generator_get_vtable"

typedef struct {
    unsigned int abi_version;
    const char *name;
    const char **known_attribute_keys;
    size_t known_attribute_key_count;
    bool (*init)(char *error, size_t error_size);
    void (*deinit)(void);
    bool (*generate)(cgem_node_t *root, cgem_writer_t *writer, char *error,
                     size_t error_size);
} cgem_generator_vtable_t;

typedef const cgem_generator_vtable_t *(*cgem_generator_entry_fn_t)(void);

#endif
