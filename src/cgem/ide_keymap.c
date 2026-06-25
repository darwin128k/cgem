#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "cgem/ide_keymap.h"

#include "cgem/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    IdeKeyAction action;
    int default_key;
} KeyActionSpec;

static const KeyActionSpec action_specs[] = {
    { "menu.file", IDE_KEY_MENU_FILE, KEY_F2 },
    { "menu.edit", IDE_KEY_MENU_EDIT, KEY_F3 },
    { "menu.build", IDE_KEY_MENU_BUILD, KEY_F4 },
    { "menu.options", IDE_KEY_MENU_OPTIONS, KEY_F5 },
    { "menu.help", IDE_KEY_MENU_HELP, KEY_F6 },
    { "action.open", IDE_KEY_OPEN, 15 },
    { "action.save", IDE_KEY_SAVE, 19 },
    { "action.generate", IDE_KEY_GENERATE, 2 },
    { "action.theme", IDE_KEY_THEME, 0 },
    { "action.quit", IDE_KEY_QUIT, 17 },
    { "action.copy", IDE_KEY_COPY, 3 },
    { "action.cut", IDE_KEY_CUT, 24 },
    { "action.paste", IDE_KEY_PASTE, 22 },
    { "action.paste_formatted", IDE_KEY_PASTE_FORMATTED, 21 },
    { "action.select_all", IDE_KEY_SELECT_ALL, 1 },
    { "action.undo", IDE_KEY_UNDO, 26 },
    { "action.redo", IDE_KEY_REDO, 25 },
    { "action.find", IDE_KEY_FIND, 6 },
    { "action.find_next", IDE_KEY_FIND_NEXT, 14 },
    { "action.goto_line", IDE_KEY_GOTO_LINE, 7 },
    { "action.goto_definition", IDE_KEY_GOTO_DEFINITION, 4 },
    { "action.rename", IDE_KEY_RENAME, 16 },
    { "action.format", IDE_KEY_FORMAT, 11 },
    { "action.help", IDE_KEY_HELP, KEY_F1 },
};

static int bindings[(int) IDE_KEY_HELP + 1];

static IdeKeyAction action_from_name(const char *name)
{
    for (size_t i = 0; i < sizeof(action_specs) / sizeof(action_specs[0]); i++) {
        if (strcmp(action_specs[i].name, name) == 0) {
            return action_specs[i].action;
        }
    }
    return IDE_KEY_NONE;
}

static int parse_function_key(const char *text)
{
    long number;
    char *end;

    if (text[0] != 'F' && text[0] != 'f') {
        return 0;
    }
    number = strtol(text + 1, &end, 10);
    if (end == text + 1 || *end != '\0' || number < 1 || number > 12) {
        return 0;
    }
    switch (number) {
    case 1: return KEY_F1;
    case 2: return KEY_F2;
    case 3: return KEY_F3;
    case 4: return KEY_F4;
    case 5: return KEY_F5;
    case 6: return KEY_F6;
    case 7: return KEY_F7;
    case 8: return KEY_F8;
    case 9: return KEY_F9;
    case 10: return KEY_F10;
    case 11: return KEY_F11;
    case 12: return KEY_F12;
    default: return 0;
    }
}

static int parse_key_token(const char *token)
{
    if (!token || !token[0]) {
        return 0;
    }
    if (token[0] == 'F' || token[0] == 'f') {
        return parse_function_key(token);
    }
    if (strncasecmp(token, "Ctrl+", 5) == 0) {
        char letter = (char) token[5];

        if (!letter || token[6] != '\0') {
            return 0;
        }
        letter = (char) toupper((unsigned char) letter);
        if (letter < 'A' || letter > 'Z') {
            return 0;
        }
        return letter - 'A' + 1;
    }
    return 0;
}

static void set_defaults(void)
{
    for (size_t i = 0; i < sizeof(action_specs) / sizeof(action_specs[0]); i++) {
        bindings[(int) action_specs[i].action] = action_specs[i].default_key;
    }
}

static void apply_binding(IdeKeyAction action, int key)
{
    if (action == IDE_KEY_NONE || key == 0) {
        return;
    }
    for (int other = (int) IDE_KEY_MENU_FILE; other <= (int) IDE_KEY_HELP;
         other++) {
        if (bindings[other] == key && other != (int) action) {
            bindings[other] = 0;
        }
    }
    bindings[(int) action] = key;
}

static void load_keymap_file(const char *path)
{
    FILE *input = fopen(path, "r");
    char line[256];

    if (!input) {
        return;
    }
    while (fgets(line, sizeof(line), input)) {
        char *at = line;
        char *action_name;
        char *key_token;
        IdeKeyAction action;
        int key;

        while (*at == ' ' || *at == '\t') {
            at++;
        }
        if (*at == '#' || *at == '\0' || *at == '\n') {
            continue;
        }
        action_name = at;
        while (*at && *at != ' ' && *at != '\t' && *at != '\n') {
            at++;
        }
        if (*at == '\0' || *at == '\n') {
            continue;
        }
        *at++ = '\0';
        while (*at == ' ' || *at == '\t') {
            at++;
        }
        key_token = at;
        while (*at && *at != ' ' && *at != '\t' && *at != '\n' && *at != '\r') {
            at++;
        }
        *at = '\0';
        action = action_from_name(action_name);
        key = parse_key_token(key_token);
        apply_binding(action, key);
    }
    fclose(input);
}

void ide_keymap_init(const char *workspace_root, const char *include_path)
{
    char path[512];

    set_defaults();
    if (include_path && include_path[0]) {
        snprintf(path, sizeof(path), "%s/keymap/default.keymap", include_path);
        load_keymap_file(path);
        snprintf(path, sizeof(path), "%s/.cgem/keymap", include_path);
        load_keymap_file(path);
    }
    if (!workspace_root || !workspace_root[0]) {
        return;
    }
    snprintf(path, sizeof(path), "%s/keymap/default.keymap", workspace_root);
    load_keymap_file(path);
    snprintf(path, sizeof(path), "%s/.cgem/keymap", workspace_root);
    load_keymap_file(path);
}

IdeKeyAction ide_keymap_lookup(int key)
{
    for (int action = (int) IDE_KEY_MENU_FILE; action <= (int) IDE_KEY_HELP;
         action++) {
        if (bindings[action] != 0 && bindings[action] == key) {
            return (IdeKeyAction) action;
        }
    }
    return IDE_KEY_NONE;
}

int ide_keymap_key_for(IdeKeyAction action)
{
    if (action <= IDE_KEY_NONE || action > IDE_KEY_HELP) {
        return 0;
    }
    return bindings[(int) action];
}

static void format_key_label(int key, char *buffer, size_t size)
{
    if (size == 0) {
        return;
    }
    if (key >= KEY_F1 && key <= KEY_F12) {
        snprintf(buffer, size, "F%ld", (long) (key - KEY_F1 + 1));
        return;
    }
    if (key >= 1 && key <= 26) {
        snprintf(buffer, size, "Ctrl+%c", (char) ('A' + key - 1));
        return;
    }
    buffer[0] = '\0';
}

const char *ide_keymap_key_label(IdeKeyAction action)
{
    static char labels[4][24];
    static int label_index;
    char *buffer = labels[label_index];

    label_index = (label_index + 1) % 4;
    format_key_label(ide_keymap_key_for(action), buffer, sizeof(labels[0]));
    return buffer;
}
