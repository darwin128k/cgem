#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#include "cgem/platform.h"

#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef ENABLE_QUICK_EDIT_MODE
#define ENABLE_QUICK_EDIT_MODE 0x0040
#endif

#ifndef FROM_RIGHT_2ND_BUTTON_PRESSED
#define FROM_RIGHT_2ND_BUTTON_PRESSED RIGHTMOST_BUTTON_PRESSED
#endif
#ifndef FROM_MIDDLE_3RD_BUTTON_PRESSED
#define FROM_MIDDLE_3RD_BUTTON_PRESSED FROM_LEFT_2ND_BUTTON_PRESSED
#endif

static HANDLE console_in;
static HANDLE console_out;
static DWORD original_in_mode;
static DWORD original_out_mode;
static CONSOLE_SCREEN_BUFFER_INFO original_info;
static COORD cursor;
static WORD current_attr = 7;
static bool cursor_visible = true;
static bool terminal_ready;
static bool virtual_terminal;
static bool mouse_left_down;
static PlatformMouseButton mouse_tracking_button;
static WORD fg_nibble = 7;
static WORD bg_nibble = 0;
static bool fg_intense = false;

static char *duplicate_string(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, length);
    return copy;
}

static void compose_attr(void)
{
    WORD fg = (WORD) (fg_nibble & 7);

    if (fg_intense) {
        fg = (WORD) (fg | 8);
    }
    current_attr = (WORD) (fg | (bg_nibble << 4));
}

static int color_luminance(int red, int green, int blue)
{
    return red * 299 + green * 587 + blue * 114;
}

static WORD rgb_to_fg_index(int red, int green, int blue)
{
    static const int palette[8][3] = {
        {0, 0, 0}, {0, 0, 170}, {0, 170, 0}, {0, 170, 170},
        {170, 0, 0}, {170, 0, 170}, {170, 85, 0}, {170, 170, 170}
    };
    int best = 7;
    int best_distance = 0;

    for (int i = 0; i < 8; i++) {
        int dr = red - palette[i][0];
        int dg = green - palette[i][1];
        int db = blue - palette[i][2];
        int distance = dr * dr + dg * dg + db * db;

        if (i == 0 || distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    return (WORD) best;
}

static WORD rgb_to_bg_index(int red, int green, int blue)
{
    int lum = color_luminance(red, green, blue);

    if (lum < 20000) {
        return 0;
    }
    if (lum > 520000) {
        return 7;
    }
    if (blue > red + 20 && blue > green + 20) {
        return 1;
    }
    if (red > green + 30 && red > blue + 30) {
        return 4;
    }
    if (green > red + 30 && green > blue + 30) {
        return 2;
    }
    return 0;
}

static void sync_console_buffer(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD size;

    if (!GetConsoleScreenBufferInfo(console_out, &info)) {
        return;
    }
    size.X = (SHORT) (info.srWindow.Right - info.srWindow.Left + 1);
    size.Y = (SHORT) (info.srWindow.Bottom - info.srWindow.Top + 1);
    if (size.X < 1) {
        size.X = 80;
    }
    if (size.Y < 1) {
        size.Y = 25;
    }
    SetConsoleScreenBufferSize(console_out, size);
    cursor.X = 0;
    cursor.Y = 0;
    SetConsoleCursorPosition(console_out, cursor);
}

static void apply_attr(void)
{
    SetConsoleTextAttribute(console_out, current_attr);
}

/* Windows 10+ terminals can render ANSI directly; older consoles use the
   parser below and the classic Console API fallback. */
static void write_raw(const char *text, size_t length)
{
    while (length > 0) {
        DWORD chunk = length > (size_t) 0xffffffffUL
                          ? (DWORD) 0xffffffffUL
                          : (DWORD) length;
        DWORD written = 0;

        if (!WriteFile(console_out, text, chunk, &written, NULL) ||
            written == 0) {
            return;
        }
        text += written;
        length -= written;
    }
}

static void write_text(const char *text, size_t length)
{
    DWORD written;

    if (length == 0) {
        return;
    }
    apply_attr();
    WriteConsoleA(console_out, text, (DWORD) length, &written, NULL);
    cursor.X = (SHORT) (cursor.X + (SHORT) length);
}

static void newline(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    SHORT window_height;

    GetConsoleScreenBufferInfo(console_out, &info);
    window_height = (SHORT) (info.srWindow.Bottom - info.srWindow.Top + 1);
    cursor.X = 0;
    cursor.Y = (SHORT) (cursor.Y + 1);
    if (cursor.Y >= window_height) {
        SMALL_RECT scroll = info.srWindow;
        COORD origin = {0, 1};
        CHAR_INFO fill;
        fill.Char.AsciiChar = ' ';
        fill.Attributes = current_attr;

        ScrollConsoleScreenBuffer(console_out, &scroll, NULL, origin, &fill);
        cursor.Y = (SHORT) (window_height - 1);
    }
    SetConsoleCursorPosition(console_out, cursor);
}

static void clear_screen(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    COORD home = {0, 0};
    DWORD cells;
    DWORD written;
    CHAR ch = ' ';

    GetConsoleScreenBufferInfo(console_out, &info);
    cells = (DWORD) info.dwSize.X * (DWORD) info.dwSize.Y;
    FillConsoleOutputCharacterA(console_out, ch, cells, home, &written);
    FillConsoleOutputAttribute(console_out, 0x07, cells, home, &written);
    fg_nibble = 7;
    bg_nibble = 0;
    fg_intense = false;
    current_attr = 0x07;
    cursor = home;
    SetConsoleCursorPosition(console_out, cursor);
}

static void set_cursor_position(int row, int col)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    GetConsoleScreenBufferInfo(console_out, &info);
    cursor.X = (SHORT) (col - 1);
    cursor.Y = (SHORT) (info.srWindow.Top + row - 1);
    SetConsoleCursorPosition(console_out, cursor);
}

static void hide_cursor(bool hide)
{
    CONSOLE_CURSOR_INFO info;

    GetConsoleCursorInfo(console_out, &info);
    info.bVisible = hide ? FALSE : TRUE;
    SetConsoleCursorInfo(console_out, &info);
    cursor_visible = !hide;
}

static int parse_number(const char *text, int *value, int *consumed)
{
    int at = 0;

    *value = 0;
    while (text[at] >= '0' && text[at] <= '9') {
        *value = *value * 10 + (text[at] - '0');
        at++;
    }
    *consumed = at;
    return at;
}

static void handle_sgr(const char *params)
{
    int at = 0;
    int fg = -1;
    int fg_r = 248;
    int fg_g = 248;
    int fg_b = 242;
    int bg = -1;
    int bg_r = 0;
    int bg_g = 0;
    int bg_b = 0;
    bool bold = false;

    while (params[at] != '\0') {
        int value = 0;
        int consumed = 0;

        parse_number(params + at, &value, &consumed);
        if (consumed == 0) {
            break;
        }
        at += consumed;
        if (params[at] == ';') {
            at++;
        }

        if (value == 0) {
            fg_nibble = 7;
            bg_nibble = 0;
            fg_intense = false;
            fg = bg = -1;
            bold = false;
        } else if (value == 1) {
            bold = true;
        } else if (value == 22) {
            bold = false;
        } else if (value >= 30 && value <= 37) {
            fg_nibble = (WORD) (value - 30);
            fg = -1;
        } else if (value >= 40 && value <= 47) {
            bg_nibble = (WORD) (value - 40);
            bg = -1;
        } else if (value >= 90 && value <= 97) {
            fg_nibble = (WORD) (value - 90);
            fg_intense = true;
            fg = -1;
        } else if (value == 38 && params[at] == '2') {
            int parts[3];
            at++;
            for (int i = 0; i < 3; i++) {
                if (params[at] == ';') {
                    at++;
                }
                parse_number(params + at, &parts[i], &consumed);
                at += consumed;
            }
            fg = 0;
            fg_r = parts[0];
            fg_g = parts[1];
            fg_b = parts[2];
        } else if (value == 48 && params[at] == '2') {
            int parts[3];
            at++;
            for (int i = 0; i < 3; i++) {
                if (params[at] == ';') {
                    at++;
                }
                parse_number(params + at, &parts[i], &consumed);
                at += consumed;
            }
            bg = 0;
            bg_r = parts[0];
            bg_g = parts[1];
            bg_b = parts[2];
        }
    }

    if (fg >= 0) {
        fg_nibble = rgb_to_fg_index(fg_r, fg_g, fg_b);
    }
    if (bg >= 0) {
        bg_nibble = rgb_to_bg_index(bg_r, bg_g, bg_b);
    }
    if (bold) {
        fg_intense = true;
    }
    compose_attr();
}

static void handle_csi(char final, char *params, size_t params_length)
{
    params[params_length] = '\0';

    if (final == 'H' || final == 'f') {
        int row = 1;
        int col = 1;
        int consumed = 0;
        char *semi = strchr(params, ';');

        parse_number(params, &row, &consumed);
        if (semi) {
            parse_number(semi + 1, &col, &consumed);
        }
        if (row < 1) {
            row = 1;
        }
        if (col < 1) {
            col = 1;
        }
        set_cursor_position(row, col);
    } else if (final == 'J') {
        if (params_length == 0 || strcmp(params, "2") == 0) {
            clear_screen();
        }
    } else if (final == 'm') {
        handle_sgr(params);
    } else if (final == 'h' || final == 'l') {
        if (strcmp(params, "?25") == 0) {
            hide_cursor(final == 'l');
        }
    }
}

static void write_ansi_chunk(const char *data, size_t length)
{
    size_t at = 0;

    while (at < length) {
        if (data[at] == '\x1b') {
            if (at + 1 >= length) {
                break;
            }
            if (data[at + 1] == '[') {
                char params[64];
                size_t params_length = 0;
                size_t seq = at + 2;

                while (seq < length) {
                    char ch = data[seq];
                    if ((ch >= 'A' && ch <= 'Z') ||
                        (ch >= 'a' && ch <= 'z')) {
                        handle_csi(ch, params, params_length);
                        at = seq + 1;
                        break;
                    }
                    if (params_length + 1 < sizeof(params)) {
                        params[params_length++] = ch;
                    }
                    seq++;
                }
                if (seq >= length) {
                    break;
                }
                continue;
            }
            at += 2;
            continue;
        }
        if (data[at] == '\r') {
            cursor.X = 0;
            SetConsoleCursorPosition(console_out, cursor);
            at++;
            if (at < length && data[at] == '\n') {
                at++;
            }
            newline();
            continue;
        }
        if (data[at] == '\n') {
            newline();
            at++;
            continue;
        }

        size_t start = at;
        while (at < length && data[at] != '\x1b' &&
               data[at] != '\r' && data[at] != '\n') {
            at++;
        }
        write_text(data + start, at - start);
    }
}

static void terminal_die(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

bool platform_terminal_init(void)
{
    console_in = GetStdHandle(STD_INPUT_HANDLE);
    console_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (console_in == INVALID_HANDLE_VALUE ||
        console_out == INVALID_HANDLE_VALUE) {
        terminal_die("GetStdHandle");
    }
    if (!GetConsoleScreenBufferInfo(console_out, &original_info)) {
        terminal_die("GetConsoleScreenBufferInfo");
    }
    if (!GetConsoleMode(console_in, &original_in_mode) ||
        !GetConsoleMode(console_out, &original_out_mode)) {
        terminal_die("GetConsoleMode");
    }

    DWORD in_mode = original_in_mode;
    DWORD out_mode = original_out_mode;
    DWORD vt_out_mode;

    in_mode &= ~(DWORD) (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
                         ENABLE_PROCESSED_INPUT |
                         ENABLE_QUICK_EDIT_MODE);
    in_mode |= ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT;
    out_mode |= ENABLE_PROCESSED_OUTPUT;

    if (!SetConsoleMode(console_in, in_mode) ||
        !SetConsoleMode(console_out, out_mode)) {
        terminal_die("SetConsoleMode");
    }

    cursor = original_info.dwCursorPosition;
    current_attr = original_info.wAttributes;
    fg_nibble = (WORD) (current_attr & 0x07);
    bg_nibble = (WORD) ((current_attr >> 4) & 0x07);
    fg_intense = (current_attr & 0x08) != 0;
    vt_out_mode = out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    virtual_terminal = SetConsoleMode(console_out, vt_out_mode) != 0;
    sync_console_buffer();
    terminal_ready = true;
    atexit(platform_terminal_shutdown);
    return true;
}

void platform_terminal_shutdown(void)
{
    if (!terminal_ready) {
        return;
    }
    if (virtual_terminal) {
        write_raw("\x1b[0m\x1b[?25h", 11);
    } else {
        hide_cursor(false);
    }
    SetConsoleMode(console_in, original_in_mode);
    SetConsoleMode(console_out, original_out_mode);
    SetConsoleTextAttribute(console_out, original_info.wAttributes);
    terminal_ready = false;
}

bool platform_terminal_is_interactive(void)
{
    return _isatty(0) && _isatty(1);
}

void platform_terminal_update_size(int *rows, int *cols)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (!GetConsoleScreenBufferInfo(console_out, &info)) {
        *rows = 24;
        *cols = 80;
        return;
    }
    *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    *cols = info.srWindow.Right - info.srWindow.Left + 1;
    if (*rows < 1) {
        *rows = 24;
    }
    if (*cols < 1) {
        *cols = 80;
    }
}

void platform_terminal_write(const char *data, size_t length)
{
    if (virtual_terminal) {
        write_raw(data, length);
    } else {
        write_ansi_chunk(data, length);
    }
}

void platform_terminal_clear(void)
{
    static const char sequence[] = "\x1b[0m\x1b[2J\x1b[H";

    if (virtual_terminal) {
        write_raw(sequence, sizeof(sequence) - 1);
    } else {
        clear_screen();
    }
}

void platform_input_flush(void)
{
}

static int map_function_key(WORD virtual_key, DWORD control)
{
    bool shift = (control & SHIFT_PRESSED) != 0;

    switch (virtual_key) {
    case VK_LEFT: return shift ? KEY_SHIFT_ARROW_LEFT : KEY_ARROW_LEFT;
    case VK_RIGHT: return shift ? KEY_SHIFT_ARROW_RIGHT : KEY_ARROW_RIGHT;
    case VK_UP: return shift ? KEY_SHIFT_ARROW_UP : KEY_ARROW_UP;
    case VK_DOWN: return shift ? KEY_SHIFT_ARROW_DOWN : KEY_ARROW_DOWN;
    case VK_HOME: return KEY_HOME;
    case VK_END: return KEY_END;
    case VK_PRIOR: return KEY_PAGE_UP;
    case VK_NEXT: return KEY_PAGE_DOWN;
    case VK_DELETE: return KEY_DELETE;
    case VK_BACK: return KEY_BACKSPACE;
    case VK_F1: return KEY_F1;
    case VK_F2: return KEY_F2;
    case VK_F3: return KEY_F3;
    case VK_F4: return KEY_F4;
    case VK_F5: return KEY_F5;
    case VK_F6: return KEY_F6;
    case VK_F7: return KEY_F7;
    case VK_F8: return KEY_F8;
    case VK_F9: return KEY_F9;
    case VK_F10: return KEY_F10;
    case VK_F11: return KEY_F11;
    case VK_F12: return KEY_F12;
    default: return -1;
    }
}

static PlatformEvent key_event(int key)
{
    PlatformEvent event = {PLATFORM_EVENT_KEY, key, 0, 0,
                           PLATFORM_MOUSE_BUTTON_NONE};

    return event;
}

static PlatformEvent platform_mouse_event(PlatformEventKind kind, int row,
                                          int col, PlatformMouseButton button)
{
    PlatformEvent event = {kind, 0, row, col, button};

    return event;
}

static PlatformMouseButton mouse_button_from_state(DWORD state)
{
    if ((state & FROM_LEFT_1ST_BUTTON_PRESSED) != 0) {
        return PLATFORM_MOUSE_BUTTON_LEFT;
    }
    if ((state & FROM_RIGHT_2ND_BUTTON_PRESSED) != 0) {
        return PLATFORM_MOUSE_BUTTON_RIGHT;
    }
    if ((state & FROM_MIDDLE_3RD_BUTTON_PRESSED) != 0) {
        return PLATFORM_MOUSE_BUTTON_MIDDLE;
    }
    return PLATFORM_MOUSE_BUTTON_NONE;
}

PlatformEvent platform_read_event(void)
{
    INPUT_RECORD record;
    DWORD read_count;

    while (true) {
        if (!ReadConsoleInputA(console_in, &record, 1, &read_count) ||
            read_count != 1) {
            continue;
        }
        if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            sync_console_buffer();
            continue;
        }
        if (record.EventType == MOUSE_EVENT) {
            MOUSE_EVENT_RECORD mouse = record.Event.MouseEvent;

            if (mouse.dwEventFlags == MOUSE_WHEELED) {
                SHORT delta = (SHORT) HIWORD(mouse.dwButtonState);

                if (delta > 0) {
                    return platform_mouse_event(PLATFORM_EVENT_MOUSE_SCROLL_UP,
                                                0, 0,
                                                PLATFORM_MOUSE_BUTTON_NONE);
                }
                if (delta < 0) {
                    return platform_mouse_event(
                        PLATFORM_EVENT_MOUSE_SCROLL_DOWN, 0, 0,
                        PLATFORM_MOUSE_BUTTON_NONE);
                }
                continue;
            }
            {
                DWORD buttons = mouse.dwButtonState &
                    (FROM_LEFT_1ST_BUTTON_PRESSED |
                     FROM_RIGHT_2ND_BUTTON_PRESSED |
                     FROM_MIDDLE_3RD_BUTTON_PRESSED);
                bool pressed = buttons != 0;
                bool moved = mouse.dwEventFlags == MOUSE_MOVED;

                if (pressed || mouse_tracking_button != PLATFORM_MOUSE_BUTTON_NONE) {
                CONSOLE_SCREEN_BUFFER_INFO info;
                int row = mouse.dwMousePosition.Y + 1;
                int col = mouse.dwMousePosition.X + 1;

                if (GetConsoleScreenBufferInfo(console_out, &info)) {
                    row = mouse.dwMousePosition.Y - info.srWindow.Top + 1;
                    col = mouse.dwMousePosition.X - info.srWindow.Left + 1;
                }
                if (pressed && mouse_tracking_button == PLATFORM_MOUSE_BUTTON_NONE) {
                    mouse_tracking_button = mouse_button_from_state(buttons);
                    if (mouse_tracking_button == PLATFORM_MOUSE_BUTTON_LEFT) {
                        mouse_left_down = true;
                    }
                    return platform_mouse_event(PLATFORM_EVENT_MOUSE_DOWN, row,
                                                col, mouse_tracking_button);
                }
                if (pressed && mouse_tracking_button == PLATFORM_MOUSE_BUTTON_LEFT &&
                    moved) {
                    return platform_mouse_event(PLATFORM_EVENT_MOUSE_DRAG, row,
                                                col, PLATFORM_MOUSE_BUTTON_LEFT);
                }
                if (!pressed &&
                    mouse_tracking_button != PLATFORM_MOUSE_BUTTON_NONE) {
                    PlatformMouseButton released = mouse_tracking_button;

                    mouse_tracking_button = PLATFORM_MOUSE_BUTTON_NONE;
                    mouse_left_down = false;
                    return platform_mouse_event(PLATFORM_EVENT_MOUSE_UP, row,
                                                col, released);
                }
                }
            }
            continue;
        }
        if (record.EventType != KEY_EVENT ||
            !record.Event.KeyEvent.bKeyDown) {
            continue;
        }

        DWORD control = record.Event.KeyEvent.dwControlKeyState;
        WORD virtual_key = record.Event.KeyEvent.wVirtualKeyCode;
        char ch = record.Event.KeyEvent.uChar.AsciiChar;

        if (virtual_key == VK_TAB &&
            (control & SHIFT_PRESSED) != 0) {
            return key_event(KEY_SHIFT_TAB);
        }

        if (ch != 0) {
            return key_event((unsigned char) ch);
        }

        int mapped = map_function_key(virtual_key, control);
        if (mapped != -1) {
            return key_event(mapped);
        }
    }
}

bool platform_path_exists(const char *path)
{
    struct _stat status;

    return _stat(path, &status) == 0;
}

bool platform_path_is_directory(const char *path)
{
    struct _stat status;

    return _stat(path, &status) == 0 && (status.st_mode & _S_IFDIR) != 0;
}

bool platform_path_is_regular_file(const char *path)
{
    struct _stat status;

    return _stat(path, &status) == 0 && (status.st_mode & _S_IFREG) != 0;
}

bool platform_clang_format_available(void)
{
    char path[MAX_PATH];

    return SearchPathA(NULL, "clang-format.exe", NULL,
                       (DWORD) sizeof(path), path, NULL) > 0;
}

int platform_clang_format(const char *path, const char *style_path)
{
    size_t style_length = strlen("--style=file:") + strlen(style_path) + 1;
    char *style = malloc(style_length);
    const char *arguments[5];
    intptr_t result;

    if (!style) {
        return -1;
    }
    snprintf(style, style_length, "--style=file:%s", style_path);
    arguments[0] = "clang-format";
    arguments[1] = "-i";
    arguments[2] = style;
    arguments[3] = path;
    arguments[4] = NULL;
    result = _spawnvp(_P_WAIT, "clang-format", arguments);
    free(style);
    return result == 0 ? 0 : -1;
}

bool platform_scan_directory(const char *directory, PlatformDirCallback callback,
                             void *context)
{
    char pattern[MAX_PATH];
    WIN32_FIND_DATAA entry;
    HANDLE handle;

    if (snprintf(pattern, sizeof(pattern), "%s\\*", directory) >=
        (int) sizeof(pattern)) {
        return false;
    }
    handle = FindFirstFileA(pattern, &entry);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    do {
        char *child;
        size_t length;

        if (strcmp(entry.cFileName, ".") == 0 ||
            strcmp(entry.cFileName, "..") == 0) {
            continue;
        }
        length = strlen(directory) + strlen(entry.cFileName) + 2;
        child = malloc(length);
        if (!child) {
            FindClose(handle);
            return false;
        }
        snprintf(child, length, "%s/%s", directory, entry.cFileName);
        if (!callback(child, context)) {
            free(child);
            FindClose(handle);
            return false;
        }
        free(child);
    } while (FindNextFileA(handle, &entry));
    FindClose(handle);
    return true;
}

int platform_mkdir_p(const char *path, char *error, size_t error_size)
{
    char *copy = duplicate_string(path);

    if (!copy) {
        snprintf(error, error_size, "out of memory");
        return -1;
    }
    for (char *at = copy + 1; *at; at++) {
        if (*at != '/' && *at != '\\') {
            continue;
        }
        *at = '\0';
        if (_mkdir(copy) != 0 && errno != EEXIST) {
            snprintf(error, error_size, "%s: %s", copy, strerror(errno));
            free(copy);
            return -1;
        }
        *at = '/';
    }
    if (_mkdir(copy) != 0 && errno != EEXIST) {
        snprintf(error, error_size, "%s: %s", copy, strerror(errno));
        free(copy);
        return -1;
    }
    free(copy);
    return 0;
}

bool platform_set_clipboard(const char *text)
{
    HGLOBAL memory;
    size_t length;
    char *copy;

    if (!text) {
        return false;
    }
    length = strlen(text) + 1;
    if (!OpenClipboard(NULL)) {
        return false;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }
    memory = GlobalAlloc(GMEM_MOVEABLE, length);
    if (!memory) {
        CloseClipboard();
        return false;
    }
    copy = GlobalLock(memory);
    if (!copy) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    memcpy(copy, text, length);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_TEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

bool platform_get_clipboard(char **text)
{
    HANDLE memory;
    char *data;
    char *copy;

    if (!text) {
        return false;
    }
    *text = NULL;
    if (!OpenClipboard(NULL)) {
        return false;
    }
    memory = GetClipboardData(CF_TEXT);
    if (!memory) {
        CloseClipboard();
        return false;
    }
    data = GlobalLock(memory);
    if (!data) {
        CloseClipboard();
        return false;
    }
    copy = _strdup(data);
    GlobalUnlock(memory);
    CloseClipboard();
    if (!copy) {
        return false;
    }
    *text = copy;
    return true;
}
