#ifndef CGEM_IDE_MENU_H
#define CGEM_IDE_MENU_H

#include <stdbool.h>
#include <stddef.h>

#include "cgem/theme.h"

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} IdeMenuBuffer;

typedef enum {
    IDE_MENU_ACTION_NONE = 0,
    IDE_MENU_ACTION_OPEN,
    IDE_MENU_ACTION_SAVE,
    IDE_MENU_ACTION_SAVE_AS,
    IDE_MENU_ACTION_QUIT,
    IDE_MENU_ACTION_COPY,
    IDE_MENU_ACTION_CUT,
    IDE_MENU_ACTION_PASTE,
    IDE_MENU_ACTION_PASTE_FORMATTED,
    IDE_MENU_ACTION_SELECT_ALL,
    IDE_MENU_ACTION_UNDO,
    IDE_MENU_ACTION_REDO,
    IDE_MENU_ACTION_FIND,
    IDE_MENU_ACTION_FIND_NEXT,
    IDE_MENU_ACTION_GOTO_LINE,
    IDE_MENU_ACTION_GOTO_DEFINITION,
    IDE_MENU_ACTION_RENAME,
    IDE_MENU_ACTION_FORMAT,
    IDE_MENU_ACTION_INDENT,
    IDE_MENU_ACTION_UNINDENT,
    IDE_MENU_ACTION_THEME,
    IDE_MENU_ACTION_HELP,
    IDE_MENU_ACTION_GENERATE
} IdeMenuAction;

#define IDE_MENU_BAR_COUNT 5
#define IDE_MENU_FILE 0
#define IDE_MENU_EDIT 1
#define IDE_MENU_BUILD 2
#define IDE_MENU_OPTIONS 3
#define IDE_MENU_HELP 4

typedef struct {
    bool open;
    int bar_index;
    int item_index;
    int bar_x[IDE_MENU_BAR_COUNT];
    int bar_width[IDE_MENU_BAR_COUNT];
} IdeMenu;

typedef struct {
    IdeMenuAction action;
    bool handled;
    bool quit_request;
} IdeMenuKeyResult;

#define IDE_MENU_BAR_ROW 2

void ide_menu_init(IdeMenu *menu);
void ide_menu_close(IdeMenu *menu);
IdeMenuAction ide_menu_open_bar(IdeMenu *menu, int bar_index);
bool ide_menu_is_open(const IdeMenu *menu);

IdeMenuKeyResult ide_menu_handle_key(IdeMenu *menu, int key);
bool ide_menu_handle_mouse(IdeMenu *menu, int row, int col, bool down,
                           int screen_cols, IdeMenuAction *action);

void ide_menu_draw_bar(IdeMenuBuffer *buffer, const EditorTheme *palette,
                       int screen_cols, const char *filename, bool dirty,
                       IdeMenu *menu);
void ide_menu_draw_popup(IdeMenuBuffer *buffer, const EditorTheme *palette,
                         int screen_cols, int screen_rows, const IdeMenu *menu);

#endif
