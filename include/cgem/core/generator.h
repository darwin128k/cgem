#ifndef CGEM_GENERATOR_H
#define CGEM_GENERATOR_H

#include "cgem/core/attributes.h"
#include "cgem/core/generator_sink.h"
#include "cgem/core/node.h"
#include "cgem/core/primitive.h"
#include "cgem/generator_abi.h"

#include <stddef.h>

typedef struct cgem_generator cgem_generator_t;

cgem_generator_t *cgem_generator_load(const cgem_char_t *path,
                                      const cgem_attributes_t *config,
                                      cgem_char_t *error, size_t error_size);
void cgem_generator_free(cgem_generator_t *generator);

const cgem_char_t *cgem_generator_get_name(const cgem_generator_t *generator);

cgem_bool_t cgem_generator_has_attribute_key(const cgem_generator_t *generator,
                                             const cgem_char_t *key);
size_t cgem_generator_get_attribute_key_count(const cgem_generator_t *generator);
const cgem_char_t *cgem_generator_get_attribute_key(
    const cgem_generator_t *generator, size_t index);

cgem_bool_t cgem_generator_has_type_key(const cgem_generator_t *generator,
                                        const cgem_char_t *name);
size_t cgem_generator_get_type_key_count(const cgem_generator_t *generator);
const cgem_char_t *cgem_generator_get_type_key(
    const cgem_generator_t *generator, size_t index);
/* Real byte size on the generator's actually configured target, queried
 * from the target toolchain -- never a host-compiler assumption. */
size_t cgem_generator_get_type_key_size(const cgem_generator_t *generator,
                                        size_t index);

size_t cgem_generator_get_target_count(const cgem_generator_t *generator);
const cgem_char_t *cgem_generator_get_target_name(
    const cgem_generator_t *generator, size_t index);

cgem_bool_t cgem_generator_generate(cgem_generator_t *generator,
                                    const cgem_char_t *target,
                                    cgem_node_t *root,
                                    cgem_generator_sink_t *sink,
                                    cgem_char_t *error, size_t error_size);

/* Materializes the generator's registered type keys as flat leaf nodes
 * (cgem_type_t) under `owner` -- the bottom of the type hierarchy, each
 * carrying the real, target-queried size the generator supplied via
 * cgem_generator_registrar_add_type_key. Fails atomically: on any error,
 * nodes already added to `owner` are left in place (owner owns them and
 * will free them normally), but no partial node is left dangling. */
cgem_bool_t cgem_generator_import_types(const cgem_generator_t *generator,
                                        cgem_node_t *owner);

#endif
