#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#endif

#include "cgem/ide_file_tree.h"
#include "cgem/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <limits.h>
#endif

static void free_expand_state(void);
static char *tree_dup_path(const char *path);

static char *tree_dup_path(const char *path)
{
#if !defined(_WIN32)
    char resolved[PATH_MAX];

    if (path && path[0] && realpath(path, resolved)) {
        return strdup(resolved);
    }
#endif
    if (!path) {
        return NULL;
    }
    return strdup(path);
}

char *ide_file_tree_dup_canonical(const char *path)
{
    return tree_dup_path(path);
}

bool ide_file_tree_paths_equal(const char *a, const char *b)
{
#if !defined(_WIN32)
    char abs_a[PATH_MAX];
    char abs_b[PATH_MAX];
#endif

    if (!a || !b) {
        return false;
    }
    if (strcmp(a, b) == 0) {
        return true;
    }
#if !defined(_WIN32)
    if (realpath(a, abs_a) && realpath(b, abs_b)) {
        return strcmp(abs_a, abs_b) == 0;
    }
#endif
    return false;
}

void ide_file_tree_sync_selection(IdeFileTree *tree, const char *current_path)
{
    if (!tree || !current_path || !current_path[0]) {
        return;
    }
    for (size_t i = 0; i < tree->entry_count; i++) {
        if (ide_file_tree_paths_equal(tree->entries[i].full_path,
                                      current_path)) {
            tree->selected = i;
            return;
        }
    }
}

void ide_file_tree_init(IdeFileTree *tree)
{
    tree->entry_count = 0;
    tree->selected = 0;
    tree->scroll = 0;
    tree->dirty = true;
    tree->focused = false;
    free_expand_state();
}

void ide_file_tree_free(IdeFileTree *tree)
{
    for (size_t i = 0; i < tree->entry_count; i++) {
        free(tree->entries[i].name);
        free(tree->entries[i].full_path);
    }
    tree->entry_count = 0;
}

static char **saved_expand_paths = NULL;
static bool *saved_expand_states = NULL;
static size_t saved_expand_count = 0;

static void free_expand_state(void)
{
    if (saved_expand_paths) {
        for (size_t i = 0; i < saved_expand_count; i++) {
            free(saved_expand_paths[i]);
        }
        free(saved_expand_paths);
        saved_expand_paths = NULL;
    }
    free(saved_expand_states);
    saved_expand_states = NULL;
    saved_expand_count = 0;
}

static void save_expand_state(const IdeFileTree *tree)
{
    free_expand_state();
    saved_expand_count = tree->entry_count;
    if (saved_expand_count == 0) {
        return;
    }
    saved_expand_paths = malloc(saved_expand_count * sizeof(char *));
    saved_expand_states = malloc(saved_expand_count * sizeof(bool));
    if (!saved_expand_paths || !saved_expand_states) {
        free_expand_state();
        return;
    }
    for (size_t i = 0; i < saved_expand_count; i++) {
        saved_expand_paths[i] =
            tree->entries[i].full_path
                ? strdup(tree->entries[i].full_path) : NULL;
        saved_expand_states[i] = tree->entries[i].expanded;
    }
}

static bool was_expanded(const char *full_path)
{
    for (size_t i = 0; i < saved_expand_count; i++) {
        if (saved_expand_paths[i] && full_path &&
            ide_file_tree_paths_equal(saved_expand_paths[i], full_path)) {
            return saved_expand_states[i];
        }
    }
    return false;
}

typedef struct {
    char **names;
    char **paths;
    bool *is_dirs;
    size_t count;
    size_t capacity;
} CollectState;

static bool collect_callback(const char *path, void *context)
{
    CollectState *state = (CollectState *) context;
    const char *name;
    size_t name_len;
    size_t path_len;

    name = strrchr(path, '/');
    name = name ? name + 1 : path;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return true;
    }

    if (state->count >= state->capacity) {
        size_t new_cap = state->capacity ? state->capacity * 2 : 64;
        char **new_names = realloc(state->names, new_cap * sizeof(*new_names));
        char **new_paths = realloc(state->paths, new_cap * sizeof(*new_paths));
        bool *new_dirs = realloc(state->is_dirs, new_cap * sizeof(*new_dirs));

        if (!new_names || !new_paths || !new_dirs) {
            free(new_names);
            free(new_paths);
            free(new_dirs);
            return false;
        }
        state->names = new_names;
        state->paths = new_paths;
        state->is_dirs = new_dirs;
        state->capacity = new_cap;
    }

    name_len = strlen(name) + 1;
    state->names[state->count] = malloc(name_len);
    if (!state->names[state->count]) {
        return false;
    }
    memcpy(state->names[state->count], name, name_len);

    path_len = strlen(path) + 1;
    state->paths[state->count] = malloc(path_len);
    if (!state->paths[state->count]) {
        free(state->names[state->count]);
        state->names[state->count] = NULL;
        return false;
    }
    memcpy(state->paths[state->count], path, path_len);

    state->is_dirs[state->count] = platform_path_is_directory(path);
    state->count++;
    return true;
}

static int name_compare(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = (unsigned char) *a;
        int cb = (unsigned char) *b;

        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char) *a - (unsigned char) *b;
}

static void sort_entries(CollectState *state)
{
    if (state->count < 2) {
        return;
    }
    for (size_t i = 0; i < state->count - 1; i++) {
        for (size_t j = i + 1; j < state->count; j++) {
            int swap = 0;

            if (state->is_dirs[i] && !state->is_dirs[j]) {
                swap = -1;
            } else if (!state->is_dirs[i] && state->is_dirs[j]) {
                swap = 1;
            } else {
                swap = name_compare(state->names[i], state->names[j]);
            }
            if (swap > 0) {
                char *tmp_name = state->names[i];
                char *tmp_path = state->paths[i];
                bool tmp_dir = state->is_dirs[i];

                state->names[i] = state->names[j];
                state->paths[i] = state->paths[j];
                state->is_dirs[i] = state->is_dirs[j];
                state->names[j] = tmp_name;
                state->paths[j] = tmp_path;
                state->is_dirs[j] = tmp_dir;
            }
        }
    }
}

static void free_collect_state(CollectState *state)
{
    for (size_t i = 0; i < state->count; i++) {
        free(state->names[i]);
        free(state->paths[i]);
    }
    free(state->names);
    free(state->paths);
    free(state->is_dirs);
    state->names = NULL;
    state->paths = NULL;
    state->is_dirs = NULL;
    state->count = 0;
    state->capacity = 0;
}

static int add_entry_to_tree(IdeFileTree *tree, const char *name,
                              const char *full_path, bool is_directory,
                              size_t depth, bool expanded)
{
    if (tree->entry_count >= FILE_TREE_MAX_ENTRIES) {
        return -1;
    }
    FileTreeEntry *entry = &tree->entries[tree->entry_count];

    entry->name = strdup(name);
    entry->full_path = tree_dup_path(full_path);
    if (!entry->name || !entry->full_path) {
        free(entry->name);
        entry->name = NULL;
        free(entry->full_path);
        entry->full_path = NULL;
        return -1;
    }
    entry->depth = depth;
    entry->is_directory = is_directory;
    entry->expanded = expanded;
    entry->has_children = false;
    tree->entry_count++;
    return 0;
}

static void scan_directory(IdeFileTree *tree, const char *dir_path,
                            size_t depth)
{
    CollectState state = {0};

    if (!platform_scan_directory(dir_path, collect_callback, &state)) {
        free_collect_state(&state);
        return;
    }
    sort_entries(&state);

    for (size_t i = 0; i < state.count; i++) {
        if (state.is_dirs[i]) {
            bool expanded = was_expanded(state.paths[i]);

            if (add_entry_to_tree(tree, state.names[i], state.paths[i],
                                    true, depth, expanded) == 0) {
                tree->entries[tree->entry_count - 1].has_children = true;
            }
            if (expanded) {
                scan_directory(tree, state.paths[i], depth + 1);
            }
        } else {
            add_entry_to_tree(tree, state.names[i], state.paths[i],
                               false, depth, false);
        }
    }

    free_collect_state(&state);
}

static const char *path_basename(const char *path)
{
    const char *slash;

    if (!path) {
        return NULL;
    }
    slash = strrchr(path, '/');
    if (!slash) {
#if defined(_WIN32)
        slash = strrchr(path, '\\');
#endif
    }
    return slash ? slash + 1 : path;
}

static bool paths_equal(const char *a, const char *b)
{
    return ide_file_tree_paths_equal(a, b);
}

static void add_output_root(IdeFileTree *tree, const char *path,
                            bool restore_expand_state)
{
    const char *name;
    bool expanded;
    bool is_directory;

    if (!path || !path[0]) {
        return;
    }
    is_directory = platform_path_is_directory(path);
    name = path_basename(path);
    if (!name || !name[0]) {
        name = path;
    }
    expanded = is_directory &&
               (restore_expand_state ? was_expanded(path) : true);
    if (add_entry_to_tree(tree, name, path, is_directory, 0, expanded) != 0) {
        return;
    }
    if (is_directory) {
        tree->entries[tree->entry_count - 1].has_children = true;
        if (expanded) {
            scan_directory(tree, path, 1);
        }
    }
}

static void add_input_file(IdeFileTree *tree, const char *path)
{
    const char *name;

    if (!path || !path[0]) {
        return;
    }
    name = path_basename(path);
    if (!name || !name[0]) {
        name = path;
    }
    add_entry_to_tree(tree, name, path, false, 0, false);
}

void ide_file_tree_refresh(IdeFileTree *tree, const char *input_path,
                           const char *include_path, const char *source_path,
                           const char *current_path)
{
    size_t old_selected_path_len = 0;
    char *old_selected_path = NULL;
    bool restore_expand_state;

    if ((!input_path || !input_path[0]) &&
        (!include_path || !include_path[0]) &&
        (!source_path || !source_path[0])) {
        return;
    }

    if (tree->selected < tree->entry_count &&
        tree->entries[tree->selected].full_path) {
        old_selected_path_len =
            strlen(tree->entries[tree->selected].full_path) + 1;
        old_selected_path = malloc(old_selected_path_len);
        if (old_selected_path) {
            memcpy(old_selected_path,
                   tree->entries[tree->selected].full_path,
                   old_selected_path_len);
        }
    }

    restore_expand_state = tree->entry_count > 0;
    save_expand_state(tree);
    ide_file_tree_free(tree);
    add_input_file(tree, input_path);
    add_output_root(tree, include_path, restore_expand_state);
    if (!paths_equal(include_path, source_path)) {
        add_output_root(tree, source_path, restore_expand_state);
    }
    free_expand_state();

    if (tree->entry_count > 0) {
        tree->selected = 0;
        if (old_selected_path) {
            for (size_t i = 0; i < tree->entry_count; i++) {
                if (tree->entries[i].full_path &&
                    ide_file_tree_paths_equal(tree->entries[i].full_path,
                                              old_selected_path)) {
                    tree->selected = i;
                    break;
                }
            }
        } else if (current_path && current_path[0]) {
            ide_file_tree_sync_selection(tree, current_path);
        }
        if (tree->selected >= tree->entry_count) {
            tree->selected = tree->entry_count - 1;
        }
    }

    free(old_selected_path);
    tree->dirty = false;
}

bool ide_file_tree_select_up(IdeFileTree *tree)
{
    if (tree->selected == 0) {
        return false;
    }
    tree->selected--;
    return true;
}

bool ide_file_tree_select_down(IdeFileTree *tree)
{
    if (tree->entry_count == 0 ||
        tree->selected >= tree->entry_count - 1) {
        return false;
    }
    tree->selected++;
    return true;
}

void ide_file_tree_toggle_expand(IdeFileTree *tree)
{
    if (tree->selected >= tree->entry_count) {
        return;
    }
    FileTreeEntry *entry = &tree->entries[tree->selected];

    if (!entry->is_directory) {
        return;
    }
    entry->expanded = !entry->expanded;
    tree->dirty = true;
}

const char *ide_file_tree_selected_path(const IdeFileTree *tree)
{
    if (tree->selected >= tree->entry_count) {
        return NULL;
    }
    return tree->entries[tree->selected].full_path;
}

bool ide_file_tree_selected_is_directory(const IdeFileTree *tree)
{
    if (tree->selected >= tree->entry_count) {
        return false;
    }
    return tree->entries[tree->selected].is_directory;
}
