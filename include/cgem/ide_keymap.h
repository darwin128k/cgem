#ifndef CGEM_IDE_KEYMAP_H
#define CGEM_IDE_KEYMAP_H

#include <stddef.h>

typedef enum {
    IDE_KEY_NONE = 0,
    IDE_KEY_MENU_FILE,
    IDE_KEY_MENU_EDIT,
    IDE_KEY_MENU_BUILD,
    IDE_KEY_MENU_OPTIONS,
    IDE_KEY_MENU_HELP,
    IDE_KEY_OPEN,
    IDE_KEY_SAVE,
    IDE_KEY_GENERATE,
    IDE_KEY_THEME,
    IDE_KEY_QUIT,
    IDE_KEY_COPY,
    IDE_KEY_CUT,
    IDE_KEY_PASTE,
    IDE_KEY_PASTE_FORMATTED,
    IDE_KEY_SELECT_ALL,
    IDE_KEY_UNDO,
    IDE_KEY_REDO,
    IDE_KEY_FIND,
    IDE_KEY_FIND_NEXT,
    IDE_KEY_GOTO_LINE,
    IDE_KEY_FORMAT,
    IDE_KEY_HELP
} IdeKeyAction;

void ide_keymap_init(const char *workspace_root, const char *include_path);
IdeKeyAction ide_keymap_lookup(int key);
int ide_keymap_key_for(IdeKeyAction action);
const char *ide_keymap_key_label(IdeKeyAction action);

#endif
