#ifndef CGEM_GENERATOR_SINK_H
#define CGEM_GENERATOR_SINK_H

#include "cgem/core/writer.h"

#include <stddef.h>

typedef struct cgem_generator_sink cgem_generator_sink_t;

cgem_generator_sink_t *cgem_generator_sink_new(const char *root);
void cgem_generator_sink_free(cgem_generator_sink_t *sink);

/* Opens a writer for a file at relative_path under the sink's root,
 * creating any missing parent directories first. */
cgem_writer_t *cgem_generator_sink_open(cgem_generator_sink_t *sink,
                                       const char *relative_path,
                                       char *error, size_t error_size);

#endif
