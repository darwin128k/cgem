#ifndef CGEM_GENERATOR_H
#define CGEM_GENERATOR_H

#include "cgem/core/attributes.h"
#include "cgem/core/generator_sink.h"
#include "cgem/core/node.h"
#include "cgem/generator_abi.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct cgem_generator cgem_generator_t;

cgem_generator_t *cgem_generator_load(const char *path,
                                      const cgem_attributes_t *config,
                                      char *error, size_t error_size);
void cgem_generator_free(cgem_generator_t *generator);

const char *cgem_generator_get_name(const cgem_generator_t *generator);

bool cgem_generator_has_attribute_key(const cgem_generator_t *generator,
                                      const char *key);
size_t cgem_generator_get_attribute_key_count(const cgem_generator_t *generator);
const char *cgem_generator_get_attribute_key(const cgem_generator_t *generator,
                                             size_t index);

bool cgem_generator_has_type_key(const cgem_generator_t *generator,
                                 const char *name);
size_t cgem_generator_get_type_key_count(const cgem_generator_t *generator);
const char *cgem_generator_get_type_key(const cgem_generator_t *generator,
                                        size_t index);

size_t cgem_generator_get_target_count(const cgem_generator_t *generator);
const char *cgem_generator_get_target_name(const cgem_generator_t *generator,
                                           size_t index);

bool cgem_generator_generate(cgem_generator_t *generator, const char *target,
                             cgem_node_t *root, cgem_generator_sink_t *sink,
                             char *error, size_t error_size);

#endif
