#ifndef CGEM_COMMON_H
#define CGEM_COMMON_H

#include <stddef.h>

const char *cgem_generated_output_style(const char *include_path,
                                        const char *source_path);
int cgem_clean_output_package(const char *include_path,
                              const char *source_path,
                              const char *package_name,
                              char *error, size_t error_size);

#endif
