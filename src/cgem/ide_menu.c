#define _POSIX_C_SOURCE 200809L

#include "cgem/ide_menu.h"

#include "cgem/ide_keymap.h"
#include "cgem/platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *label;
    IdeMenuAction action;
    bool section;
    bool enabled;
} MenuEntry;

typedef struct {
    const char *title;
    const MenuEntry *items;
    size_t count;
} MenuBar;

static const MenuEntry file_items[] = {
    { "File", IDE_MENU_ACTION_NONE, true, false },
    { "Open", IDE_MENU_ACTION_OPEN, false, true },
    { "Save", IDE_MENU_ACTION_SAVE, false, true },
    { "Save As", IDE_MENU_ACTION_SAVE_AS, false, true },
    { "Session", IDE_MENU_ACTION_NONE, true, false },
    { "Quit", IDE_MENU_ACTION_QUIT, false, true },
};

static const MenuEntry edit_items[] = {
    { "History", IDE_MENU_ACTION_NONE, true, false },
    { "Undo", IDE_MENU_ACTION_UNDO, false, true },
    { "Redo", IDE_MENU_ACTION_REDO, false, true },
    { "Clipboard", IDE_MENU_ACTION_NONE, true, false },
    { "Copy", IDE_MENU_ACTION_COPY, false, true },
    { "Cut", IDE_MENU_ACTION_CUT, false, true },
    { "Paste", IDE_MENU_ACTION_PASTE, false, true },
    { "Paste Formatted", IDE_MENU_ACTION_PASTE_FORMATTED, false, true },
    { "Select All", IDE_MENU_ACTION_SELECT_ALL, false, true },
    { "Section", IDE_MENU_ACTION_NONE, true, false },
    { "Increase", IDE_MENU_ACTION_INDENT, false, true },
    { "Decrease", IDE_MENU_ACTION_UNINDENT, false, true },
    { "Format", IDE_MENU_ACTION_FORMAT, false, true },
    { "Search", IDE_MENU_ACTION_NONE, true, false },
    { "Find", IDE_MENU_ACTION_FIND, false, true },
    { "Find Next", IDE_MENU_ACTION_FIND_NEXT, false, true },
    { "Go to Line", IDE_MENU_ACTION_GOTO_LINE, false, true },
};

static const MenuEntry build_items[] = {
    { "Build", IDE_MENU_ACTION_NONE, true, false },
    { "Generate", IDE_MENU_ACTION_GENERATE, false, true },
};

static const MenuEntry options_items[] = {
    { "Customize", IDE_MENU_ACTION_NONE, true, false },
    { "Theme", IDE_MENU_ACTION_THEME, false, true },
};

static const MenuBar menus[] = {
    { "File", file_items, sizeof(file_items) / sizeof(file_items[0]) },
    { "Edit", edit_items, sizeof(edit_items) / sizeof(edit_items[0]) },
    { "Build", build_items, sizeof(build_items) / sizeof(build_items[0]) },
    { "Options", options_items,
      sizeof(options_items) / sizeof(options_items[0]) },
    { "Help", NULL, 0 },
};

static void buffer_append(IdeMenuBuffer *buffer, const char *data, size_t length)
{
    size_t needed = buffer->length + length;

    if (needed > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 256;

        while (capacity < needed) {
            capacity *= 2;
        }
        {
            char *grown = realloc(buffer->data, capacity);

            if (!grown) {
                return;
            }
            buffer->data = grown;
            buffer->capacity = capacity;
        }
    }
    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
}

static void buffer_printf(IdeMenuBuffer *buffer, const char *format, ...)
{
    char text[256];
    va_list args;
    int length;

    va_start(args, format);
    length = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (length > 0) {
        size_t used = (size_t) length < sizeof(text) ? (size_t) length
                                                     : sizeof(text) - 1;
        buffer_append(buffer, text, used);
    }
}

static int text_width(const char *text)
{
    return text ? (int) strlen(text) : 0;
}

static const MenuBar *menu_bar(int index)
{
    if (index < 0 || index >= (int) (sizeof(menus) / sizeof(menus[0]))) {
        return NULL;
    }
    return &menus[index];
}

static const MenuEntry *menu_entry(const MenuBar *bar, int index)
{
    if (!bar || index < 0 || (size_t) index >= bar->count) {
        return NULL;
    }
    return &bar->items[index];
}

static const char *menu_action_shortcut(IdeMenuAction action)
{
    switch (action) {
    case IDE_MENU_ACTION_SAVE:
        return ide_keymap_key_label(IDE_KEY_SAVE);
    case IDE_MENU_ACTION_QUIT:
        return ide_keymap_key_label(IDE_KEY_QUIT);
    case IDE_MENU_ACTION_COPY:
        return ide_keymap_key_label(IDE_KEY_COPY);
    case IDE_MENU_ACTION_CUT:
        return ide_keymap_key_label(IDE_KEY_CUT);
    case IDE_MENU_ACTION_PASTE:
        return ide_keymap_key_label(IDE_KEY_PASTE);
    case IDE_MENU_ACTION_PASTE_FORMATTED:
        return ide_keymap_key_label(IDE_KEY_PASTE_FORMATTED);
    case IDE_MENU_ACTION_SELECT_ALL:
        return ide_keymap_key_label(IDE_KEY_SELECT_ALL);
    case IDE_MENU_ACTION_UNDO:
        return ide_keymap_key_label(IDE_KEY_UNDO);
    case IDE_MENU_ACTION_REDO:
        return ide_keymap_key_label(IDE_KEY_REDO);
    case IDE_MENU_ACTION_FIND:
        return ide_keymap_key_label(IDE_KEY_FIND);
    case IDE_MENU_ACTION_FIND_NEXT:
        return ide_keymap_key_label(IDE_KEY_FIND_NEXT);
    case IDE_MENU_ACTION_GOTO_LINE:
        return ide_keymap_key_label(IDE_KEY_GOTO_LINE);
    case IDE_MENU_ACTION_FORMAT:
        return ide_keymap_key_label(IDE_KEY_FORMAT);
    case IDE_MENU_ACTION_INDENT:
        return "Tab";
    case IDE_MENU_ACTION_UNINDENT:
        return "Shift+Tab";
    case IDE_MENU_ACTION_THEME:
        break;
    case IDE_MENU_ACTION_GENERATE:
        return ide_keymap_key_label(IDE_KEY_GENERATE);
    case IDE_MENU_ACTION_HELP:
        return ide_keymap_key_label(IDE_KEY_HELP);
    case IDE_MENU_ACTION_OPEN:
        return ide_keymap_key_label(IDE_KEY_OPEN);
    case IDE_MENU_ACTION_SAVE_AS:
    case IDE_MENU_ACTION_NONE:
        break;
    }
    return "";
}

static int popup_width(const MenuBar *bar)
{
    int width = 14;

    if (!bar) {
        return width;
    }
    for (size_t i = 0; i < bar->count; i++) {
        const MenuEntry *item = &bar->items[i];
        const char *shortcut = menu_action_shortcut(item->action);
        int shortcut_width = (int) strlen(shortcut);
        int row_width;

        if (item->section) {
            row_width = item->label ? (int) strlen(item->label) + 2 : 0;
            if (row_width > width) {
                width = row_width;
            }
            continue;
        }
        row_width = (int) strlen(item->label) + 6 + shortcut_width;
        if (row_width > width) {
            width = row_width;
        }
    }
    return width + 2;
}

static int popup_height(const MenuBar *bar)
{
    if (!bar) {
        return 0;
    }
    return (int) bar->count;
}

static int next_item(const MenuBar *bar, int index, int direction)
{
    int next = index;
    int attempts = 0;

    if (!bar || bar->count == 0) {
        return 0;
    }
    do {
        next += direction;
        if (next < 0) {
            next = (int) bar->count - 1;
        } else if (next >= (int) bar->count) {
            next = 0;
        }
        attempts++;
        if (attempts > (int) bar->count) {
            return index >= 0 ? index : 0;
        }
    } while (bar->items[next].section ||
             (!bar->items[next].enabled && next != index));
    return next;
}

static void menu_bar_track(IdeMenu *menu, int *cursor, int index, int width)
{
    if (!menu || index < 0 || index >= IDE_MENU_BAR_COUNT) {
        return;
    }
    menu->bar_x[index] = *cursor + 1;
    menu->bar_width[index] = width;
    *cursor += width;
}

void ide_menu_draw_bar(IdeMenuBuffer *buffer, const EditorTheme *palette,
                       int screen_cols, const char *filename, bool dirty,
                       IdeMenu *menu)
{
    char right[256];
    int cursor = 0;
    int right_width;
    int spaces;

    if (!buffer || !palette || !menu) {
        return;
    }

    snprintf(right, sizeof(right), " %s%s ",
             filename ? filename : "[No Name]", dirty ? " *" : "");
    right_width = text_width(right);

    buffer_append(buffer, palette->header, strlen(palette->header));
    for (size_t i = 0; i < sizeof(menus) / sizeof(menus[0]); i++) {
        bool active = menu->open && menu->bar_index == (int) i;
        int segment = 1 + (int) strlen(menus[i].title) + 1;

        menu_bar_track(menu, &cursor, (int) i, segment);
        if (active) {
            buffer_append(buffer, palette->menu_active,
                          strlen(palette->menu_active));
        } else {
            buffer_append(buffer, palette->header, strlen(palette->header));
        }
        buffer_append(buffer, " ", 1);
        buffer_append(buffer, menus[i].title, strlen(menus[i].title));
        buffer_append(buffer, " ", 1);
    }

    spaces = screen_cols - cursor - right_width;
    if (spaces < 0) {
        spaces = 0;
    }
    buffer_append(buffer, palette->header, strlen(palette->header));
    while (spaces-- > 0) {
        buffer_append(buffer, " ", 1);
    }
    if (cursor + right_width <= screen_cols) {
        buffer_append(buffer, right, strlen(right));
    }
    buffer_append(buffer, "\x1b[0m", 4);
}

void ide_menu_init(IdeMenu *menu)
{
    if (!menu) {
        return;
    }
    *menu = (IdeMenu) {0};
}

void ide_menu_close(IdeMenu *menu)
{
    if (!menu) {
        return;
    }
    if (menu->open) {
        platform_input_flush();
    }
    menu->open = false;
    menu->bar_index = -1;
    menu->item_index = 0;
}

static void open_bar(IdeMenu *menu, int bar_index)
{
    const MenuBar *bar = menu_bar(bar_index);

    if (!bar || bar->count == 0) {
        return;
    }
    menu->open = true;
    menu->bar_index = bar_index;
    menu->item_index = next_item(bar, -1, 1);
}

IdeMenuAction ide_menu_open_bar(IdeMenu *menu, int bar_index)
{
    if (!menu) {
        return IDE_MENU_ACTION_NONE;
    }
    if (bar_index == IDE_MENU_HELP) {
        ide_menu_close(menu);
        return IDE_MENU_ACTION_HELP;
    }
    if (menu->open && menu->bar_index == bar_index) {
        ide_menu_close(menu);
        return IDE_MENU_ACTION_NONE;
    }
    open_bar(menu, bar_index);
    return IDE_MENU_ACTION_NONE;
}

bool ide_menu_is_open(const IdeMenu *menu)
{
    return menu && menu->open;
}

static IdeMenuAction action_at(const IdeMenu *menu)
{
    const MenuBar *bar;
    const MenuEntry *item;

    if (!menu || !menu->open || menu->bar_index < 0) {
        return IDE_MENU_ACTION_NONE;
    }
    bar = menu_bar(menu->bar_index);
    item = menu_entry(bar, menu->item_index);
    if (!item || item->section || !item->enabled) {
        return IDE_MENU_ACTION_NONE;
    }
    return item->action;
}

IdeMenuKeyResult ide_menu_handle_key(IdeMenu *menu, int key)
{
    IdeMenuKeyResult result = {IDE_MENU_ACTION_NONE, false, false};
    const MenuBar *bar;

    if (!menu) {
        return result;
    }

    if (!menu->open) {
        return result;
    }

    result.handled = true;
    bar = menu_bar(menu->bar_index);
    if (!bar) {
        ide_menu_close(menu);
        return result;
    }

    switch (key) {
    case 27:
        ide_menu_close(menu);
        break;
    case KEY_ARROW_UP:
        menu->item_index = next_item(bar, menu->item_index, -1);
        break;
    case KEY_ARROW_DOWN:
        menu->item_index = next_item(bar, menu->item_index, 1);
        break;
    case KEY_ARROW_LEFT:
        menu->bar_index =
            (menu->bar_index + IDE_MENU_HELP - 1) % IDE_MENU_HELP;
        menu->item_index = next_item(menu_bar(menu->bar_index), -1, 1);
        break;
    case KEY_ARROW_RIGHT:
        menu->bar_index = (menu->bar_index + 1) % IDE_MENU_HELP;
        menu->item_index = next_item(menu_bar(menu->bar_index), -1, 1);
        break;
    case '\r':
        result.action = action_at(menu);
        if (result.action == IDE_MENU_ACTION_QUIT) {
            result.quit_request = true;
        }
        ide_menu_close(menu);
        break;
    default:
        result.handled = false;
        break;
    }
    return result;
}

static int popup_origin_col(const IdeMenu *menu, const MenuBar *bar,
                            int screen_cols)
{
    int width = popup_width(bar);
    int origin_col = menu->bar_x[menu->bar_index];

    if (origin_col < 1) {
        origin_col = 1;
    }
    if (origin_col + width > screen_cols) {
        origin_col = screen_cols - width;
        if (origin_col < 1) {
            origin_col = 1;
        }
    }
    return origin_col;
}

static bool point_in_popup(const IdeMenu *menu, int row, int col, int screen_cols,
                           int *item_index)
{
    const MenuBar *bar;
    int width;
    int height;
    int origin_row;
    int origin_col;

    if (!menu || !menu->open || menu->bar_index < 0 ||
        menu->bar_index >= (int) (sizeof(menus) / sizeof(menus[0]))) {
        return false;
    }
    bar = menu_bar(menu->bar_index);
    width = popup_width(bar);
    height = popup_height(bar);
    origin_row = IDE_MENU_BAR_ROW + 1;
    origin_col = popup_origin_col(menu, bar, screen_cols);

    if (row < origin_row || row >= origin_row + height || col < origin_col ||
        col >= origin_col + width) {
        return false;
    }
    {
        int inner_row = row - origin_row;
        size_t i;

        for (i = 0; i < bar->count; i++) {
            if ((int) i == inner_row) {
                if (bar->items[i].section) {
                    return false;
                }
                if (item_index) {
                    *item_index = (int) i;
                }
                return true;
            }
        }
    }
    return false;
}

bool ide_menu_handle_mouse(IdeMenu *menu, int row, int col, bool down,
                           int screen_cols, IdeMenuAction *action)
{
    int bar_index = -1;

    if (!menu || !down) {
        return false;
    }

    if (row == IDE_MENU_BAR_ROW) {
        for (size_t i = 0; i < sizeof(menus) / sizeof(menus[0]); i++) {
            int start = menu->bar_x[i];
            int end = start + menu->bar_width[i];

            if (col >= start && col < end) {
                bar_index = (int) i;
                break;
            }
        }
        if (bar_index >= 0) {
            if (bar_index == IDE_MENU_HELP) {
                if (action) {
                    *action = IDE_MENU_ACTION_HELP;
                }
                ide_menu_close(menu);
                return true;
            }
            if (menu->open && menu->bar_index == bar_index) {
                ide_menu_close(menu);
            } else {
                open_bar(menu, bar_index);
            }
            return true;
        }
        if (menu->open) {
            ide_menu_close(menu);
        }
        return true;
    }

    if (menu->open) {
        int item_index = 0;

        if (point_in_popup(menu, row, col, screen_cols, &item_index)) {
            const MenuBar *bar = menu_bar(menu->bar_index);
            const MenuEntry *item = menu_entry(bar, item_index);

            menu->item_index = item_index;
            if (item && item->enabled && !item->section) {
                if (action) {
                    *action = item->action;
                }
                ide_menu_close(menu);
                return true;
            }
            return true;
        }
        ide_menu_close(menu);
        return true;
    }

    return false;
}

void ide_menu_draw_popup(IdeMenuBuffer *buffer, const EditorTheme *palette,
                         int screen_cols, int screen_rows, const IdeMenu *menu)
{
    const MenuBar *bar;
    int width;
    int height;
    int origin_row;
    int origin_col;

    if (!buffer || !palette || !menu || !menu->open || menu->bar_index < 0) {
        return;
    }

    bar = menu_bar(menu->bar_index);
    width = popup_width(bar);
    height = popup_height(bar);
    origin_row = IDE_MENU_BAR_ROW + 1;
    origin_col = popup_origin_col(menu, bar, screen_cols);
    if (origin_row + height > screen_rows) {
        return;
    }

    for (int row = 0; row < height; row++) {
        const MenuEntry *item = &bar->items[row];
        bool selected = menu->item_index == row;
        const char *row_color =
            selected ? palette->header : palette->menu_active;

        buffer_printf(buffer, "\x1b[%d;%dH", origin_row + row, origin_col);

        if (item->section) {
            int label_width = item->label ? (int) strlen(item->label) : 0;
            int pad;

            buffer_append(buffer, palette->menu_section,
                          strlen(palette->menu_section));
            buffer_append(buffer, " ", 1);
            if (label_width > 0) {
                buffer_append(buffer, item->label, (size_t) label_width);
            }
            pad = width - 1 - label_width;
            while (pad-- > 0) {
                buffer_append(buffer, " ", 1);
            }
            continue;
        }

        buffer_append(buffer, row_color, strlen(row_color));
        {
            const char *shortcut = menu_action_shortcut(item->action);
            int label_width = (int) strlen(item->label);
            int shortcut_width = (int) strlen(shortcut);
            int pad = width - 2 - label_width - shortcut_width;

            if (pad < 0) {
                pad = 0;
            }
            buffer_append(buffer, " ", 1);
            buffer_append(buffer, item->label, (size_t) label_width);
            while (pad-- > 0) {
                buffer_append(buffer, " ", 1);
            }
            if (shortcut_width > 0) {
                buffer_append(buffer, shortcut, (size_t) shortcut_width);
            }
            buffer_append(buffer, " ", 1);
        }
    }
    buffer_append(buffer, "\x1b[0m", 4);
}
