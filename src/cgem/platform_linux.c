#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "cgem/platform.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static struct termios original_termios;
static int pending_key = -1;

static const char term_mouse_on[] = "\x1b[?1002h\x1b[?1006h\x1b[?1007h";
static const char term_mouse_off[] = "\x1b[?1002l\x1b[?1006l\x1b[?1007l";
static const char term_alt_on[] = "\x1b[?1049h";
static const char term_alt_off[] = "\x1b[?1049l";
static const char term_show_cursor[] = "\x1b[0m\x1b[?25h";
static const char term_clear_screen[] = "\x1b[0m\x1b[2J\x1b[H";

static void terminal_die(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

bool platform_terminal_init(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        terminal_die("tcgetattr");
    }
    atexit(platform_terminal_shutdown);

    raw = original_termios;
    raw.c_iflag &= (tcflag_t) ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t) ~OPOST;
    raw.c_cflag |= CS8;
    raw.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        terminal_die("tcsetattr");
    }
    platform_terminal_write(term_alt_on, sizeof(term_alt_on) - 1);
    platform_terminal_write(term_clear_screen, sizeof(term_clear_screen) - 1);
    platform_terminal_write(term_mouse_on, sizeof(term_mouse_on) - 1);
    return true;
}

void platform_terminal_shutdown(void)
{
    platform_terminal_write(term_mouse_off, sizeof(term_mouse_off) - 1);
    platform_terminal_write(term_alt_off, sizeof(term_alt_off) - 1);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    platform_terminal_write(term_show_cursor, sizeof(term_show_cursor) - 1);
}

bool platform_terminal_is_interactive(void)
{
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}

void platform_terminal_update_size(int *rows, int *cols)
{
    struct winsize size;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1 || size.ws_col == 0) {
        *rows = 24;
        *cols = 80;
    } else {
        *rows = size.ws_row;
        *cols = size.ws_col;
    }
}

void platform_terminal_write(const char *data, size_t length)
{
    while (length > 0) {
        ssize_t written = write(STDOUT_FILENO, data, length);
        if (written <= 0) {
            return;
        }
        data += (size_t) written;
        length -= (size_t) written;
    }
}

void platform_terminal_clear(void)
{
    platform_terminal_write(term_clear_screen, sizeof(term_clear_screen) - 1);
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

static PlatformMouseButton sgr_mouse_button(long encoded)
{
    switch (encoded & 3) {
    case 0:
        return PLATFORM_MOUSE_BUTTON_LEFT;
    case 1:
        return PLATFORM_MOUSE_BUTTON_MIDDLE;
    case 2:
        return PLATFORM_MOUSE_BUTTON_RIGHT;
    default:
        return PLATFORM_MOUSE_BUTTON_NONE;
    }
}

static bool parse_sgr_mouse(char *sequence, PlatformEventKind *kind,
                            int *row, int *col, PlatformMouseButton *out_button)
{
    char *at = sequence;
    char *end;
    long encoded;
    long x;
    long y;

    if (*at != '<') {
        return false;
    }
    at++;
    encoded = strtol(at, &end, 10);
    if (end == at || *end != ';') {
        return false;
    }
    at = end + 1;
    x = strtol(at, &end, 10);
    if (end == at || *end != ';') {
        return false;
    }
    at = end + 1;
    y = strtol(at, &end, 10);
    if (end == at || (*end != 'M' && *end != 'm')) {
        return false;
    }
    if (x < 1 || y < 1) {
        return false;
    }
    if (encoded == 64) {
        *kind = PLATFORM_EVENT_MOUSE_SCROLL_UP;
        if (out_button) {
            *out_button = PLATFORM_MOUSE_BUTTON_NONE;
        }
    } else if (encoded == 65) {
        *kind = PLATFORM_EVENT_MOUSE_SCROLL_DOWN;
        if (out_button) {
            *out_button = PLATFORM_MOUSE_BUTTON_NONE;
        }
    } else if (*end == 'm') {
        *kind = PLATFORM_EVENT_MOUSE_UP;
        if (out_button) {
            *out_button = sgr_mouse_button(encoded);
        }
    } else if ((encoded & 32) != 0) {
        *kind = PLATFORM_EVENT_MOUSE_DRAG;
        if (out_button) {
            *out_button = sgr_mouse_button(encoded);
        }
    } else if ((encoded & 3) <= 2) {
        *kind = PLATFORM_EVENT_MOUSE_DOWN;
        if (out_button) {
            *out_button = sgr_mouse_button(encoded);
        }
    } else {
        return false;
    }
    *row = (int) y;
    *col = (int) x;
    return true;
}

static bool csi_final_byte(char ch)
{
    return ch >= '@' && ch <= '~';
}

static bool stdin_has_pending(void)
{
    int count = 0;

    return ioctl(STDIN_FILENO, FIONREAD, &count) == 0 && count > 0;
}

void platform_input_flush(void)
{
    char drain[64];

    while (stdin_has_pending()) {
        ssize_t nbytes = read(STDIN_FILENO, drain, sizeof(drain));

        if (nbytes <= 0) {
            break;
        }
    }
}

static bool read_stdin_byte(char *out)
{
    return read(STDIN_FILENO, out, 1) == 1;
}

static PlatformEvent read_sgr_mouse_csi(char first_after_bracket)
{
    char sequence[32];
    int count = 0;

    sequence[count++] = first_after_bracket;
    while (count < (int) sizeof(sequence) - 1 &&
           read_stdin_byte(&sequence[count])) {
        if (sequence[count] == 'M' || sequence[count] == 'm') {
            count++;
            break;
        }
        count++;
    }
    sequence[count] = '\0';
    if (first_after_bracket == '<') {
        PlatformEventKind kind;
        int row;
        int col;
        PlatformMouseButton button = PLATFORM_MOUSE_BUTTON_NONE;

        if (parse_sgr_mouse(sequence, &kind, &row, &col, &button)) {
            return platform_mouse_event(kind, row, col, button);
        }
    }
    return platform_read_event();
}

static PlatformEvent read_csi_sequence(char second_byte)
{
    char sequence[32];

    sequence[0] = '[';
    sequence[1] = second_byte;

    if (second_byte == '<') {
        return read_sgr_mouse_csi('<');
    }
    if ((second_byte >= '0' && second_byte <= '9') || second_byte == ';') {
        int count = 2;

        while (count < (int) sizeof(sequence) - 1 &&
               read_stdin_byte(&sequence[count])) {
            count++;
            if (csi_final_byte(sequence[count - 1])) {
                break;
            }
        }
        sequence[count] = '\0';
        if (!strcmp(sequence + 1, "3~")) return key_event(KEY_DELETE);
        if (!strcmp(sequence + 1, "5~")) return key_event(KEY_PAGE_UP);
        if (!strcmp(sequence + 1, "6~")) return key_event(KEY_PAGE_DOWN);
        if (!strcmp(sequence + 1, "1;2A")) return key_event(KEY_SHIFT_ARROW_UP);
        if (!strcmp(sequence + 1, "1;2B")) return key_event(KEY_SHIFT_ARROW_DOWN);
        if (!strcmp(sequence + 1, "1;2C")) return key_event(KEY_SHIFT_ARROW_RIGHT);
        if (!strcmp(sequence + 1, "1;2D")) return key_event(KEY_SHIFT_ARROW_LEFT);
        if (!strcmp(sequence + 1, "11~")) return key_event(KEY_F1);
        if (!strcmp(sequence + 1, "12~")) return key_event(KEY_F2);
        if (!strcmp(sequence + 1, "13~")) return key_event(KEY_F3);
        if (!strcmp(sequence + 1, "14~")) return key_event(KEY_F4);
        if (!strcmp(sequence + 1, "15~")) return key_event(KEY_F5);
        if (!strcmp(sequence + 1, "17~")) return key_event(KEY_F6);
        if (!strcmp(sequence + 1, "18~")) return key_event(KEY_F7);
        if (!strcmp(sequence + 1, "19~")) return key_event(KEY_F8);
        if (!strcmp(sequence + 1, "20~")) return key_event(KEY_F9);
        if (!strcmp(sequence + 1, "21~")) return key_event(KEY_F10);
        if (!strcmp(sequence + 1, "23~")) return key_event(KEY_F11);
        if (!strcmp(sequence + 1, "24~")) return key_event(KEY_F12);
        if (!strcmp(sequence + 1, "1~") || !strcmp(sequence + 1, "7~")) {
            return key_event(KEY_HOME);
        }
        if (!strcmp(sequence + 1, "4~") || !strcmp(sequence + 1, "8~")) {
            return key_event(KEY_END);
        }
    } else {
        switch (second_byte) {
        case 'A': return key_event(KEY_ARROW_UP);
        case 'B': return key_event(KEY_ARROW_DOWN);
        case 'C': return key_event(KEY_ARROW_RIGHT);
        case 'D': return key_event(KEY_ARROW_LEFT);
        case 'H': return key_event(KEY_HOME);
        case 'F': return key_event(KEY_END);
        case 'Z': return key_event(KEY_SHIFT_TAB);
        default:
            break;
        }
    }
    return key_event('\x1b');
}

static PlatformEvent read_escape_sequence(void)
{
    char kind;
    char second;

    if (!read_stdin_byte(&kind)) {
        return key_event('\x1b');
    }
    if (kind != '[' && kind != 'O') {
        pending_key = (unsigned char) kind;
        return key_event('\x1b');
    }
    if (!read_stdin_byte(&second)) {
        return key_event('\x1b');
    }
    if (kind == '[') {
        return read_csi_sequence(second);
    }
    switch (second) {
    case 'P': return key_event(KEY_F1);
    case 'Q': return key_event(KEY_F2);
    case 'R': return key_event(KEY_F3);
    case 'S': return key_event(KEY_F4);
    default:
        break;
    }
    return key_event('\x1b');
}

static PlatformEvent read_orphan_bracket(void)
{
    char next;

    if (!stdin_has_pending() || !read_stdin_byte(&next)) {
        return key_event('[');
    }
    if (next == '<') {
        return read_sgr_mouse_csi('<');
    }
    pending_key = (unsigned char) next;
    return key_event('[');
}

PlatformEvent platform_read_event(void)
{
    char ch;

    if (pending_key != -1) {
        int key = pending_key;
        pending_key = -1;
        return key_event(key);
    }
    while (!read_stdin_byte(&ch)) {
    }
    if (ch == '[') {
        return read_orphan_bracket();
    }
    if (ch != '\x1b') {
        return key_event((unsigned char) ch);
    }
    if (!stdin_has_pending()) {
        return key_event('\x1b');
    }
    return read_escape_sequence();
}

bool platform_path_exists(const char *path)
{
    struct stat status;

    return stat(path, &status) == 0;
}

bool platform_path_is_directory(const char *path)
{
    struct stat status;

    return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
}

bool platform_path_is_regular_file(const char *path)
{
    struct stat status;

    return stat(path, &status) == 0 && S_ISREG(status.st_mode);
}

bool platform_clang_format_available(void)
{
    const char *path = getenv("PATH");
    const char *at;

    if (!path) {
        return false;
    }
    at = path;
    for (;;) {
        const char *end = strchr(at, ':');
        size_t directory_length = end ? (size_t) (end - at) : strlen(at);
        size_t length = directory_length == 0
                            ? strlen("./clang-format") + 1
                            : directory_length + strlen("/clang-format") + 1;
        char *candidate = malloc(length);
        bool available;

        if (!candidate) {
            return false;
        }
        if (directory_length == 0) {
            snprintf(candidate, length, "./clang-format");
        } else {
            snprintf(candidate, length, "%.*s/clang-format",
                     (int) directory_length, at);
        }
        available = access(candidate, X_OK) == 0;
        free(candidate);
        if (available) {
            return true;
        }
        if (!end) {
            return false;
        }
        at = end + 1;
    }
}

int platform_clang_format(const char *path, const char *style_path)
{
    pid_t child;
    int status;
    size_t style_length = strlen("--style=file:") + strlen(style_path) + 1;
    char *style = malloc(style_length);

    if (!style) {
        return -1;
    }
    snprintf(style, style_length, "--style=file:%s", style_path);
    child = fork();
    if (child < 0) {
        free(style);
        return -1;
    }
    if (child == 0) {
        execlp("clang-format", "clang-format", "-i", style,
               path, (char *) NULL);
        _exit(127);
    }
    free(style);
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

bool platform_scan_directory(const char *directory, PlatformDirCallback callback,
                             void *context)
{
    DIR *handle = opendir(directory);
    struct dirent *entry;

    if (!handle) {
        return false;
    }
    while ((entry = readdir(handle)) != NULL) {
        char *child;
        size_t length;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        length = strlen(directory) + strlen(entry->d_name) + 2;
        child = malloc(length);
        if (!child) {
            closedir(handle);
            return false;
        }
        snprintf(child, length, "%s/%s", directory, entry->d_name);
        if (!callback(child, context)) {
            free(child);
            closedir(handle);
            return false;
        }
        free(child);
    }
    closedir(handle);
    return true;
}

int platform_mkdir_p(const char *path, char *error, size_t error_size)
{
    char *copy = strdup(path);

    if (!copy) {
        snprintf(error, error_size, "out of memory");
        return -1;
    }
    for (char *at = copy + 1; *at; at++) {
        if (*at != '/') {
            continue;
        }
        *at = '\0';
        if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
            snprintf(error, error_size, "%s: %s", copy, strerror(errno));
            free(copy);
            return -1;
        }
        *at = '/';
    }
    if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
        snprintf(error, error_size, "%s: %s", copy, strerror(errno));
        free(copy);
        return -1;
    }
    free(copy);
    return 0;
}

static size_t base64_encoded_length(size_t length)
{
    return 4 * ((length + 2) / 3);
}

static char *base64_encode(const unsigned char *data, size_t length)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_length = base64_encoded_length(length);
    char *out = malloc(out_length + 1);
    size_t at = 0;
    size_t i = 0;

    if (!out) {
        return NULL;
    }
    while (i + 2 < length) {
        unsigned value = (unsigned) data[i] << 16 |
                         (unsigned) data[i + 1] << 8 |
                         (unsigned) data[i + 2];

        out[at++] = alphabet[(value >> 18) & 63];
        out[at++] = alphabet[(value >> 12) & 63];
        out[at++] = alphabet[(value >> 6) & 63];
        out[at++] = alphabet[value & 63];
        i += 3;
    }
    if (i < length) {
        unsigned value = (unsigned) data[i] << 16;

        if (i + 1 < length) {
            value |= (unsigned) data[i + 1] << 8;
        }
        out[at++] = alphabet[(value >> 18) & 63];
        out[at++] = alphabet[(value >> 12) & 63];
        out[at++] = (i + 1 < length) ? alphabet[(value >> 6) & 63] : '=';
        out[at++] = '=';
    }
    out[at] = '\0';
    return out;
}

bool platform_set_clipboard(const char *text)
{
    char *encoded;
    char *sequence;
    size_t sequence_length;
    FILE *pipe;
    size_t written;

    if (!text) {
        return false;
    }
    pipe = popen("wl-copy --type text 2>/dev/null || "
                 "xclip -selection clipboard 2>/dev/null",
                 "w");
    if (pipe) {
        written = fwrite(text, 1, strlen(text), pipe);
        if (pclose(pipe) == 0 && written == strlen(text)) {
            return true;
        }
    }
    encoded = base64_encode((const unsigned char *) text, strlen(text));
    if (!encoded) {
        return false;
    }
    sequence_length = strlen(encoded) + 16;
    sequence = malloc(sequence_length);
    if (!sequence) {
        free(encoded);
        return false;
    }
    snprintf(sequence, sequence_length, "\033]52;c;%s\033\\", encoded);
    platform_terminal_write(sequence, strlen(sequence));
    free(sequence);
    free(encoded);
    return true;
}

bool platform_get_clipboard(char **text)
{
    FILE *pipe;
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    if (!text) {
        return false;
    }
    *text = NULL;
    pipe = popen("wl-paste -n 2>/dev/null || "
                 "xclip -o -selection clipboard 2>/dev/null || "
                 "xsel --clipboard --output 2>/dev/null",
                 "r");
    if (!pipe) {
        return false;
    }
    while (true) {
        if (length + 256 > capacity) {
            size_t next = capacity ? capacity * 2 : 256;
            char *grown = realloc(buffer, next);

            if (!grown) {
                free(buffer);
                pclose(pipe);
                return false;
            }
            buffer = grown;
            capacity = next;
        }
        if (!fgets(buffer + length, (int) (capacity - length), pipe)) {
            break;
        }
        length += strlen(buffer + length);
    }
    pclose(pipe);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
    }
    if (length == 0) {
        free(buffer);
        return false;
    }
    *text = buffer;
    return true;
}
