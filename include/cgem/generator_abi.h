#ifndef CGEM_GENERATOR_ABI_H
#define CGEM_GENERATOR_ABI_H

#include "cgem/core/attributes.h"
#include "cgem/core/generator_sink.h"
#include "cgem/core/node.h"
#include "cgem/core/primitive.h"

#include <stddef.h>

#define CGEM_GENERATOR_ABI_VERSION 5u
#define CGEM_GENERATOR_ENTRY_SYMBOL "cgem_generator_get_vtable"

typedef struct cgem_generator_registrar cgem_generator_registrar_t;

/* sink lets a generator create as many output files as it needs (one per
 * module/header/source, mirroring package nesting as directories) instead
 * of writing everything to a single stream. */
typedef cgem_bool_t (*cgem_generate_fn_t)(cgem_node_t *root,
                                          cgem_generator_sink_t *sink,
                                          cgem_char_t *error,
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
cgem_bool_t cgem_generator_registrar_add_attribute_key(
    cgem_generator_registrar_t *registrar, const cgem_char_t *key);
cgem_bool_t cgem_generator_registrar_add_type_key(
    cgem_generator_registrar_t *registrar, const cgem_char_t *name);
cgem_bool_t cgem_generator_registrar_add_target(
    cgem_generator_registrar_t *registrar, const cgem_char_t *name,
    cgem_generate_fn_t fn);

typedef struct {
    cgem_uint_t abi_version;
    const cgem_char_t *name;
    cgem_bool_t (*init)(cgem_generator_registrar_t *registrar,
                        const cgem_attributes_t *config, cgem_char_t *error,
                        size_t error_size);
    void (*deinit)(void);
} cgem_generator_vtable_t;

typedef const cgem_generator_vtable_t *(*cgem_generator_entry_fn_t)(void);

#endif
