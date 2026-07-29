#include "cgem/core/cursor.h"

#include <stdlib.h>

struct cgem_cursor {
    cgem_node_t **stack;
    size_t count;
    size_t capacity;
};

static bool push_selected(cgem_cursor_t *cursor, cgem_node_t *node)
{
    if (cursor->count == cursor->capacity) {
        size_t capacity = cursor->capacity ? cursor->capacity * 2 : 4;
        cgem_node_t **stack = realloc(cursor->stack, capacity * sizeof(*stack));

        if (!stack) {
            return false;
        }
        cursor->stack = stack;
        cursor->capacity = capacity;
    }
    cursor->stack[cursor->count++] = node;
    return true;
}

cgem_cursor_t *cgem_cursor_new(cgem_node_t *root)
{
    cgem_cursor_t *cursor;

    if (!root) {
        return NULL;
    }
    cursor = calloc(1, sizeof(*cursor));
    if (!cursor) {
        return NULL;
    }
    if (!push_selected(cursor, root)) {
        free(cursor);
        return NULL;
    }
    return cursor;
}

void cgem_cursor_free(cgem_cursor_t *cursor)
{
    if (!cursor) {
        return;
    }
    free(cursor->stack);
    free(cursor);
}

bool cgem_cursor_select(cgem_cursor_t *cursor, size_t index)
{
    cgem_node_t *current;
    cgem_node_t *child;

    if (!cursor) {
        return false;
    }
    current = cgem_cursor_get_selected(cursor);
    if (!current) {
        return false;
    }
    child = cgem_node_get(current, index);
    if (!child) {
        return false;
    }
    return push_selected(cursor, child);
}

bool cgem_cursor_unselect(cgem_cursor_t *cursor)
{
    if (!cursor || cursor->count <= 1) {
        return false;
    }
    cursor->count--;
    return true;
}

cgem_node_t *cgem_cursor_get_selected(const cgem_cursor_t *cursor)
{
    if (!cursor || cursor->count == 0) {
        return NULL;
    }
    return cursor->stack[cursor->count - 1];
}
