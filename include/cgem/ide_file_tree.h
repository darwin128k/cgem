#ifndef CGEM_IDE_FILE_TREE_H
#define CGEM_IDE_FILE_TREE_H

#include <stdbool.h>
#include <stddef.h>

#define SIDEBAR_WIDTH 30
#define FILE_TREE_MAX_ENTRIES 4096

typedef struct {
    char *name;
    char *full_path;
    size_t depth;
    bool is_directory;
    bool expanded;
    bool has_children;
} FileTreeEntry;

typedef struct {
    FileTreeEntry entries[FILE_TREE_MAX_ENTRIES];
    size_t entry_count;
    size_t selected;
    size_t scroll;
    bool dirty;
    bool focused;
} IdeFileTree;

void ide_file_tree_init(IdeFileTree *tree);
void ide_file_tree_free(IdeFileTree *tree);
bool ide_file_tree_paths_equal(const char *a, const char *b);
char *ide_file_tree_dup_canonical(const char *path);
void ide_file_tree_sync_selection(IdeFileTree *tree, const char *current_path);
void ide_file_tree_refresh(IdeFileTree *tree, const char *input_path,
                           const char *include_path, const char *source_path,
                           const char *current_path);
bool ide_file_tree_select_up(IdeFileTree *tree);
bool ide_file_tree_select_down(IdeFileTree *tree);
void ide_file_tree_toggle_expand(IdeFileTree *tree);
const char *ide_file_tree_selected_path(const IdeFileTree *tree);
bool ide_file_tree_selected_is_directory(const IdeFileTree *tree);

#endif
