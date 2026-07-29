#ifndef CGEM_CONFIG_H
#define CGEM_CONFIG_H

#include "cgem/core/attributes.h"
#include "cgem/core/reader.h"

#include <stddef.h>

#define CGEM_CONFIG_FILENAME "cgem.config"

cgem_attributes_t *cgem_config_parse(cgem_reader_t *reader, char *error,
                                     size_t error_size);

/* Looks for CGEM_CONFIG_FILENAME in the current working directory. Returns
 * NULL with error[0] == '\0' if simply absent (not an error); returns NULL
 * with error set if present but malformed or unreadable. */
cgem_attributes_t *cgem_config_find(char *error, size_t error_size);

#endif
