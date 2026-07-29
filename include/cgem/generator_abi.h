#ifndef CGEM_GENERATOR_ABI_H
#define CGEM_GENERATOR_ABI_H

#include "cgem/core/attributes.h"
#include "cgem/core/generator_sink.h"
#include "cgem/core/node.h"

#include <stdbool.h>
#include <stddef.h>

#define CGEM_GENERATOR_ABI_VERSION 4u
#define CGEM_GENERATOR_ENTRY_SYMBOL "cgem_generator_get_vtable"

typedef struct cgem_generator_registrar cgem_generator_registrar_t;

/* sink lets a generator create as many output files as it needs (one per
 * module/header/source, mirroring package nesting as directories) instead
 * of writing everything to a single stream. */
typedef bool (*cgem_generate_fn_t)(cgem_node_t *root,
                                   cgem_generator_sink_t *sink, char *error,
                                   size_t error_size);

/* Host-provided registration calls, analogous to AMX Mod X's MF_AddNatives:
 * the generator calls these from init() to declare what it understands and
 * what it can produce, instead of handing back a fixed, fully-formed table.
 *
 * Attribute keys are things like "define"/"extern" (@define, @extern in the
 * DSL). Type keys are global type names the generator provides (e.g.
 * "long_long", only registered for C99 and later). Which ones get
 * registered can depend on the resolved config passed into init(), since
 * that is standard/target-specific and core has no opinion on it. */
bool cgem_generator_registrar_add_attribute_key(
    cgem_generator_registrar_t *registrar, const char *key);
bool cgem_generator_registrar_add_type_key(
    cgem_generator_registrar_t *registrar, const char *name);
bool cgem_generator_registrar_add_target(cgem_generator_registrar_t *registrar,
                                         const char *name,
                                         cgem_generate_fn_t fn);

typedef struct {
    unsigned int abi_version;
    const char *name;
    bool (*init)(cgem_generator_registrar_t *registrar,
                const cgem_attributes_t *config, char *error,
                size_t error_size);
    void (*deinit)(void);
} cgem_generator_vtable_t;

typedef const cgem_generator_vtable_t *(*cgem_generator_entry_fn_t)(void);

#endif
