#include "cgem/core/cursor.h"

#include "cgem/core/allocator.h"
#include "cgem/core/array.h"

struct cgem_cursor {
    cgem_array_t stack;
};

static cgem_bool_t push_selected(cgem_cursor_t *cursor, cgem_node_t *node)
{
    return cgem_array_push_back(&cursor->stack, &node);
}

cgem_cursor_t *cgem_cursor_new(cgem_node_t *root)
{
    cgem_cursor_t *cursor;

    if (!root) {
        return NULL;
    }
    cursor = cgem_alloc(sizeof(*cursor));
    if (!cursor) {
        return NULL;
    }
    cgem_array_init(&cursor->stack, 0, sizeof(cgem_node_t *));
    if (!push_selected(cursor, root)) {
        cgem_array_deinit(&cursor->stack);
        cgem_free(cursor);
        return NULL;
    }
    return cursor;
}

void cgem_cursor_free(cgem_cursor_t *cursor)
{
    if (!cursor) {
        return;
    }
    cgem_array_deinit(&cursor->stack);
    cgem_free(cursor);
}

cgem_bool_t cgem_cursor_select(cgem_cursor_t *cursor, size_t index)
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

cgem_bool_t cgem_cursor_unselect(cgem_cursor_t *cursor)
{
    if (!cursor || cgem_array_size(&cursor->stack) <= 1) {
        return false;
    }
    cgem_array_pop_back(&cursor->stack);
    return true;
}

cgem_node_t *cgem_cursor_get_selected(const cgem_cursor_t *cursor)
{
    cgem_node_t **slot;

    if (!cursor || cgem_array_size(&cursor->stack) == 0) {
        return NULL;
    }
    slot = cgem_array_at(&cursor->stack, cgem_array_size(&cursor->stack) - 1);
    return slot ? *slot : NULL;
}
