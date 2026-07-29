#include "cgem/core/string.h"

#include "cgem/core/allocator.h"

#include <string.h>

struct cgem_string {
    size_t length;
    size_t hash;
    cgem_char_t data[];
};

static size_t hash_bytes(const cgem_char_t *data, size_t length)
{
    size_t hash = 5381;
    size_t i;

    for (i = 0; i < length; i++) {
        hash = hash * 33 + (cgem_uchar_t) data[i];
    }
    return hash;
}

cgem_string_t *cgem_string_new(const cgem_char_t *text)
{
    size_t length;
    cgem_string_t *string;

    if (!text) {
        return NULL;
    }
    length = strlen(text);
    string = cgem_alloc(sizeof(cgem_string_t) + length + 1);
    if (!string) {
        return NULL;
    }
    string->length = length;
    memcpy(string->data, text, length + 1);
    string->hash = hash_bytes(string->data, length);
    return string;
}

void cgem_string_free(cgem_string_t *string)
{
    cgem_free(string);
}

const cgem_char_t *cgem_string_get_data(const cgem_string_t *string)
{
    return string ? string->data : NULL;
}

size_t cgem_string_get_length(const cgem_string_t *string)
{
    return string ? string->length : 0;
}

size_t cgem_string_get_hash(const cgem_string_t *string)
{
    return string ? string->hash : 0;
}
