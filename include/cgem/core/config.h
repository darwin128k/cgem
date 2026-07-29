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

/* Parses one "key=value" argument (same rules as a config file line, minus
 * comment/blank handling) directly into an existing attributes set. */
bool cgem_config_parse_arg(cgem_attributes_t *config, const char *arg,
                           char *error, size_t error_size);

/* Scans argv for "key=value"-shaped tokens and applies each one via
 * cgem_config_parse_arg; tokens without '=' (plain flags, paths, ...) are
 * left untouched for the caller's own argument parsing. */
bool cgem_config_parse_args(cgem_attributes_t *config, int argc, char **argv,
                            char *error, size_t error_size);

#endif
