#ifndef CGEM_GENERATOR_ABI_H
#define CGEM_GENERATOR_ABI_H

#include "cgem/core/attributes.h"
#include "cgem/core/generator_sink.h"
#include "cgem/core/node.h"
#include "cgem/core/primitive.h"

#include <stddef.h>

#define CGEM_GENERATOR_ABI_VERSION 7u
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
 * "long_long", only registered for C99 and later), each with the type's
 * real byte size in `size`. Core never assumes or hardcodes this size --
 * the generator must obtain it from the actual configured target toolchain
 * (e.g. by querying the configured cross compiler's predefined macros),
 * never from the host compiler that happens to be running cgem itself, so
 * the result stays correct even when cross-compiling. Which type keys get
 * registered, and their sizes, can depend on the resolved config passed
 * into init(), since that is standard/target-specific and core has no
 * opinion on it. */
cgem_bool_t cgem_generator_registrar_add_attribute_key(
    cgem_generator_registrar_t *registrar, const cgem_char_t *key);
cgem_bool_t cgem_generator_registrar_add_type_key(
    cgem_generator_registrar_t *registrar, const cgem_char_t *name,
    size_t size);
cgem_bool_t cgem_generator_registrar_add_target(
    cgem_generator_registrar_t *registrar, const cgem_char_t *name,
    cgem_generate_fn_t fn);

/* Optional. Called after core has materialized the generator's raw type
 * keys (see cgem_generator_import_types) as leaf nodes under `owner`: lets
 * the generator add richer types built on top of those raw primitives --
 * e.g. the C generator builds "uchar" as a real struct+field+fn wrapping
 * the raw "char" leaf with a magic "add" method. Core calls this but has
 * no idea what shape the result takes; that knowledge belongs entirely to
 * the generator, same as everything else target/language-specific. May be
 * NULL if a generator has nothing to add beyond its raw type keys. */
typedef cgem_bool_t (*cgem_bootstrap_types_fn_t)(cgem_node_t *owner,
                                                  cgem_char_t *error,
                                                  size_t error_size);

typedef struct {
    cgem_uint_t abi_version;
    const cgem_char_t *name;
    cgem_bool_t (*init)(cgem_generator_registrar_t *registrar,
                        const cgem_attributes_t *config, cgem_char_t *error,
                        size_t error_size);
    cgem_bootstrap_types_fn_t bootstrap_types;
    void (*deinit)(void);
} cgem_generator_vtable_t;

typedef const cgem_generator_vtable_t *(*cgem_generator_entry_fn_t)(void);

#endif
