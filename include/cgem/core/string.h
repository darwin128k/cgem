#ifndef CGEM_STRING_H
#define CGEM_STRING_H

#include "cgem/core/primitive.h"

#include <stddef.h>

typedef struct cgem_string cgem_string_t;

cgem_string_t *cgem_string_new(const cgem_char_t *text);
void cgem_string_free(cgem_string_t *string);

const cgem_char_t *cgem_string_get_data(const cgem_string_t *string);
size_t cgem_string_get_length(const cgem_string_t *string);
size_t cgem_string_get_hash(const cgem_string_t *string);

#endif
