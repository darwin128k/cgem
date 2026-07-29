#include "cgem/core/resolver.h"
#include "cgem/core/symbol.h"

#include <string.h>

cgem_node_t *cgem_resolve(cgem_node_t *root, const cgem_char_t *path)
{
    const cgem_char_t *segment_start;
    cgem_node_t *current;

    if (!root || !path || !*path) {
        return NULL;
    }

    current = root;
    segment_start = path;
    while (*segment_start) {
        const cgem_char_t *segment_end = segment_start;
        size_t segment_length;
        size_t count;
        size_t i;
        cgem_node_t *found = NULL;

        while (*segment_end && *segment_end != '.') {
            segment_end++;
        }
        segment_length = (size_t) (segment_end - segment_start);
        if (segment_length == 0) {
            return NULL;
        }

        count = cgem_node_get_count(current);
        for (i = 0; i < count; i++) {
            cgem_node_t *child = cgem_node_get(current, i);
            const cgem_char_t *name = cgem_symbol_get_name(child);

            if (name && strlen(name) == segment_length &&
                strncmp(name, segment_start, segment_length) == 0) {
                found = child;
                break;
            }
        }
        if (!found) {
            return NULL;
        }
        current = found;

        segment_start = segment_end;
        if (*segment_start == '.') {
            segment_start++;
            if (!*segment_start) {
                /* trailing dot ("c.int.") is not a valid path */
                return NULL;
            }
        }
    }
    return current;
}
