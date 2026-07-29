#ifndef CGEM_CURSOR_H
#define CGEM_CURSOR_H

#include "cgem/core/node.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct cgem_cursor cgem_cursor_t;

cgem_cursor_t *cgem_cursor_new(cgem_node_t *root);
void cgem_cursor_free(cgem_cursor_t *cursor);

bool cgem_cursor_select(cgem_cursor_t *cursor, size_t index);
bool cgem_cursor_unselect(cgem_cursor_t *cursor);
cgem_node_t *cgem_cursor_get_selected(const cgem_cursor_t *cursor);

#endif
