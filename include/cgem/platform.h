#ifndef CGEM_PLATFORM_H
#define CGEM_PLATFORM_H

#include <stddef.h>
#include <stdbool.h>

enum {
    KEY_BACKSPACE = 127,
    KEY_ARROW_LEFT = 1000,
    KEY_ARROW_RIGHT,
    KEY_ARROW_UP,
    KEY_ARROW_DOWN,
    KEY_SHIFT_ARROW_LEFT,
    KEY_SHIFT_ARROW_RIGHT,
    KEY_SHIFT_ARROW_UP,
    KEY_SHIFT_ARROW_DOWN,
    KEY_DELETE,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_SHIFT_TAB,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12
};

typedef enum {
    PLATFORM_EVENT_KEY,
    PLATFORM_EVENT_MOUSE_DOWN,
    PLATFORM_EVENT_MOUSE_DRAG,
    PLATFORM_EVENT_MOUSE_UP,
    PLATFORM_EVENT_MOUSE_SCROLL_UP,
    PLATFORM_EVENT_MOUSE_SCROLL_DOWN
} PlatformEventKind;

typedef enum {
    PLATFORM_MOUSE_BUTTON_NONE = 0,
    PLATFORM_MOUSE_BUTTON_LEFT,
    PLATFORM_MOUSE_BUTTON_MIDDLE,
    PLATFORM_MOUSE_BUTTON_RIGHT
} PlatformMouseButton;

typedef struct {
    PlatformEventKind kind;
    int key;
    int row;
    int col;
    PlatformMouseButton button;
} PlatformEvent;

bool platform_terminal_init(void);
void platform_terminal_shutdown(void);
bool platform_terminal_is_interactive(void);
void platform_terminal_update_size(int *rows, int *cols);
void platform_terminal_write(const char *data, size_t length);
void platform_terminal_clear(void);
PlatformEvent platform_read_event(void);
void platform_input_flush(void);

bool platform_path_exists(const char *path);
bool platform_path_is_directory(const char *path);
bool platform_path_is_regular_file(const char *path);
bool platform_clang_format_available(void);
int platform_clang_format(const char *path, const char *style_path);

typedef bool (*PlatformDirCallback)(const char *path, void *context);

bool platform_scan_directory(const char *directory, PlatformDirCallback callback,
                             void *context);

int platform_mkdir_p(const char *path, char *error, size_t error_size);

bool platform_set_clipboard(const char *text);
bool platform_get_clipboard(char **text);

#endif
