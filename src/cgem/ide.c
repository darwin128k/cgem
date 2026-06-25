#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#endif

#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "cgem/common.h"
#include "cgem/bmp.h"
#include "cgem/compiler.h"
#include "cgem/compiler_internal.h"
#include "cgem/diagnostic.h"
#include "cgem/format.h"
#include "cgem/ide.h"
#include "cgem/ide_index.h"
#include "cgem/semantic.h"
#include "cgem/ide_keymap.h"
#include "cgem/ide_menu.h"
#include "cgem/platform.h"
#include "cgem/theme.h"

#define GUTTER_SIDE_PAD 2
#define DIFF_BAR_GLYPH "|"

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Row;

typedef struct {
    Row *rows;
    size_t row_count;
    size_t cursor_x;
    size_t cursor_y;
    uint64_t revision;
} EditorSnapshot;

#define IDE_HISTORY_LIMIT 128

typedef struct {
    EditorSnapshot undo[IDE_HISTORY_LIMIT];
    size_t undo_count;
    EditorSnapshot redo[IDE_HISTORY_LIMIT];
    size_t redo_count;
    bool suspended;
} EditorHistory;

typedef enum {
    LINE_CHANGE_NONE = 0,
    LINE_CHANGE_ADDED,
    LINE_CHANGE_MODIFIED
} LineChange;

typedef enum {
    IDE_PROMPT_NONE = 0,
    IDE_PROMPT_OPEN,
    IDE_PROMPT_SAVE_AS,
    IDE_PROMPT_FIND,
    IDE_PROMPT_GOTO_LINE,
    IDE_PROMPT_RENAME,
    IDE_PROMPT_THEME
} IdePrompt;

typedef struct {
    Row *rows;
    size_t row_count;
    size_t row_capacity;
    size_t cursor_x;
    size_t cursor_y;
    size_t row_offset;
    size_t col_offset;
    int screen_rows;
    int screen_cols;
    char *filename;
    char *workspace_root;
    const char *include_path;
    const char *source_path;
    const char *compiler;
    bool dirty;
    bool quit_pending;
    bool open_pending;
    bool selecting;
    bool selection_active;
    size_t selection_anchor_x;
    size_t selection_anchor_y;
    size_t theme_index;
    size_t theme_prompt_saved_index;
    char message[160];
    char context_hint[384];
    DiagnosticList diagnostics;
    CgemSemantic semantic;
    bool semantic_dirty;
    bool semantic_force;
    clock_t semantic_debounce_until;
    bool pending_goto;
    size_t pending_goto_line;
    size_t pending_goto_column;
    char rename_qualified[256];
    bool follow_cursor;
    IdeMenu menu;
    EditorHistory history;
    EditorSnapshot saved_snapshot;
    LineChange *line_changes;
    bool *deletion_before;
    bool diff_dirty;
    IdePrompt prompt;
    char prompt_text[256];
    size_t prompt_length;
    char search_text[256];
    uint64_t revision;
    uint64_t saved_revision;
    uint64_t next_revision;
} Editor;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Buffer;

static Editor editor;

static const char IDE_HELP_HINT[] =
    "F1 Help | F2-F6 Menus | Ctrl+B Generate | Ctrl+K Format";

static Row *row_at(size_t index);
static size_t leading_spaces(const Row *row);
static bool cursor_inside_string(const Row *row, size_t cursor_x);
static void set_message(const char *format, ...);
static void update_context_hint(void);
static void history_record_edit(void);
static void clear_selection(void);
static void begin_prompt(IdePrompt prompt);
static void mark_semantic_dirty(void);
static void ensure_semantic_fresh(bool force);
static bool open_file_path(const char *path);
static bool save_file_as(const char *path);
static void row_replace_range(Row *row, size_t at, size_t old_len,
                              const char *new_text, size_t new_len);
static void apply_pending_goto(void);
static void goto_definition_at_cursor(void);
static void begin_rename_at_cursor(void);
static bool apply_symbol_rename(const char *new_short_name);

static void free_snapshot(EditorSnapshot *snapshot)
{
    for (size_t i = 0; i < snapshot->row_count; i++) {
        free(snapshot->rows[i].data);
    }
    free(snapshot->rows);
    *snapshot = (EditorSnapshot) {0};
}

static bool capture_snapshot(EditorSnapshot *snapshot)
{
    *snapshot = (EditorSnapshot) {0};
    if (editor.row_count > 0) {
        snapshot->rows = calloc(editor.row_count, sizeof(*snapshot->rows));
        if (!snapshot->rows) {
            return false;
        }
    }
    snapshot->row_count = editor.row_count;
    for (size_t i = 0; i < editor.row_count; i++) {
        Row *target = &snapshot->rows[i];
        const Row *source = &editor.rows[i];

        target->data = malloc(source->length + 1);
        if (!target->data) {
            free_snapshot(snapshot);
            return false;
        }
        memcpy(target->data, source->data, source->length + 1);
        target->length = source->length;
        target->capacity = source->length + 1;
    }
    snapshot->cursor_x = editor.cursor_x;
    snapshot->cursor_y = editor.cursor_y;
    snapshot->revision = editor.revision;
    return true;
}

static void update_saved_snapshot(void)
{
    EditorSnapshot snapshot;

    if (!capture_snapshot(&snapshot)) {
        return;
    }
    free_snapshot(&editor.saved_snapshot);
    editor.saved_snapshot = snapshot;
    editor.diff_dirty = true;
}

static void clear_snapshot_stack(EditorSnapshot *stack, size_t *count)
{
    while (*count > 0) {
        free_snapshot(&stack[--*count]);
    }
}

static void push_owned_snapshot(EditorSnapshot *stack, size_t *count,
                                EditorSnapshot snapshot)
{
    if (*count == IDE_HISTORY_LIMIT) {
        free_snapshot(&stack[0]);
        memmove(&stack[0], &stack[1],
                (IDE_HISTORY_LIMIT - 1) * sizeof(stack[0]));
        (*count)--;
    }
    stack[(*count)++] = snapshot;
}

static bool push_snapshot(EditorSnapshot *stack, size_t *count)
{
    EditorSnapshot snapshot;

    if (!capture_snapshot(&snapshot)) {
        return false;
    }
    push_owned_snapshot(stack, count, snapshot);
    return true;
}

static void history_record_edit(void)
{
    if (editor.history.suspended) {
        return;
    }
    if (push_snapshot(editor.history.undo, &editor.history.undo_count)) {
        clear_snapshot_stack(editor.history.redo, &editor.history.redo_count);
        editor.revision = ++editor.next_revision;
    }
    editor.open_pending = false;
}

static void restore_snapshot(EditorSnapshot *snapshot)
{
    for (size_t i = 0; i < editor.row_count; i++) {
        free(editor.rows[i].data);
    }
    free(editor.rows);
    editor.rows = snapshot->rows;
    editor.row_count = snapshot->row_count;
    editor.row_capacity = snapshot->row_count;
    editor.cursor_x = snapshot->cursor_x;
    editor.cursor_y = snapshot->cursor_y;
    editor.revision = snapshot->revision;
    editor.dirty = editor.revision != editor.saved_revision;
    snapshot->rows = NULL;
    snapshot->row_count = 0;
    editor.follow_cursor = true;
    editor.quit_pending = false;
    clear_selection();
    mark_semantic_dirty();
}

static bool history_step(bool redo)
{
    EditorSnapshot *from = redo ? editor.history.redo : editor.history.undo;
    EditorSnapshot *to = redo ? editor.history.undo : editor.history.redo;
    size_t *from_count = redo ? &editor.history.redo_count
                              : &editor.history.undo_count;
    size_t *to_count = redo ? &editor.history.undo_count
                            : &editor.history.redo_count;
    EditorSnapshot snapshot;

    if (*from_count == 0) {
        set_message(redo ? "Nothing to redo" : "Nothing to undo");
        return false;
    }
    if (!push_snapshot(to, to_count)) {
        set_message("History is out of memory");
        return false;
    }
    snapshot = from[--*from_count];
    restore_snapshot(&snapshot);
    set_message(redo ? "Redone" : "Undone");
    return true;
}

static struct {
    char last_file[512];
} ide_settings;

static const EditorTheme *current_theme(void)
{
    return theme_get(editor.theme_index);
}

#define BUFFER_LITERAL(buffer, text) buffer_append((buffer), (text), sizeof(text) - 1)

static void die(const char *message)
{
    platform_terminal_clear();
    perror(message);
    exit(EXIT_FAILURE);
}

static void buffer_append(Buffer *buffer, const char *data, size_t length)
{
    size_t needed = buffer->length + length;

    if (needed > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 1024;
        while (capacity < needed) {
            capacity *= 2;
        }
        buffer->data = realloc(buffer->data, capacity);
        if (!buffer->data) {
            die("realloc");
        }
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
}

static void buffer_printf(Buffer *buffer, const char *format, ...)
{
    char text[256];
    va_list args;
    int length;

    va_start(args, format);
    length = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (length > 0) {
        size_t used = (size_t) length < sizeof(text) ? (size_t) length : sizeof(text) - 1;
        buffer_append(buffer, text, used);
    }
}

static void mark_semantic_dirty(void)
{
    if (cg_diagnostic_has_errors(&editor.diagnostics)) {
        cg_diagnostic_clear(&editor.diagnostics);
    }
    editor.semantic_dirty = true;
    editor.semantic_debounce_until =
        clock() + (clock_t) (400 * (long) CLOCKS_PER_SEC / 1000);
    editor.diff_dirty = true;
}

static IdeIndexRow *snapshot_index_rows(size_t *row_count_out)
{
    IdeIndexRow *rows;
    size_t i;

    if (row_count_out) {
        *row_count_out = editor.row_count;
    }
    if (editor.row_count == 0) {
        return NULL;
    }
    rows = calloc(editor.row_count, sizeof(*rows));
    if (!rows) {
        return NULL;
    }
    for (i = 0; i < editor.row_count; i++) {
        if (!editor.rows[i].data) {
            free(rows);
            return NULL;
        }
        rows[i].data = editor.rows[i].data;
        rows[i].length = editor.rows[i].length;
    }
    return rows;
}

static void ensure_semantic_fresh(bool force)
{
    IdeIndexRow *rows;
    size_t row_count;
    const char *workspace =
        editor.workspace_root && editor.workspace_root[0]
            ? editor.workspace_root
            : NULL;
    const char *current_file = editor.filename ? editor.filename : NULL;

    if (!editor.semantic_dirty && !force) {
        return;
    }
    if (!force && !editor.semantic_force) {
        if (clock() < editor.semantic_debounce_until) {
            return;
        }
    }
    rows = snapshot_index_rows(&row_count);
    if (!rows && row_count > 0) {
        return;
    }
    cgem_semantic_analyze_rows(rows, row_count, editor.compiler,
                               editor.include_path, editor.source_path,
                               workspace, current_file, &editor.diagnostics,
                               &editor.semantic);
    free(rows);
    editor.semantic_dirty = false;
    editor.semantic_force = false;
    editor.semantic.analyzed_revision = editor.revision;
}

static void apply_pending_goto(void)
{
    Row *row;

    if (!editor.pending_goto) {
        return;
    }
    if (editor.pending_goto_line > 0 &&
        editor.pending_goto_line <= editor.row_count) {
        editor.cursor_y = editor.pending_goto_line - 1;
        row = row_at(editor.cursor_y);
        if (row) {
            size_t x = editor.pending_goto_column > 0
                           ? editor.pending_goto_column - 1
                           : 0;

            if (x > row->length) {
                x = row->length;
            }
            editor.cursor_x = x;
        }
    }
    editor.pending_goto = false;
    editor.follow_cursor = true;
    clear_selection();
}

static void goto_definition_at_cursor(void)
{
    Row *row = row_at(editor.cursor_y);
    IdeIndexRow *rows;
    size_t row_count;
    char reference[256];
    char scope[256];
    char qualified[256];
    size_t line;
    size_t column;
    char def_file[512];

    if (!row) {
        set_message("No symbol at cursor");
        return;
    }
    ensure_semantic_fresh(true);
    if (!cgem_semantic_reference_at(row->data, row->length, editor.cursor_x,
                                    reference, sizeof(reference), NULL)) {
        set_message("No symbol at cursor");
        return;
    }
    rows = snapshot_index_rows(&row_count);
    if (!rows && row_count > 0) {
        set_message("Go to definition failed");
        return;
    }
    if (!cgem_semantic_scope_path(rows, row_count, editor.cursor_y, scope,
                                  sizeof(scope))) {
        scope[0] = '\0';
    }
    if (!cgem_semantic_qualify_reference(reference, scope, &editor.semantic,
                                         qualified, sizeof(qualified)) ||
        !cgem_semantic_find_definition(&editor.semantic, qualified, &line,
                                       &column, def_file, sizeof(def_file))) {
        free(rows);
        set_message("Definition not found: %s", reference);
        return;
    }
    free(rows);
    if (def_file[0] && editor.filename &&
        strcmp(def_file, editor.filename) != 0) {
        editor.pending_goto = true;
        editor.pending_goto_line = line;
        editor.pending_goto_column = column;
        open_file_path(def_file);
        apply_pending_goto();
        set_message("Definition: %s", qualified);
        return;
    }
    if (line > 0 && line <= editor.row_count) {
        Row *target = row_at(line - 1);
        size_t x = column > 0 ? column - 1 : 0;

        editor.cursor_y = line - 1;
        if (target && x > target->length) {
            x = target->length;
        }
        editor.cursor_x = x;
    }
    editor.follow_cursor = true;
    clear_selection();
    set_message("Definition: %s", qualified);
}

static void begin_rename_at_cursor(void)
{
    Row *row = row_at(editor.cursor_y);
    IdeIndexRow *rows;
    size_t row_count;
    char reference[256];
    char scope[256];
    char qualified[256];
    size_t line;
    size_t column;
    char def_file[512];
    const char *short_name;

    if (!row) {
        set_message("No symbol at cursor");
        return;
    }
    ensure_semantic_fresh(true);
    if (!cgem_semantic_reference_at(row->data, row->length, editor.cursor_x,
                                    reference, sizeof(reference), NULL)) {
        set_message("No symbol at cursor");
        return;
    }
    rows = snapshot_index_rows(&row_count);
    if (!rows && row_count > 0) {
        set_message("Rename failed");
        return;
    }
    if (!cgem_semantic_scope_path(rows, row_count, editor.cursor_y, scope,
                                  sizeof(scope))) {
        scope[0] = '\0';
    }
    if (!cgem_semantic_qualify_reference(reference, scope, &editor.semantic,
                                         qualified, sizeof(qualified)) ||
        !cgem_semantic_find_definition(&editor.semantic, qualified, &line,
                                       &column, def_file, sizeof(def_file))) {
        free(rows);
        set_message("Definition not found: %s", reference);
        return;
    }
    free(rows);
    snprintf(editor.rename_qualified, sizeof(editor.rename_qualified), "%s",
             qualified);
    short_name = strrchr(editor.rename_qualified, '.');
    short_name = short_name ? short_name + 1 : editor.rename_qualified;
    begin_prompt(IDE_PROMPT_RENAME);
    snprintf(editor.prompt_text, sizeof(editor.prompt_text), "%s", short_name);
    editor.prompt_length = strlen(editor.prompt_text);
}

static bool apply_symbol_rename(const char *new_short_name)
{
    const char *last_dot;
    char new_qualified[256];
    IdeIndexRow *rows;
    size_t row_count;
    size_t replacements = 0;

    if (!new_short_name || !new_short_name[0] || !editor.rename_qualified[0]) {
        return false;
    }
    if (strchr(new_short_name, '.') || strchr(new_short_name, ' ')) {
        set_message("Invalid name");
        return false;
    }
    last_dot = strrchr(editor.rename_qualified, '.');
    if (last_dot) {
        size_t prefix_len = (size_t) (last_dot - editor.rename_qualified);

        if (prefix_len + 1 + strlen(new_short_name) + 1 >
            sizeof(new_qualified)) {
            set_message("Rename failed: name too long");
            return false;
        }
        memcpy(new_qualified, editor.rename_qualified, prefix_len);
        new_qualified[prefix_len] = '.';
        strcpy(new_qualified + prefix_len + 1, new_short_name);
    } else if (snprintf(new_qualified, sizeof(new_qualified), "%s",
                        new_short_name) >= (int) sizeof(new_qualified)) {
        set_message("Rename failed: name too long");
        return false;
    }
    if (strcmp(new_qualified, editor.rename_qualified) == 0) {
        set_message("Name unchanged");
        return true;
    }
    rows = snapshot_index_rows(&row_count);
    if (!rows && row_count > 0) {
        set_message("Rename failed");
        return false;
    }
    history_record_edit();
    for (size_t y = 0; y < editor.row_count; y++) {
        Row *target = &editor.rows[y];
        char scope[256];
        size_t scan = 0;

        if (!cgem_semantic_scope_path(rows, row_count, y, scope,
                                      sizeof(scope))) {
            scope[0] = '\0';
        }
        while (scan < target->length) {
            char reference[256];
            char qualified[256];
            size_t start_col;
            size_t at;
            size_t ref_len;
            size_t new_len;

            if (!cgem_semantic_reference_at(target->data, target->length, scan,
                                            reference, sizeof(reference),
                                            &start_col)) {
                break;
            }
            at = start_col > 0 ? start_col - 1 : 0;
            ref_len = strlen(reference);
            if (!cgem_semantic_qualify_reference(reference, scope,
                                                 &editor.semantic, qualified,
                                                 sizeof(qualified)) ||
                strcmp(qualified, editor.rename_qualified) != 0) {
                scan = at + ref_len;
                continue;
            }
            new_len = strlen(new_qualified);
            row_replace_range(target, at, ref_len, new_qualified, new_len);
            replacements++;
            if (editor.cursor_y == y && editor.cursor_x > at) {
                if (editor.cursor_x >= at + ref_len) {
                    editor.cursor_x =
                        editor.cursor_x - ref_len + new_len;
                }
            }
            scan = at + new_len;
        }
    }
    free(rows);
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
    editor.semantic_force = true;
    snprintf(editor.rename_qualified, sizeof(editor.rename_qualified), "%s",
             new_qualified);
    if (replacements > 0) {
        set_message("Renamed %zu occurrence(s) to %s", replacements,
                    new_qualified);
    } else {
        set_message("No occurrences renamed");
    }
    return replacements > 0;
}

static bool call_context_callee(const Row *row, size_t cursor, char *callee,
                                size_t callee_size)
{
    size_t depth = 0;
    size_t paren_pos = (size_t) -1;
    const char *token;
    size_t token_length;
    size_t token_end;

    if (!row || !callee || callee_size == 0 || cursor > row->length) {
        return false;
    }
    for (size_t i = cursor; i > 0; ) {
        char ch = row->data[i - 1];

        i--;
        if (ch == ')') {
            depth++;
        } else if (ch == '(') {
            if (depth == 0) {
                paren_pos = i;
                break;
            }
            depth--;
        }
    }
    if (paren_pos == (size_t) -1) {
        return false;
    }
    depth = 1;
    for (size_t i = paren_pos + 1; i < row->length; i++) {
        char ch = row->data[i];

        if (ch == '(') {
            depth++;
        } else if (ch == ')') {
            depth--;
            if (depth == 0) {
                if (cursor > i) {
                    return false;
                }
                break;
            }
        }
    }
    if (depth != 0) {
        return false;
    }
    if (!ide_index_completion_token_at(row->data, paren_pos, &token,
                                       &token_length)) {
        return false;
    }
    token_end = (size_t) (token - row->data) + token_length;
    while (token_end < paren_pos && row->data[token_end] == ' ') {
        token_end++;
    }
    if (token_end != paren_pos) {
        return false;
    }
    if (token_length + 1 > callee_size) {
        return false;
    }
    memcpy(callee, token, token_length);
    callee[token_length] = '\0';
    return true;
}

static bool row_is_blank(const Row *row)
{
    return !row || row->length == leading_spaces(row);
}

static bool cursor_after_colon(const Row *row, size_t cursor_x)
{
    size_t indent;
    size_t at;

    if (!row) {
        return false;
    }
    indent = leading_spaces(row);
    for (at = indent; at < row->length; at++) {
        if (row->data[at] == ':') {
            return cursor_x > at;
        }
    }
    return false;
}

static bool block_body_contains_cursor(size_t opener_y, size_t opener_indent,
                                       size_t cursor_y)
{
    if (cursor_y <= opener_y) {
        return false;
    }
    for (size_t z = opener_y + 1; z < cursor_y; z++) {
        Row *zrow = row_at(z);

        if (row_is_blank(zrow)) {
            continue;
        }
        if (leading_spaces(zrow) <= opener_indent) {
            return false;
        }
    }
    if (cursor_y > opener_y) {
        Row *cursor_row = row_at(cursor_y);

        if (!cursor_row) {
            return false;
        }
        if (!row_is_blank(cursor_row) &&
            leading_spaces(cursor_row) <= opener_indent) {
            return false;
        }
    }
    return true;
}

static bool block_call_context_callee(size_t cursor_y, size_t cursor_x,
                                      char *callee, size_t callee_size)
{
    for (size_t y = cursor_y; ; y--) {
        Row *row = row_at(y);
        size_t indent;
        const char *text;
        char *method = NULL;
        int written;

        if (!row || row_is_blank(row)) {
            if (y == 0) {
                break;
            }
            continue;
        }
        indent = leading_spaces(row);
        text = row->data + indent;
        if (!cg_parse_self_method_call_block_opener(text, &method)) {
            if (y == 0) {
                break;
            }
            continue;
        }
        written = snprintf(callee, callee_size, "self.%s", method);
        free(method);
        if (written < 0 || (size_t) written >= callee_size) {
            return false;
        }
        if (y == cursor_y) {
            if (cursor_after_colon(row, cursor_x)) {
                return true;
            }
        } else if (block_body_contains_cursor(y, indent, cursor_y)) {
            return true;
        }
        if (y == 0) {
            break;
        }
    }
    return false;
}

static const char *lookup_fn_hint(const char *callee)
{
    const char *hint;
    IdeIndexRow *rows;
    char scope[256];

    hint = cgem_semantic_fn_hint(&editor.semantic, NULL, callee);
    if (hint || editor.row_count == 0) {
        return hint;
    }
    rows = calloc(editor.row_count, sizeof(*rows));
    if (!rows) {
        return NULL;
    }
    for (size_t i = 0; i < editor.row_count; i++) {
        rows[i].data = editor.rows[i].data;
        rows[i].length = editor.rows[i].length;
    }
    if (cgem_semantic_scope_path(rows, editor.row_count, editor.cursor_y, scope,
                                 sizeof(scope))) {
        hint = cgem_semantic_fn_hint(&editor.semantic, scope, callee);
    }
    free(rows);
    return hint;
}

static void update_context_hint(void)
{
    Row *row;
    char callee[128];
    const char *hint;

    editor.context_hint[0] = '\0';
    if (editor.prompt != IDE_PROMPT_NONE) {
        return;
    }
    row = row_at(editor.cursor_y);
    if (!row) {
        return;
    }
    if (!call_context_callee(row, editor.cursor_x, callee, sizeof(callee)) &&
        !block_call_context_callee(editor.cursor_y, editor.cursor_x, callee,
                                   sizeof(callee))) {
        return;
    }
    hint = lookup_fn_hint(callee);
    if (hint) {
        snprintf(editor.context_hint, sizeof(editor.context_hint), "%s", hint);
    }
}

static bool rows_equal(const Row *left, const Row *right)
{
    return left->length == right->length &&
           memcmp(left->data, right->data, left->length) == 0;
}

static void mark_diff_gap(size_t saved_start, size_t saved_end,
                          size_t current_start, size_t current_end)
{
    size_t saved_count = saved_end - saved_start;
    size_t current_count = current_end - current_start;
    size_t paired = saved_count < current_count ? saved_count : current_count;

    for (size_t i = 0; i < paired; i++) {
        editor.line_changes[current_start + i] = LINE_CHANGE_MODIFIED;
    }
    for (size_t i = paired; i < current_count; i++) {
        editor.line_changes[current_start + i] = LINE_CHANGE_ADDED;
    }
    if (saved_count > paired) {
        editor.deletion_before[current_start + paired] = true;
    }
}

static void build_positional_diff(void)
{
    size_t saved_count = editor.saved_snapshot.row_count;
    size_t paired = saved_count < editor.row_count ? saved_count
                                                   : editor.row_count;

    for (size_t i = 0; i < paired; i++) {
        if (!rows_equal(&editor.saved_snapshot.rows[i], &editor.rows[i])) {
            editor.line_changes[i] = LINE_CHANGE_MODIFIED;
        }
    }
    for (size_t i = paired; i < editor.row_count; i++) {
        editor.line_changes[i] = LINE_CHANGE_ADDED;
    }
    if (saved_count > paired) {
        editor.deletion_before[paired] = true;
    }
}

static void ensure_diff_fresh(void)
{
    size_t saved_count = editor.saved_snapshot.row_count;
    size_t current_count = editor.row_count;
    size_t columns = current_count + 1;
    size_t cells;
    uint32_t *lcs;
    size_t saved_at = 0;
    size_t current_at = 0;
    size_t gap_saved = 0;
    size_t gap_current = 0;

    if (!editor.diff_dirty) {
        return;
    }
    free(editor.line_changes);
    free(editor.deletion_before);
    editor.line_changes = calloc(current_count, sizeof(*editor.line_changes));
    editor.deletion_before = calloc(current_count + 1,
                                    sizeof(*editor.deletion_before));
    editor.diff_dirty = false;
    if ((!editor.line_changes && current_count > 0) ||
        !editor.deletion_before) {
        free(editor.line_changes);
        free(editor.deletion_before);
        editor.line_changes = NULL;
        editor.deletion_before = NULL;
        return;
    }
    if (saved_count > SIZE_MAX / columns) {
        build_positional_diff();
        return;
    }
    cells = (saved_count + 1) * columns;
    if (cells > 4 * 1024 * 1024) {
        build_positional_diff();
        return;
    }
    lcs = calloc(cells, sizeof(*lcs));
    if (!lcs) {
        build_positional_diff();
        return;
    }
    for (size_t i = saved_count; i-- > 0;) {
        for (size_t j = current_count; j-- > 0;) {
            size_t cell = i * columns + j;

            if (rows_equal(&editor.saved_snapshot.rows[i], &editor.rows[j])) {
                lcs[cell] = lcs[(i + 1) * columns + j + 1] + 1;
            } else {
                uint32_t skip_saved = lcs[(i + 1) * columns + j];
                uint32_t skip_current = lcs[i * columns + j + 1];

                lcs[cell] = skip_saved > skip_current ? skip_saved
                                                      : skip_current;
            }
        }
    }
    while (saved_at < saved_count && current_at < current_count) {
        if (rows_equal(&editor.saved_snapshot.rows[saved_at],
                       &editor.rows[current_at])) {
            mark_diff_gap(gap_saved, saved_at, gap_current, current_at);
            saved_at++;
            current_at++;
            gap_saved = saved_at;
            gap_current = current_at;
        } else if (lcs[(saved_at + 1) * columns + current_at] >=
                   lcs[saved_at * columns + current_at + 1]) {
            saved_at++;
        } else {
            current_at++;
        }
    }
    mark_diff_gap(gap_saved, saved_count, gap_current, current_count);
    free(lcs);
}

static bool row_diagnostic(size_t file_row, DiagnosticSeverity *severity)
{
    size_t line = file_row + 1;
    bool found = false;
    DiagnosticSeverity worst = DIAG_NOTE;

    if (!severity) {
        return false;
    }
    for (size_t i = 0; i < editor.diagnostics.count; i++) {
        const Diagnostic *item = &editor.diagnostics.items[i];

        if (item->line != line) {
            continue;
        }
        if (item->severity == DIAG_ERROR) {
            *severity = DIAG_ERROR;
            return true;
        }
        if (item->severity == DIAG_WARNING) {
            worst = DIAG_WARNING;
            found = true;
        } else if (!found) {
            worst = DIAG_NOTE;
            found = true;
        }
    }
    if (found) {
        *severity = worst;
    }
    return found;
}

static void focus_first_diagnostic(DiagnosticSeverity severity)
{
    for (size_t i = 0; i < editor.diagnostics.count; i++) {
        const Diagnostic *item = &editor.diagnostics.items[i];
        size_t row;

        if (item->severity != severity || item->line == 0) {
            continue;
        }
        row = item->line - 1;
        if (row >= editor.row_count) {
            continue;
        }
        editor.cursor_y = row;
        if (item->column > 0 && item->column - 1 <= editor.rows[row].length) {
            editor.cursor_x = item->column - 1;
        } else {
            editor.cursor_x = 0;
        }
        editor.follow_cursor = true;
        return;
    }
}

static void set_message(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vsnprintf(editor.message, sizeof(editor.message), format, args);
    va_end(args);
}

static void clear_selection(void)
{
    editor.selecting = false;
    editor.selection_active = false;
}

static char *path_dirname(const char *path)
{
    size_t length = strlen(path);
    size_t end = length;

    while (end > 0 && (path[end - 1] == '/' || path[end - 1] == '\\')) {
        end--;
    }
    if (end == 0) {
        return strdup(".");
    }
    size_t slash = end;
    while (slash > 0 && path[slash - 1] != '/' && path[slash - 1] != '\\') {
        slash--;
    }
    if (slash == 0) {
        if (path[0] == '/' || path[0] == '\\') {
            return strdup("/");
        }
        return strdup(".");
    }
    char *out = malloc(slash + 1);
    if (!out) {
        die("malloc");
    }
    memcpy(out, path, slash);
    out[slash] = '\0';
    return out;
}

static void file_relative_to_workspace(const char *filepath, char *out,
                                       size_t out_size)
{
    const char *root = editor.workspace_root ? editor.workspace_root : ".";

    if (strcmp(root, ".") == 0) {
        if (filepath[0] == '.' && filepath[1] == '/') {
            snprintf(out, out_size, "%s", filepath + 2);
            return;
        }
        snprintf(out, out_size, "%s", filepath);
        return;
    }
    size_t root_len = strlen(root);
    if (strncmp(filepath, root, root_len) == 0 &&
        (filepath[root_len] == '\0' || filepath[root_len] == '/')) {
        const char *rel = filepath + root_len;
        if (*rel == '/') {
            rel++;
        }
        snprintf(out, out_size, "%s", rel);
        return;
    }
    snprintf(out, out_size, "%s", filepath);
}

static void load_ide_settings(void)
{
    char path[1024];
    FILE *input;
    char line[640];

    ide_settings.last_file[0] = '\0';
    snprintf(path, sizeof(path), "%s/.cgem/settings", editor.workspace_root);
    input = fopen(path, "r");
    if (!input) {
        return;
    }
    while (fgets(line, sizeof(line), input)) {
        char *key = line;
        char *value;
        size_t length = strlen(line);

        while (length > 0 &&
               (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (length == 0 || line[0] == '#') {
            continue;
        }
        value = strchr(line, '=');
        if (!value) {
            continue;
        }
        *value++ = '\0';
        if (strcmp(key, "theme") == 0) {
            size_t index = theme_find_index(value);
            if (index < theme_count()) {
                editor.theme_index = index;
            }
        } else if (strcmp(key, "file") == 0) {
            strncpy(ide_settings.last_file, value,
                    sizeof(ide_settings.last_file) - 1);
            ide_settings.last_file[sizeof(ide_settings.last_file) - 1] = '\0';
        }
    }
    fclose(input);
}

static void save_ide_settings(void)
{
    char settings_dir[512];
    char path[576];
    char rel[512];
    char error[128];
    FILE *output;

    if (!editor.workspace_root || !editor.filename) {
        return;
    }
    snprintf(settings_dir, sizeof(settings_dir), "%s/.cgem",
             editor.workspace_root);
    if (platform_mkdir_p(settings_dir, error, sizeof(error)) != 0) {
        return;
    }
    snprintf(path, sizeof(path), "%s/settings", settings_dir);
    output = fopen(path, "w");
    if (!output) {
        return;
    }
    file_relative_to_workspace(editor.filename, rel, sizeof(rel));
    fprintf(output, "theme=%s\n", current_theme()->name);
    fprintf(output, "file=%s\n", rel);
    fclose(output);
}

static bool theme_word_is_acronym(const char *word, size_t len)
{
    if (len <= 2) {
        return true;
    }
    if (len == 3 && strncmp(word, "cmd", 3) == 0) {
        return true;
    }
    return false;
}

static void format_theme_display_name(const char *id, char *out, size_t out_size)
{
    size_t i = 0;

    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (!id) {
        return;
    }
    while (*id && i + 1 < out_size) {
        const char *word = id;
        size_t len = 0;

        while (id[len] && id[len] != '-') {
            len++;
        }
        if (len > 0) {
            size_t w;

            if (i > 0) {
                out[i++] = ' ';
            }
            for (w = 0; w < len && i + 1 < out_size; w++) {
                char ch = word[w];

                if (theme_word_is_acronym(word, len)) {
                    if (ch >= 'a' && ch <= 'z') {
                        ch = (char) (ch - 'a' + 'A');
                    }
                } else if (w == 0 && ch >= 'a' && ch <= 'z') {
                    ch = (char) (ch - 'a' + 'A');
                }
                out[i++] = ch;
            }
        }
        if (id[len] == '-') {
            id += len + 1;
        } else {
            break;
        }
    }
    out[i] = '\0';
}

static void update_theme_prompt_label(void)
{
    format_theme_display_name(current_theme()->name, editor.prompt_text,
                              sizeof(editor.prompt_text));
    editor.prompt_length = strlen(editor.prompt_text);
}

static void begin_theme_prompt(void)
{
    editor.theme_prompt_saved_index = editor.theme_index;
    editor.prompt = IDE_PROMPT_THEME;
    update_theme_prompt_label();
    ide_menu_close(&editor.menu);
}

static void preview_theme(int direction)
{
    size_t count = theme_count();

    if (count == 0) {
        return;
    }
    if (direction < 0) {
        editor.theme_index = (editor.theme_index + count - 1) % count;
    } else {
        editor.theme_index = (editor.theme_index + 1) % count;
    }
    update_theme_prompt_label();
}

static void update_window_size(void)
{
    platform_terminal_update_size(&editor.screen_rows, &editor.screen_cols);
}

static Row *row_at(size_t index)
{
    return index < editor.row_count ? &editor.rows[index] : NULL;
}

static void ensure_row_capacity(Row *row, size_t needed)
{
    if (needed <= row->capacity) {
        return;
    }
    size_t capacity = row->capacity ? row->capacity : 32;
    while (capacity < needed) {
        capacity *= 2;
    }
    row->data = realloc(row->data, capacity);
    if (!row->data) {
        die("realloc");
    }
    row->capacity = capacity;
}

static void insert_row(size_t index, const char *data, size_t length)
{
    if (editor.row_count == editor.row_capacity) {
        size_t capacity = editor.row_capacity ? editor.row_capacity * 2 : 32;
        editor.rows = realloc(editor.rows, capacity * sizeof(*editor.rows));
        if (!editor.rows) {
            die("realloc");
        }
        editor.row_capacity = capacity;
    }
    memmove(&editor.rows[index + 1], &editor.rows[index],
            (editor.row_count - index) * sizeof(*editor.rows));
    editor.rows[index] = (Row) {0};
    ensure_row_capacity(&editor.rows[index], length + 1);
    memcpy(editor.rows[index].data, data, length);
    editor.rows[index].data[length] = '\0';
    editor.rows[index].length = length;
    editor.row_count++;
}

static void delete_row(size_t index)
{
    free(editor.rows[index].data);
    memmove(&editor.rows[index], &editor.rows[index + 1],
            (editor.row_count - index - 1) * sizeof(*editor.rows));
    editor.row_count--;
}

static void row_insert_char(Row *row, size_t at, char ch)
{
    ensure_row_capacity(row, row->length + 2);
    memmove(row->data + at + 1, row->data + at, row->length - at + 1);
    row->data[at] = ch;
    row->length++;
}

static void row_replace_range(Row *row, size_t at, size_t old_len,
                              const char *new_text, size_t new_len)
{
    size_t tail;
    size_t new_total;

    if (old_len == 0 && new_len == 0) {
        return;
    }
    if (at > row->length || at + old_len > row->length) {
        return;
    }
    new_total = row->length - old_len + new_len;
    ensure_row_capacity(row, new_total + 1);
    tail = row->length - at - old_len;
    if (new_len != old_len) {
        memmove(row->data + at + new_len, row->data + at + old_len, tail + 1);
    }
    if (new_len > 0) {
        memcpy(row->data + at, new_text, new_len);
    }
    row->length = new_total;
    row->data[row->length] = '\0';
}

static void insert_char(char ch)
{
    Row *row;

    history_record_edit();
    editor.follow_cursor = true;
    if (editor.cursor_y == editor.row_count) {
        insert_row(editor.row_count, "", 0);
    }
    row = &editor.rows[editor.cursor_y];
    if (ch == ')') {
        if (editor.cursor_x < row->length &&
            row->data[editor.cursor_x] == ')') {
            editor.cursor_x++;
            return;
        }
    }
    if (ch == '"') {
        if (editor.cursor_x < row->length &&
            row->data[editor.cursor_x] == '"') {
            editor.cursor_x++;
            return;
        }
        if (!cursor_inside_string(row, editor.cursor_x)) {
            row_insert_char(row, editor.cursor_x, '"');
            row_insert_char(row, editor.cursor_x + 1, '"');
            editor.cursor_x++;
            editor.dirty = true;
            editor.quit_pending = false;
            mark_semantic_dirty();
            return;
        }
    }
    if (ch == '(') {
        row_insert_char(row, editor.cursor_x, '(');
        row_insert_char(row, editor.cursor_x + 1, ')');
        editor.cursor_x++;
        editor.dirty = true;
        editor.quit_pending = false;
        mark_semantic_dirty();
        return;
    }
    row_insert_char(row, editor.cursor_x, ch);
    editor.cursor_x++;
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
}

static size_t leading_spaces(const Row *row)
{
    size_t count = 0;

    while (count < row->length && row->data[count] == ' ') {
        count++;
    }
    return count;
}

static size_t scan_highlight_string(const Row *row, size_t at, size_t *string_start,
                                    size_t *string_end)
{
    bool escaped = false;

    *string_start = 0;
    *string_end = 0;
    if (at >= row->length || row->data[at] != '"') {
        return at;
    }
    *string_start = at++;
    while (at < row->length) {
        char ch = row->data[at++];

        if (ch == '"' && !escaped) {
            break;
        }
        escaped = ch == '\\' && !escaped;
    }
    *string_end = at;
    return at;
}

static bool cursor_inside_string(const Row *row, size_t cursor_x)
{
    bool in_string = false;
    bool escaped = false;

    if (!row) {
        return false;
    }
    for (size_t i = 0; i < cursor_x && i < row->length; i++) {
        char ch = row->data[i];

        if (in_string) {
            if (ch == '"' && !escaped) {
                in_string = false;
            }
            escaped = ch == '\\' && !escaped;
            if (ch != '\\') {
                escaped = false;
            }
        } else if (ch == '"') {
            in_string = true;
            escaped = false;
        }
    }
    return in_string;
}

static bool row_ends_with_colon_attribute(const Row *row, size_t at,
                                            const char *name)
{
    size_t name_length = strlen(name);
    size_t pos;

    if (row->length - at < 1 + name_length + 1 || row->data[at] != '@') {
        return false;
    }
    if (memcmp(row->data + at + 1, name, name_length) != 0) {
        return false;
    }
    if (at + 1 + name_length < row->length &&
        cg_name_char((unsigned char) row->data[at + 1 + name_length])) {
        return false;
    }
    pos = at + 1 + name_length;
    while (pos < row->length && row->data[pos] == ' ') {
        pos++;
    }
    if (pos >= row->length || row->data[pos] != ':') {
        return false;
    }
    pos++;
    while (pos < row->length && row->data[pos] == ' ') {
        pos++;
    }
    return pos == row->length;
}

static bool row_is_block_require_spec_line(const Row *row)
{
    size_t at = leading_spaces(row);
    ParamRequire require = cg_param_require_any();
    bool parsed;

    while (at < row->length && row->data[at] == ' ') {
        at++;
    }
    parsed = cg_parse_require_spec(row->data + at, &require);
    cg_param_require_free(&require);
    return parsed;
}

static bool row_is_block_attribute_string_line(const Row *row)
{
    size_t at = leading_spaces(row);
    size_t string_start = 0;
    size_t string_end = 0;
    size_t end;

    while (at < row->length && row->data[at] == ' ') {
        at++;
    }
    end = scan_highlight_string(row, at, &string_start, &string_end);
    while (end < row->length && row->data[end] == ' ') {
        end++;
    }
    return string_start != 0 && end == row->length;
}

static bool row_is_muted_attribute_line(size_t row_index)
{
    const Row *row;
    size_t at;
    size_t prev_indent;

    if (row_index >= editor.row_count) {
        return false;
    }
    row = &editor.rows[row_index];
    at = leading_spaces(row);
    if (row_ends_with_colon_attribute(row, at, "doc") ||
        row_ends_with_colon_attribute(row, at, "include") ||
        row_ends_with_colon_attribute(row, at, "require")) {
        return true;
    }
    if (!row_is_block_attribute_string_line(row) &&
        !row_is_block_require_spec_line(row)) {
        return false;
    }
    if (row_index == 0) {
        return false;
    }
    prev_indent = leading_spaces(&editor.rows[row_index - 1]);
    if (at == prev_indent + 4 &&
        (row_ends_with_colon_attribute(&editor.rows[row_index - 1], prev_indent,
                                       "doc") ||
         row_ends_with_colon_attribute(&editor.rows[row_index - 1], prev_indent,
                                       "include") ||
         row_ends_with_colon_attribute(&editor.rows[row_index - 1], prev_indent,
                                       "require"))) {
        return true;
    }
    if (at == prev_indent &&
        row_is_block_attribute_string_line(&editor.rows[row_index - 1]) &&
        row_is_muted_attribute_line(row_index - 1)) {
        return true;
    }
    return false;
}

static const char *block_value_ghost(size_t row_index)
{
    const Row *row;
    size_t indent;
    size_t prev;
    const Row *parent;
    size_t parent_indent;

    if (row_index >= editor.row_count || editor.cursor_y != row_index) {
        return NULL;
    }
    row = &editor.rows[row_index];
    if (editor.cursor_x != row->length) {
        return NULL;
    }
    indent = leading_spaces(row);
    if (row->length != indent) {
        return NULL;
    }
    if (row_index == 0) {
        return NULL;
    }
    prev = row_index - 1;
    parent = &editor.rows[prev];
    parent_indent = leading_spaces(parent);
    if (indent != parent_indent + 4) {
        return NULL;
    }
    if (row_ends_with_colon_attribute(parent, parent_indent, "doc") ||
        row_ends_with_colon_attribute(parent, parent_indent, "include") ||
        row_is_muted_attribute_line(prev)) {
        return "\"";
    }
    return NULL;
}

static void insert_indent(void)
{
    size_t spaces = 4 - (editor.cursor_x % 4);

    history_record_edit();
    editor.history.suspended = true;
    while (spaces-- > 0) {
        insert_char(' ');
    }
    editor.history.suspended = false;
}

static void selection_bounds(size_t *start_y, size_t *start_x,
                             size_t *end_y, size_t *end_x);

static const char *doc_tag_ghost(const Row *row, size_t indent)
{
    size_t at = indent;

    if (row->length - at >= strlen("@doc:") &&
        memcmp(row->data + at, "@doc:", strlen("@doc:")) == 0) {
        at += strlen("@doc:");
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
        if (at == row->length) {
            return NULL;
        }
    } else if (row->length - at >= strlen("@doc") &&
               memcmp(row->data + at, "@doc", strlen("@doc")) == 0 &&
               (row->length - at == strlen("@doc") ||
                row->data[at + strlen("@doc")] == '(')) {
        at += strlen("@doc");
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
        if (at == row->length) {
            return "(\"";
        }
        if (row->data[at] == '(') {
            at++;
            while (at < row->length && row->data[at] == ' ') {
                at++;
            }
            if (at == row->length) {
                return "\"";
            }
        }
    }
    if (row->length - at >= strlen("@include:") &&
        memcmp(row->data + at, "@include:", strlen("@include:")) == 0) {
        at += strlen("@include:");
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
        if (at == row->length) {
            return NULL;
        }
    } else if (row->length - at >= strlen("@include") &&
               memcmp(row->data + at, "@include", strlen("@include")) == 0 &&
               (row->length - at == strlen("@include") ||
                row->data[at + strlen("@include")] == '(')) {
        at += strlen("@include");
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
        if (at == row->length) {
            return "(\"";
        }
        if (row->data[at] == '(') {
            at++;
            while (at < row->length && row->data[at] == ' ') {
                at++;
            }
            if (at == row->length) {
                return "\"";
            }
        }
    }
    return NULL;
}

static const char *index_completion_ghost(const Row *row, size_t row_index,
                                          size_t cursor_x)
{
    const char *completion_token;
    size_t completion_length;
    char scope[256];
    IdeIndexRow rows[512];
    size_t row_count = editor.row_count;

    if (!ide_index_completion_token_at(row->data, cursor_x, &completion_token,
                                    &completion_length)) {
        return NULL;
    }
    if (row_index < row_count && row_count <= sizeof(rows) / sizeof(rows[0])) {
        const char *suffix;

        for (size_t i = 0; i < row_count; i++) {
            rows[i].data = editor.rows[i].data;
            rows[i].length = editor.rows[i].length;
        }
        if (cgem_semantic_scope_path(rows, row_count, row_index, scope,
                                     sizeof(scope))) {
            suffix = cgem_semantic_ghost_suffix(&editor.semantic, scope,
                                                completion_token,
                                                completion_length);
        } else {
            suffix = cgem_semantic_ghost_suffix(&editor.semantic, NULL,
                                                completion_token,
                                                completion_length);
        }
        if (suffix && suffix[0] != '\0') {
            return suffix;
        }
        return NULL;
    }
    {
        const char *suffix =
            cgem_semantic_ghost_suffix(&editor.semantic, NULL, completion_token,
                                       completion_length);

        if (suffix && suffix[0] != '\0') {
            return suffix;
        }
    }
    return NULL;
}

static bool completion_suffix_is_punctuation(const Row *row, size_t cursor_x)
{
    if (cursor_x > row->length) {
        return false;
    }
    for (size_t at = cursor_x; at < row->length; at++) {
        char ch = row->data[at];

        if (ch == ' ') {
            continue;
        }
        if (ch == ')' || ch == ',') {
            continue;
        }
        return false;
    }
    return cursor_x < row->length;
}

static bool ghost_cursor_ok(const Row *row, size_t cursor_x)
{
    if (cursor_x > row->length) {
        return false;
    }
    if (cursor_x == row->length) {
        return true;
    }
    return completion_suffix_is_punctuation(row, cursor_x);
}

static const char *keyword_ghost(const Row *row, size_t row_index,
                                 bool *trailing_space)
{
    static const char *keywords[] = {
        "scope", "module", "package", "type", "enum", "case", "let",
        "struct", "field", "param", "fn", "return", "use"
    };
    static const char *attributes[] = {
        "noscope", "include", "public", "private", "internal", "extern",
        "opaque", "define", "doc", "mutable", "pointer", "used", "require",
        "initializer"
    };
    static const char *members[] = {
        "c.void", "c.bool", "c.char", "c.schar", "c.uchar", "c.short",
        "c.sshort", "c.ushort", "c.int", "c.sint", "c.uint", "c.long",
        "c.slong", "c.ulong", "c.llong", "c.sllong", "c.ullong",
        "c.float", "c.double", "c.ldouble", "c.initializer"
    };
    size_t indent = leading_spaces(row);
    size_t typed = row->length - indent;
    bool inline_completion = editor.cursor_x < row->length &&
                             completion_suffix_is_punctuation(row, editor.cursor_x);

    if (trailing_space) {
        *trailing_space = false;
    }
    if (editor.cursor_y >= editor.row_count ||
        row != &editor.rows[editor.cursor_y] ||
        !ghost_cursor_ok(row, editor.cursor_x) ||
        typed == 0) {
        return NULL;
    }
    if (row_index != SIZE_MAX) {
        const char *value_ghost = block_value_ghost(row_index);

        if (value_ghost) {
            return value_ghost;
        }
    }
    if (!inline_completion) {
    if (row->data[indent] == '@') {
        const char *doc_tag = doc_tag_ghost(row, indent);

        if (doc_tag) {
            return doc_tag;
        }
        size_t typed_attribute = typed - 1;

        if (typed_attribute == 0) {
            return NULL;
        }
        for (size_t i = 0;
             i < sizeof(attributes) / sizeof(attributes[0]); i++) {
            size_t length = strlen(attributes[i]);

            if (typed_attribute < length &&
                memcmp(row->data + indent + 1,
                       attributes[i], typed_attribute) == 0) {
                return attributes[i] + typed_attribute;
            }
        }
        return NULL;
    }
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        size_t length = strlen(keywords[i]);

        if (typed < length &&
            memcmp(row->data + indent, keywords[i], typed) == 0) {
            if (trailing_space) {
                *trailing_space = true;
            }
            return keywords[i] + typed;
        }
    }
    if (typed >= strlen("enum x a")) {
        size_t prefix_length = 0;

        if (memcmp(row->data + indent, "type ", strlen("type ")) == 0) {
            prefix_length = strlen("type ");
        } else if (memcmp(row->data + indent, "enum ",
                          strlen("enum ")) == 0) {
            prefix_length = strlen("enum ");
        } else if (memcmp(row->data + indent, "field ",
                          strlen("field ")) == 0) {
            prefix_length = strlen("field ");
        }
        size_t at = indent + prefix_length;

        if (prefix_length && at < row->length &&
            cg_name_start((unsigned char) row->data[at])) {
            while (at < row->length &&
                   cg_name_char((unsigned char) row->data[at])) {
                at++;
            }
            while (at < row->length && row->data[at] == ' ') {
                at++;
            }
            if (at + 1 == row->length && row->data[at] == 'a') {
                if (trailing_space) {
                    *trailing_space = true;
                }
                return "s";
            }
        }
    }
    }
    {
    const char *completion_token;
    size_t completion_length;
    const char *suffix;

    if (!ide_index_completion_token_at(row->data, editor.cursor_x,
                                        &completion_token, &completion_length)) {
        return NULL;
    }
    if (strstr(row->data, " as ") != NULL) {
        suffix = index_completion_ghost(row, row_index, editor.cursor_x);
        if (suffix) {
            return suffix;
        }
    }
    for (size_t i = 0; i < sizeof(members) / sizeof(members[0]); i++) {
        size_t member_length = strlen(members[i]);

        if (completion_length >= 2 &&
            completion_length < member_length &&
            memcmp(completion_token, members[i], completion_length) == 0) {
            return members[i] + completion_length;
        }
    }
    suffix = index_completion_ghost(row, row_index, editor.cursor_x);
    if (suffix) {
        return suffix;
    }
    }
    return NULL;
}

static bool accept_keyword_hint(void)
{
    Row *row;
    const char *ghost;
    size_t length;
    size_t insert_at;
    bool trailing_space;
    bool inline_hint;

    if (editor.cursor_y >= editor.row_count) {
        return false;
    }
    row = &editor.rows[editor.cursor_y];
    ghost = keyword_ghost(row, editor.cursor_y, &trailing_space);
    if (!ghost) {
        return false;
    }

    history_record_edit();
    length = strlen(ghost);
    inline_hint = editor.cursor_x < row->length &&
                  completion_suffix_is_punctuation(row, editor.cursor_x);
    insert_at = inline_hint ? editor.cursor_x : row->length;
    ensure_row_capacity(row, row->length + length +
                             (trailing_space ? 2 : 1));
    if (inline_hint) {
        memmove(row->data + insert_at + length, row->data + insert_at,
                row->length - insert_at + 1);
    }
    memcpy(row->data + insert_at, ghost, length);
    row->length += length;
    if (trailing_space) {
        row->data[row->length++] = ' ';
    }
    row->data[row->length] = '\0';
    editor.cursor_x = insert_at + length;
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
    return true;
}

static void remove_indent(void)
{
    Row *row;
    size_t remove;

    if (editor.cursor_y >= editor.row_count) {
        return;
    }
    row = &editor.rows[editor.cursor_y];
    remove = leading_spaces(row);
    if (remove > 4) {
        remove = 4;
    }
    if (!remove) {
        return;
    }

    history_record_edit();
    memmove(row->data, row->data + remove, row->length - remove + 1);
    row->length -= remove;
    editor.cursor_x = editor.cursor_x > remove ? editor.cursor_x - remove : 0;
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
}

static void indent_selection(void)
{
    size_t start_y;
    size_t start_x;
    size_t end_y;
    size_t end_x;

    if (!editor.selection_active) {
        insert_indent();
        return;
    }
    history_record_edit();
    selection_bounds(&start_y, &start_x, &end_y, &end_x);
    if (start_y == end_y) {
        Row *row = row_at(start_y);

        if (!row) {
            return;
        }
        ensure_row_capacity(row, row->length + 4 + 1);
        memmove(row->data + start_x + 4, row->data + start_x,
                row->length - start_x + 1);
        memset(row->data + start_x, ' ', 4);
        row->length += 4;
        if (editor.selection_anchor_y == start_y &&
            editor.selection_anchor_x >= start_x) {
            editor.selection_anchor_x += 4;
        }
        if (editor.cursor_y == start_y && editor.cursor_x >= start_x) {
            editor.cursor_x += 4;
        }
    } else {
        for (size_t y = start_y; y <= end_y; y++) {
            Row *row = row_at(y);

            if (!row) {
                continue;
            }
            ensure_row_capacity(row, row->length + 4 + 1);
            memmove(row->data + 4, row->data, row->length + 1);
            memset(row->data, ' ', 4);
            row->length += 4;
        }
        if (editor.selection_anchor_y == start_y) {
            editor.selection_anchor_x += 4;
        }
        if (editor.cursor_y == end_y) {
            editor.cursor_x += 4;
        }
    }
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
}

static void unindent_selection(void)
{
    size_t start_y;
    size_t start_x;
    size_t end_y;
    size_t end_x;

    if (!editor.selection_active) {
        remove_indent();
        return;
    }
    selection_bounds(&start_y, &start_x, &end_y, &end_x);
    if (start_y == end_y) {
        remove_indent();
        return;
    }
    history_record_edit();
    for (size_t y = start_y; y <= end_y; y++) {
        Row *row = row_at(y);
        size_t remove;

        if (!row) {
            continue;
        }
        remove = leading_spaces(row);
        if (remove > 4) {
            remove = 4;
        }
        if (!remove) {
            continue;
        }
        memmove(row->data, row->data + remove, row->length - remove + 1);
        row->length -= remove;
    }
    if (editor.selection_anchor_y == start_y &&
        editor.selection_anchor_x >= 4) {
        editor.selection_anchor_x -= 4;
    }
    if (editor.cursor_y == end_y && editor.cursor_x >= 4) {
        editor.cursor_x -= 4;
    }
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
}

static void insert_newline(void)
{
    Row *row;
    size_t indent = 0;
    bool opens_block = false;

    history_record_edit();
    if (editor.cursor_y == editor.row_count) {
        insert_row(editor.row_count, "", 0);
    } else {
        row = &editor.rows[editor.cursor_y];
        indent = leading_spaces(row);
        if (editor.cursor_x > indent) {
            size_t at = editor.cursor_x;

            while (at > indent && row->data[at - 1] == ' ') {
                at--;
            }
            opens_block = at > indent && row->data[at - 1] == ':';
        }
        insert_row(editor.cursor_y + 1, row->data + editor.cursor_x,
                   row->length - editor.cursor_x);
        row = &editor.rows[editor.cursor_y];
        row->length = editor.cursor_x;
        row->data[row->length] = '\0';
    }
    editor.cursor_y++;
    if (opens_block) {
        indent += 4;
    }
    row = &editor.rows[editor.cursor_y];
    for (size_t i = 0; i < indent; i++) {
        row_insert_char(row, i, ' ');
    }
    editor.cursor_x = indent;
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
}

static void insert_newline_plain(void)
{
    Row *row;

    if (editor.cursor_y == editor.row_count) {
        insert_row(editor.row_count, "", 0);
    } else {
        row = &editor.rows[editor.cursor_y];
        insert_row(editor.cursor_y + 1, row->data + editor.cursor_x,
                   row->length - editor.cursor_x);
        row->length = editor.cursor_x;
        row->data[row->length] = '\0';
    }
    editor.cursor_y++;
    editor.cursor_x = 0;
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
}

static void delete_char(void)
{
    Row *row;

    if (editor.cursor_y == editor.row_count) {
        return;
    }
    row = &editor.rows[editor.cursor_y];
    if (editor.cursor_x == 0 && editor.cursor_y == 0) {
        return;
    }
    history_record_edit();
    if (editor.cursor_x > 0) {
        size_t remove = 1;

        if (editor.cursor_x < row->length &&
            editor.cursor_x > 0 &&
            row->data[editor.cursor_x - 1] == '(' &&
            row->data[editor.cursor_x] == ')') {
            memmove(row->data + editor.cursor_x - 1,
                    row->data + editor.cursor_x + 1,
                    row->length - editor.cursor_x);
            row->length -= 2;
            editor.cursor_x--;
            editor.dirty = true;
            editor.quit_pending = false;
            mark_semantic_dirty();
            return;
        }
        if (editor.cursor_x < row->length &&
            editor.cursor_x > 0 &&
            row->data[editor.cursor_x - 1] == '"' &&
            row->data[editor.cursor_x] == '"' &&
            !cursor_inside_string(row, editor.cursor_x - 1)) {
            memmove(row->data + editor.cursor_x - 1,
                    row->data + editor.cursor_x + 1,
                    row->length - editor.cursor_x);
            row->length -= 2;
            editor.cursor_x--;
            editor.dirty = true;
            editor.quit_pending = false;
            mark_semantic_dirty();
            return;
        }
        if (editor.cursor_x <= leading_spaces(row)) {
            size_t target = ((editor.cursor_x - 1) / 4) * 4;
            remove = editor.cursor_x - target;
        }
        memmove(row->data + editor.cursor_x - remove,
                row->data + editor.cursor_x,
                row->length - editor.cursor_x + 1);
        row->length -= remove;
        editor.cursor_x -= remove;
    } else if (editor.cursor_y > 0) {
        size_t previous_length = editor.rows[editor.cursor_y - 1].length;
        Row *previous = &editor.rows[editor.cursor_y - 1];
        ensure_row_capacity(previous, previous->length + row->length + 1);
        memcpy(previous->data + previous->length, row->data, row->length + 1);
        previous->length += row->length;
        delete_row(editor.cursor_y);
        editor.cursor_y--;
        editor.cursor_x = previous_length;
    }
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
}

static void delete_char_forward(void)
{
    Row *row;

    if (editor.cursor_y == editor.row_count) {
        return;
    }
    row = &editor.rows[editor.cursor_y];
    if (editor.cursor_x >= row->length &&
        editor.cursor_y + 1 >= editor.row_count) {
        return;
    }
    history_record_edit();
    if (editor.cursor_x < row->length) {
        memmove(row->data + editor.cursor_x, row->data + editor.cursor_x + 1,
                row->length - editor.cursor_x);
        row->length--;
    } else if (editor.cursor_y + 1 < editor.row_count) {
        Row *next = &editor.rows[editor.cursor_y + 1];

        ensure_row_capacity(row, row->length + next->length + 1);
        memcpy(row->data + row->length, next->data, next->length + 1);
        row->length += next->length;
        delete_row(editor.cursor_y + 1);
    }
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
}

static int gutter_width(void);

static void move_cursor(int key)
{
    Row *row = row_at(editor.cursor_y);

    editor.follow_cursor = true;
    switch (key) {
    case KEY_ARROW_LEFT:
    case KEY_SHIFT_ARROW_LEFT:
        if (editor.cursor_x > 0) {
            editor.cursor_x--;
        } else if (editor.cursor_y > 0) {
            editor.cursor_y--;
            editor.cursor_x = editor.rows[editor.cursor_y].length;
        }
        break;
    case KEY_ARROW_RIGHT:
    case KEY_SHIFT_ARROW_RIGHT:
        if (row && editor.cursor_x < row->length) {
            editor.cursor_x++;
        } else if (row && editor.cursor_y + 1 < editor.row_count) {
            editor.cursor_y++;
            editor.cursor_x = 0;
        }
        break;
    case KEY_ARROW_UP:
    case KEY_SHIFT_ARROW_UP:
        if (editor.cursor_y > 0) {
            editor.cursor_y--;
        }
        break;
    case KEY_ARROW_DOWN:
    case KEY_SHIFT_ARROW_DOWN:
        if (editor.cursor_y + 1 < editor.row_count) {
            editor.cursor_y++;
        }
        break;
    }

    row = row_at(editor.cursor_y);
    if (row && editor.cursor_x > row->length) {
        editor.cursor_x = row->length;
    }
}

#define STICKY_SCROLL_MAX 5
#define EDITOR_EOF_SCROLL_PADDING 1
#define CALL_PUNCTUATION_MAX 4

typedef struct {
    size_t sticky_rows;
    int scroll_rows;
    size_t max_offset;
} EditorScrollGeom;

typedef struct {
    size_t rows[STICKY_SCROLL_MAX];
    size_t count;
} StickyScroll;

static size_t prepare_sticky_scroll(size_t first_visible, int content_rows,
                                    StickyScroll *sticky);
static void collect_sticky_scroll(size_t before_row, StickyScroll *sticky);

static EditorScrollGeom editor_scroll_geometry(size_t row_offset, int content_rows)
{
    StickyScroll sticky;
    EditorScrollGeom geom;
    size_t scroll_span = editor.row_count + EDITOR_EOF_SCROLL_PADDING;

    geom.sticky_rows = prepare_sticky_scroll(row_offset, content_rows, &sticky);
    geom.scroll_rows = content_rows - (int) geom.sticky_rows;
    if (geom.scroll_rows < 1) {
        geom.scroll_rows = 1;
    }
    if (scroll_span <= (size_t) geom.scroll_rows) {
        geom.max_offset = 0;
    } else {
        geom.max_offset = scroll_span - (size_t) geom.scroll_rows;
    }
    return geom;
}

static void clamp_row_offset(int content_rows)
{
    EditorScrollGeom geom =
        editor_scroll_geometry(editor.row_offset, content_rows);

    if (editor.row_offset > geom.max_offset) {
        editor.row_offset = geom.max_offset;
    }
}

static int gutter_width(void);
static int editor_screen_top(void);
static int editor_content_rows(void);

static bool move_cursor_to_screen(int row, int col)
{
    int content_rows = editor_content_rows();
    int gutter = gutter_width();
    int text_row = row - editor_screen_top();
    int text_col = col - gutter - 1;
    EditorScrollGeom geom =
        editor_scroll_geometry(editor.row_offset, content_rows);
    size_t file_row;
    Row *target;
    bool sticky_click = false;

    if (content_rows < 1) {
        content_rows = 1;
    }
    if (text_row < 0 || text_col < 0) {
        return false;
    }
    if (text_row < (int) geom.sticky_rows) {
        StickyScroll sticky;

        collect_sticky_scroll(editor.row_offset, &sticky);
        if ((size_t) text_row >= sticky.count) {
            return false;
        }
        file_row = sticky.rows[text_row];
        sticky_click = true;
    } else {
        text_row -= (int) geom.sticky_rows;
        if (text_row >= geom.scroll_rows) {
            return false;
        }
        file_row = editor.row_offset + (size_t) text_row;
    }
    if (file_row >= editor.row_count) {
        editor.cursor_y = editor.row_count;
        editor.cursor_x = 0;
        editor.quit_pending = false;
        return sticky_click;
    }
    editor.cursor_y = file_row;
    editor.cursor_x = editor.col_offset + (size_t) text_col;
    editor.follow_cursor = true;
    target = row_at(editor.cursor_y);
    if (target && editor.cursor_x > target->length) {
        editor.cursor_x = target->length;
    }
    editor.quit_pending = false;
    return sticky_click;
}

static void update_selection_active(void)
{
    editor.selection_active =
        editor.selection_anchor_y != editor.cursor_y ||
        editor.selection_anchor_x != editor.cursor_x;
}

static void scroll_editor_by_lines(int lines)
{
    int content_rows = editor_content_rows();
    EditorScrollGeom geom;

    if (lines == 0) {
        return;
    }
    editor.follow_cursor = false;
    geom = editor_scroll_geometry(editor.row_offset, content_rows);
    if (lines < 0) {
        size_t up = (size_t) (-lines);

        editor.row_offset =
            editor.row_offset > up ? editor.row_offset - up : 0;
    } else {
        size_t down = (size_t) lines;

        if (editor.row_offset + down > geom.max_offset) {
            editor.row_offset = geom.max_offset;
        } else {
            editor.row_offset += down;
        }
    }
    clamp_row_offset(content_rows);
}

static void page_editor(bool up)
{
    int content_rows = editor_content_rows();
    EditorScrollGeom geom;
    size_t page;

    editor.follow_cursor = false;
    geom = editor_scroll_geometry(editor.row_offset, content_rows);
    page = (size_t) geom.scroll_rows;

    if (up) {
        editor.row_offset =
            editor.row_offset > page ? editor.row_offset - page : 0;
        if (editor.cursor_y >= page) {
            editor.cursor_y -= page;
        } else {
            editor.cursor_y = 0;
        }
    } else {
        if (editor.row_offset + page > geom.max_offset) {
            editor.row_offset = geom.max_offset;
        } else {
            editor.row_offset += page;
        }
        if (editor.cursor_y + page < editor.row_count) {
            editor.cursor_y += page;
        } else if (editor.row_count > 0) {
            editor.cursor_y = editor.row_count - 1;
        } else {
            editor.cursor_y = 0;
        }
    }
    clamp_row_offset(content_rows);
}

static bool format_document(void);
static bool save_file(void);
static bool generate_output(void);
static bool cut_selection(void);
static bool copy_selection(void);
static bool paste_clipboard(bool formatted);

static void select_all(void)
{
    editor.selection_anchor_y = 0;
    editor.selection_anchor_x = 0;
    if (editor.row_count == 0) {
        editor.cursor_y = 0;
        editor.cursor_x = 0;
        editor.selection_active = false;
        return;
    }
    editor.cursor_y = editor.row_count - 1;
    editor.cursor_x = editor.rows[editor.cursor_y].length;
    editor.selection_active = editor.row_count > 1 || editor.cursor_x > 0;
    editor.follow_cursor = true;
    set_message("Selected all");
}

static void begin_prompt(IdePrompt prompt)
{
    editor.prompt = prompt;
    editor.prompt_length = 0;
    editor.prompt_text[0] = '\0';
    if (prompt == IDE_PROMPT_FIND && editor.search_text[0]) {
        snprintf(editor.prompt_text, sizeof(editor.prompt_text), "%s",
                 editor.search_text);
        editor.prompt_length = strlen(editor.prompt_text);
    }
    ide_menu_close(&editor.menu);
}

static bool find_next_match(const char *needle)
{
    size_t start_row;
    size_t start_col;

    if (!needle || !needle[0] || editor.row_count == 0) {
        set_message("Enter text to find");
        return false;
    }
    start_row = editor.cursor_y < editor.row_count ? editor.cursor_y : 0;
    start_col = editor.cursor_y < editor.row_count ? editor.cursor_x : 0;
    for (size_t pass = 0; pass < 2; pass++) {
        size_t first = pass == 0 ? start_row : 0;
        size_t last = pass == 0 ? editor.row_count : start_row + 1;

        for (size_t y = first; y < last; y++) {
            const Row *row = &editor.rows[y];
            size_t from = pass == 0 && y == start_row ? start_col : 0;
            const char *match;

            if (from > row->length) {
                from = row->length;
            }
            match = strstr(row->data + from, needle);
            if (match && !(pass == 1 && y == start_row &&
                           (size_t) (match - row->data) >= start_col)) {
                size_t x = (size_t) (match - row->data);

                editor.selection_anchor_y = y;
                editor.selection_anchor_x = x;
                editor.cursor_y = y;
                editor.cursor_x = x + strlen(needle);
                editor.selection_active = true;
                editor.follow_cursor = true;
                set_message("Found: %s", needle);
                return true;
            }
        }
    }
    set_message("Not found: %s", needle);
    return false;
}

static void finish_prompt(void)
{
    if (editor.prompt == IDE_PROMPT_OPEN) {
        editor.prompt = IDE_PROMPT_NONE;
        if (!editor.prompt_text[0]) {
            set_message("Enter a file path");
            return;
        }
        open_file_path(editor.prompt_text);
        return;
    }
    if (editor.prompt == IDE_PROMPT_SAVE_AS) {
        editor.prompt = IDE_PROMPT_NONE;
        if (!editor.prompt_text[0]) {
            set_message("Enter a file path");
            return;
        }
        save_file_as(editor.prompt_text);
        return;
    }
    if (editor.prompt == IDE_PROMPT_FIND) {
        snprintf(editor.search_text, sizeof(editor.search_text), "%s",
                 editor.prompt_text);
        editor.prompt = IDE_PROMPT_NONE;
        find_next_match(editor.search_text);
        return;
    }
    if (editor.prompt == IDE_PROMPT_GOTO_LINE) {
        char *end;
        long line = strtol(editor.prompt_text, &end, 10);

        editor.prompt = IDE_PROMPT_NONE;
        if (!editor.prompt_text[0] || *end != '\0' || line < 1 ||
            (editor.row_count == 0 ? line > 1
                                   : (size_t) line > editor.row_count)) {
            set_message("Line must be between 1 and %zu",
                        editor.row_count > 0 ? editor.row_count : 1);
            return;
        }
        editor.cursor_y = (size_t) line - 1;
        if (editor.row_count == 0) {
            editor.cursor_x = 0;
        } else if (editor.cursor_x > editor.rows[editor.cursor_y].length) {
            editor.cursor_x = editor.rows[editor.cursor_y].length;
        }
        editor.follow_cursor = true;
        clear_selection();
        set_message("Moved to line %ld", line);
        return;
    }
    if (editor.prompt == IDE_PROMPT_RENAME) {
        editor.prompt = IDE_PROMPT_NONE;
        if (!editor.prompt_text[0]) {
            set_message("Enter a new name");
            return;
        }
        apply_symbol_rename(editor.prompt_text);
        return;
    }
    if (editor.prompt == IDE_PROMPT_THEME) {
        char theme_label[256];

        editor.prompt = IDE_PROMPT_NONE;
        save_ide_settings();
        format_theme_display_name(current_theme()->name, theme_label,
                                  sizeof(theme_label));
        set_message("Theme selected: %s", theme_label);
    }
}

static void handle_prompt_key(int key)
{
    if (editor.prompt == IDE_PROMPT_THEME) {
        if (key == '\x1b') {
            editor.theme_index = editor.theme_prompt_saved_index;
            editor.prompt = IDE_PROMPT_NONE;
            set_message("Cancelled");
            return;
        }
        if (key == '\r') {
            finish_prompt();
            return;
        }
        if (key == KEY_ARROW_LEFT) {
            preview_theme(-1);
            return;
        }
        if (key == KEY_ARROW_RIGHT) {
            preview_theme(1);
            return;
        }
        return;
    }

    if (key == 27) {
        editor.prompt = IDE_PROMPT_NONE;
        set_message("Cancelled");
    } else if (key == '\r') {
        finish_prompt();
    } else if (key == KEY_BACKSPACE || key == 8) {
        if (editor.prompt_length > 0) {
            editor.prompt_text[--editor.prompt_length] = '\0';
        }
    } else if (key >= 32 && key <= 126 &&
               editor.prompt_length + 1 < sizeof(editor.prompt_text)) {
        bool allow = true;

        if (editor.prompt == IDE_PROMPT_GOTO_LINE) {
            allow = key >= '0' && key <= '9';
        } else if (editor.prompt == IDE_PROMPT_RENAME) {
            if (editor.prompt_length == 0) {
                allow = cg_name_start((unsigned char) key);
            } else {
                allow = cg_name_char((unsigned char) key);
            }
        }
        if (allow) {
            editor.prompt_text[editor.prompt_length++] = (char) key;
            editor.prompt_text[editor.prompt_length] = '\0';
        }
    }
}

static void editor_indent_action(void)
{
    if (editor.selection_active) {
        indent_selection();
    } else if (!accept_keyword_hint()) {
        insert_indent();
    }
}

static void editor_unindent_action(void)
{
    unindent_selection();
}

static bool handle_menu_action(IdeMenuAction action)
{
    switch (action) {
    case IDE_MENU_ACTION_OPEN:
        if (editor.dirty && !editor.open_pending) {
            editor.open_pending = true;
            set_message("Unsaved changes. Choose Open again to discard.");
            return true;
        }
        editor.open_pending = false;
        begin_prompt(IDE_PROMPT_OPEN);
        return true;
    case IDE_MENU_ACTION_SAVE:
        save_file();
        return true;
    case IDE_MENU_ACTION_SAVE_AS:
        begin_prompt(IDE_PROMPT_SAVE_AS);
        return true;
    case IDE_MENU_ACTION_QUIT:
        if (editor.dirty && !editor.quit_pending) {
            editor.quit_pending = true;
            set_message("Unsaved changes. Press Quit again to discard.");
            return true;
        }
        return false;
    case IDE_MENU_ACTION_COPY:
        copy_selection();
        return true;
    case IDE_MENU_ACTION_CUT:
        cut_selection();
        return true;
    case IDE_MENU_ACTION_PASTE:
        paste_clipboard(false);
        return true;
    case IDE_MENU_ACTION_PASTE_FORMATTED:
        paste_clipboard(true);
        return true;
    case IDE_MENU_ACTION_SELECT_ALL:
        select_all();
        return true;
    case IDE_MENU_ACTION_INDENT:
        editor_indent_action();
        return true;
    case IDE_MENU_ACTION_UNINDENT:
        editor_unindent_action();
        return true;
    case IDE_MENU_ACTION_UNDO:
        history_step(false);
        return true;
    case IDE_MENU_ACTION_REDO:
        history_step(true);
        return true;
    case IDE_MENU_ACTION_FIND:
        begin_prompt(IDE_PROMPT_FIND);
        return true;
    case IDE_MENU_ACTION_FIND_NEXT:
        find_next_match(editor.search_text);
        return true;
    case IDE_MENU_ACTION_GOTO_LINE:
        begin_prompt(IDE_PROMPT_GOTO_LINE);
        return true;
    case IDE_MENU_ACTION_GOTO_DEFINITION:
        goto_definition_at_cursor();
        return true;
    case IDE_MENU_ACTION_RENAME:
        begin_rename_at_cursor();
        return true;
    case IDE_MENU_ACTION_FORMAT:
        format_document();
        return true;
    case IDE_MENU_ACTION_THEME:
        begin_theme_prompt();
        return true;
    case IDE_MENU_ACTION_HELP:
        set_message(IDE_HELP_HINT);
        return true;
    case IDE_MENU_ACTION_GENERATE:
        generate_output();
        return true;
    case IDE_MENU_ACTION_NONE:
        break;
    }
    return true;
}

static bool handle_mouse_event(const PlatformEvent *event)
{
    IdeMenuAction action = IDE_MENU_ACTION_NONE;

    if (event->kind == PLATFORM_EVENT_MOUSE_DOWN &&
        ide_menu_handle_mouse(&editor.menu, event->row, event->col, true,
                              editor.screen_cols, &action)) {
        if (action != IDE_MENU_ACTION_NONE &&
            !handle_menu_action(action)) {
            return false;
        }
        return true;
    }
    if (ide_menu_is_open(&editor.menu) &&
        event->kind == PLATFORM_EVENT_MOUSE_DOWN) {
        ide_menu_handle_mouse(&editor.menu, event->row, event->col, true,
                              editor.screen_cols, NULL);
        return true;
    }
    if (event->kind == PLATFORM_EVENT_MOUSE_SCROLL_UP) {
        scroll_editor_by_lines(3);
        return true;
    }
    if (event->kind == PLATFORM_EVENT_MOUSE_SCROLL_DOWN) {
        scroll_editor_by_lines(-3);
        return true;
    }
    if (event->button == PLATFORM_MOUSE_BUTTON_RIGHT) {
        if (event->kind == PLATFORM_EVENT_MOUSE_DOWN) {
            if (editor.selection_active) {
                copy_selection();
            } else {
                paste_clipboard(false);
            }
            clear_selection();
            editor.selecting = false;
            move_cursor_to_screen(event->row, event->col);
        }
        return true;
    }
    if (event->button == PLATFORM_MOUSE_BUTTON_MIDDLE) {
        if (event->kind == PLATFORM_EVENT_MOUSE_DOWN) {
            paste_clipboard(false);
        }
        return true;
    }
    if (event->kind == PLATFORM_EVENT_MOUSE_DOWN) {
        if (move_cursor_to_screen(event->row, event->col)) {
            clear_selection();
            editor.selecting = false;
        } else {
            editor.selection_anchor_x = editor.cursor_x;
            editor.selection_anchor_y = editor.cursor_y;
            editor.selecting = true;
            editor.selection_active = false;
        }
    } else if (event->kind == PLATFORM_EVENT_MOUSE_DRAG) {
        if (event->button != PLATFORM_MOUSE_BUTTON_LEFT) {
            return true;
        }
        if (!editor.selecting) {
            return true;
        }
        move_cursor_to_screen(event->row, event->col);
        update_selection_active();
    } else if (event->kind == PLATFORM_EVENT_MOUSE_UP) {
        if (event->button != PLATFORM_MOUSE_BUTTON_LEFT) {
            return true;
        }
        if (!editor.selecting) {
            return true;
        }
        move_cursor_to_screen(event->row, event->col);
        update_selection_active();
        editor.selecting = false;
    }
    return true;
}

static int line_number_digits(void)
{
    size_t lines = editor.row_count > 0 ? editor.row_count : 1;
    int digits = 1;

    while (lines >= 10) {
        lines /= 10;
        digits++;
    }
    return digits;
}

static int gutter_width(void)
{
    int width = 1 + 1 + line_number_digits() + 2 * GUTTER_SIDE_PAD;

    return editor.screen_cols > width + 1 ? width : 0;
}

static int editor_chrome_rows(void)
{
    return 3;
}

static int editor_screen_top(void)
{
    return 3;
}

static int editor_content_rows(void)
{
    int rows = editor.screen_rows - editor_chrome_rows();

    return rows < 1 ? 1 : rows;
}

static void scroll_to_cursor(void)
{
    int content_rows = editor_content_rows();
    int available_cols = editor.screen_cols - gutter_width();
    size_t content_cols = available_cols > 1 ? (size_t) available_cols : 1;

    if (!editor.follow_cursor) {
        return;
    }

    {
        Row *row = row_at(editor.cursor_y);

        if (row && editor.cursor_x > row->length) {
            editor.cursor_x = row->length;
        }
        if (row && row->length > 0 && editor.col_offset >= row->length) {
            editor.col_offset = 0;
        }
    }

    for (int pass = 0; pass < 8; pass++) {
        EditorScrollGeom geom =
            editor_scroll_geometry(editor.row_offset, content_rows);
        size_t new_offset = editor.row_offset;

        if (editor.cursor_y < editor.row_offset) {
            new_offset = editor.cursor_y;
        } else if (editor.cursor_y >= editor.row_offset +
                                        (size_t) geom.scroll_rows) {
            new_offset = editor.cursor_y - (size_t) geom.scroll_rows + 1;
        } else if (editor.row_count > 0 &&
                   editor.cursor_y + 1 == editor.row_count &&
                   geom.scroll_rows > 1) {
            size_t preferred = editor.row_count + EDITOR_EOF_SCROLL_PADDING -
                               (size_t) geom.scroll_rows;

            if (preferred <= geom.max_offset) {
                new_offset = preferred;
            }
        }
        if (new_offset > geom.max_offset) {
            new_offset = geom.max_offset;
        }
        if (new_offset == editor.row_offset) {
            break;
        }
        editor.row_offset = new_offset;
    }

    clamp_row_offset(content_rows);

    if (editor.cursor_x < editor.col_offset) {
        editor.col_offset = editor.cursor_x;
    }
    if (editor.cursor_x >= editor.col_offset + content_cols) {
        editor.col_offset = editor.cursor_x + 1 > (size_t) content_cols
                                ? editor.cursor_x - (size_t) content_cols + 1
                                : 0;
    }
}

static int display_width(wchar_t wide)
{
#ifdef _WIN32
    return wide <= 0xFF ? 1 : 2;
#else
    int columns = wcwidth(wide);

    return columns > 0 ? columns : 0;
#endif
}

static int text_width(const char *text)
{
    mbstate_t state = {0};
    int width = 0;

    while (*text) {
        wchar_t wide;
        size_t length = mbrtowc(&wide, text, MB_CUR_MAX, &state);

        if (length == (size_t) -1 || length == (size_t) -2) {
            memset(&state, 0, sizeof(state));
            text++;
            width++;
            continue;
        }
        if (length == 0) {
            break;
        }
        width += display_width(wide);
        text += length;
    }
    return width;
}

static void draw_bar(Buffer *buffer, const char *left, const char *right,
                     const char *colors)
{
    int left_width = text_width(left);
    int right_width = text_width(right);
    int spaces = editor.screen_cols - left_width - right_width;

    buffer_append(buffer, colors, strlen(colors));
    if (left_width <= editor.screen_cols) {
        buffer_append(buffer, left, strlen(left));
    }
    while (spaces-- > 0) {
        buffer_append(buffer, " ", 1);
    }
    if (left_width < editor.screen_cols &&
        right_width <= editor.screen_cols - left_width) {
        buffer_append(buffer, right, strlen(right));
    }
    buffer_append(buffer, "\x1b[0m", 4);
}

static void draw_centered_bar(Buffer *buffer, const char *label, const char *colors)
{
    int label_width = text_width(label);
    int left_pad = label_width < editor.screen_cols
                       ? (editor.screen_cols - label_width) / 2
                       : 0;
    int right_pad = label_width < editor.screen_cols
                        ? editor.screen_cols - label_width - left_pad
                        : 0;

    buffer_append(buffer, colors, strlen(colors));
    while (left_pad-- > 0) {
        buffer_append(buffer, " ", 1);
    }
    if (label_width <= editor.screen_cols) {
        buffer_append(buffer, label, strlen(label));
    }
    while (right_pad-- > 0) {
        buffer_append(buffer, " ", 1);
    }
}

static void screen_begin_row(Buffer *buffer, int row, const char *background)
{
    buffer_printf(buffer, "\x1b[%d;1H", row);
    if (background && background[0] != '\0') {
        buffer_append(buffer, background, strlen(background));
        BUFFER_LITERAL(buffer, "\x1b[0K");
    }
}

static void screen_end_row(Buffer *buffer, const char *background)
{
    if (background && background[0] != '\0') {
        buffer_append(buffer, background, strlen(background));
        BUFFER_LITERAL(buffer, "\x1b[0K");
    }
}

static size_t declaration_keyword_length(const Row *row, size_t at)
{
    static const char *keywords[] = {
        "scope", "module", "package", "case", "let", "struct", "param",
        "fn", "return", "if", "elif", "else"
    };

    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        size_t length = strlen(keywords[i]);

        if (row->length - at >= length &&
            memcmp(row->data + at, keywords[i], length) == 0) {
            char next = at + length < row->length ? row->data[at + length] : '\0';

            if (next == '\0' || next == ' ' || next == ':') {
                return length;
            }
        }
    }
    return 0;
}

static size_t decl_as_keyword_length(const Row *row, size_t at)
{
    static const char *keywords[] = {"type", "enum", "field", "let", "fn",
                                     "return"};

    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        size_t length = strlen(keywords[i]);

        if (row->length - at > length &&
            memcmp(row->data + at, keywords[i], length) == 0 &&
            row->data[at + length] == ' ') {
            return length;
        }
    }
    return 0;
}

static size_t skip_identifier(const Row *row, size_t at)
{
    if (at >= row->length ||
        !cg_name_start((unsigned char) row->data[at])) {
        return at;
    }
    at++;
    while (at < row->length &&
           cg_name_char((unsigned char) row->data[at])) {
        at++;
    }
    return at;
}

static bool span_is_module_export(const Row *row, size_t start, size_t end)
{
    return end > start && (size_t) (end - start) == 6 &&
           memcmp(row->data + start, "module", 6) == 0;
}

static bool row_starts_with(const Row *row, size_t at, const char *word)
{
    size_t length = strlen(word);

    return row->length - at > length &&
           memcmp(row->data + at, word, length) == 0 &&
           row->data[at + length] == ' ';
}

static bool row_named_declaration(const Row *row, const char *keyword,
                                  char *name, size_t name_size)
{
    size_t at = leading_spaces(row);
    size_t kw_len = strlen(keyword);
    size_t name_start;
    size_t name_end;

    if (row->length - at < kw_len + 1) {
        return false;
    }
    if (memcmp(row->data + at, keyword, kw_len) != 0 ||
        row->data[at + kw_len] != ' ') {
        return false;
    }
    at += kw_len + 1;
    while (at < row->length && row->data[at] == ' ') {
        at++;
    }
    name_start = at;
    at = skip_identifier(row, at);
    if (at == name_start) {
        return false;
    }
    name_end = at;
    while (name_end < row->length && row->data[name_end] == ' ') {
        name_end++;
    }
    if (name_end >= row->length || row->data[name_end] != ':') {
        return false;
    }
    if ((size_t) (name_end - name_start) >= name_size) {
        return false;
    }
    memcpy(name, row->data + name_start, name_end - name_start);
    name[name_end - name_start] = '\0';
    return true;
}

static bool row_scope_declaration(const Row *row)
{
    char name[64];

    return row_named_declaration(row, "package", name, sizeof(name)) ||
           row_named_declaration(row, "module", name, sizeof(name)) ||
           row_named_declaration(row, "scope", name, sizeof(name));
}

static void collect_sticky_scroll(size_t before_row, StickyScroll *sticky)
{
    typedef struct {
        size_t indent;
        size_t row;
    } ScopeFrame;

    ScopeFrame scopes[16];
    size_t scope_count = 0;

    sticky->count = 0;
    if (before_row == 0) {
        return;
    }

    for (size_t y = 0; y < before_row && y < editor.row_count; y++) {
        Row *row = &editor.rows[y];
        size_t indent = leading_spaces(row);

        if (!row_scope_declaration(row)) {
            continue;
        }
        while (scope_count > 0 &&
               scopes[scope_count - 1].indent >= indent) {
            scope_count--;
        }
        if (scope_count >= sizeof(scopes) / sizeof(scopes[0])) {
            continue;
        }
        scopes[scope_count].indent = indent;
        scopes[scope_count].row = y;
        scope_count++;
    }

    for (size_t i = 0; i < scope_count && sticky->count < STICKY_SCROLL_MAX;
         i++) {
        sticky->rows[sticky->count++] = scopes[i].row;
    }
}

static size_t prepare_sticky_scroll(size_t first_visible, int content_rows,
                                    StickyScroll *sticky)
{
    size_t max_sticky;

    collect_sticky_scroll(first_visible, sticky);
    if (sticky->count == 0) {
        return 0;
    }
    max_sticky = content_rows > 1 ? (size_t) content_rows - 1 : 0;
    if (max_sticky > STICKY_SCROLL_MAX) {
        max_sticky = STICKY_SCROLL_MAX;
    }
    if (sticky->count > max_sticky) {
        sticky->count = max_sticky;
    }
    return sticky->count;
}

static void append_gutter_bar_column(Buffer *buffer, const char *gutter_bg,
                                     const char *change_fg)
{
    buffer_append(buffer, gutter_bg, strlen(gutter_bg));
    if (change_fg) {
        buffer_append(buffer, change_fg, strlen(change_fg));
        buffer_append(buffer, DIFF_BAR_GLYPH, sizeof(DIFF_BAR_GLYPH) - 1);
    } else {
        buffer_append(buffer, " ", 1);
    }
}

static void draw_gutter(Buffer *buffer, size_t file_row, bool has_row)
{
    int width = gutter_width();
    int digits = line_number_digits();
    const EditorTheme *theme = current_theme();
    DiagnosticSeverity severity;
    bool diagnostic = row_diagnostic(file_row, &severity);
    const char *change_bar = NULL;
    const char *gutter_bg;
    const char *line_theme;

    if (!width) {
        return;
    }
    if (editor.deletion_before && file_row <= editor.row_count &&
        editor.deletion_before[file_row]) {
        change_bar = theme->diff_deleted;
    } else if (has_row && editor.line_changes &&
               editor.line_changes[file_row] == LINE_CHANGE_ADDED) {
        change_bar = theme->diff_added;
    } else if (has_row && editor.line_changes &&
               editor.line_changes[file_row] == LINE_CHANGE_MODIFIED) {
        change_bar = theme->diff_modified;
    }
    if (has_row) {
        int pad;

        if (diagnostic && severity == DIAG_ERROR) {
            gutter_bg = theme->error_line;
            line_theme = theme->error_gutter;
        } else if (diagnostic) {
            gutter_bg = theme->warning_line;
            line_theme = theme->warning_gutter;
        } else if (file_row == editor.cursor_y) {
            gutter_bg = theme->editor_active;
            line_theme = theme->gutter_line_active;
        } else if (file_row % 2 == 1) {
            gutter_bg = theme->stripe.editor;
            line_theme = theme->gutter_line_stripe;
        } else {
            gutter_bg = theme->editor;
            line_theme = theme->gutter_line;
        }
        append_gutter_bar_column(buffer, gutter_bg, change_bar);
        for (pad = 0; pad < GUTTER_SIDE_PAD; pad++) {
            buffer_append(buffer, " ", 1);
        }
        buffer_append(buffer, line_theme, strlen(line_theme));
        buffer_printf(buffer, "%*zu", digits, file_row + 1);
        buffer_append(buffer, gutter_bg, strlen(gutter_bg));
        for (pad = 0; pad < GUTTER_SIDE_PAD; pad++) {
            buffer_append(buffer, " ", 1);
        }
        buffer_append(buffer, gutter_bg, strlen(gutter_bg));
        buffer_append(buffer, " ", 1);
    } else {
        int pad;
        const char *bg = theme->editor;

        append_gutter_bar_column(buffer, bg, change_bar);
        for (pad = 0; pad < GUTTER_SIDE_PAD; pad++) {
            buffer_append(buffer, " ", 1);
        }
        BUFFER_LITERAL(buffer, bg);
        for (pad = 0; pad < digits; pad++) {
            buffer_append(buffer, " ", 1);
        }
        for (pad = 0; pad < GUTTER_SIDE_PAD; pad++) {
            buffer_append(buffer, " ", 1);
        }
        buffer_append(buffer, bg, strlen(bg));
        buffer_append(buffer, " ", 1);
    }
}

static void draw_editor_row(Buffer *buffer, const Row *row, int content_cols,
                            size_t file_row, bool active, bool mark_diagnostic,
                            DiagnosticSeverity line_diag);

static void draw_sticky_row(Buffer *buffer, const Row *row, size_t file_row,
                            int content_cols)
{
    draw_gutter(buffer, file_row, true);
    draw_editor_row(buffer, row, content_cols, file_row,
                    file_row == editor.cursor_y, false, DIAG_NOTE);
}

static bool position_before(size_t ay, size_t ax, size_t by, size_t bx)
{
    return ay < by || (ay == by && ax < bx);
}

static bool position_selected(size_t row, size_t col)
{
    size_t start_y = editor.selection_anchor_y;
    size_t start_x = editor.selection_anchor_x;
    size_t end_y = editor.cursor_y;
    size_t end_x = editor.cursor_x;

    if (!editor.selection_active) {
        return false;
    }
    if (position_before(end_y, end_x, start_y, start_x)) {
        size_t temp_y = start_y;
        size_t temp_x = start_x;

        start_y = end_y;
        start_x = end_x;
        end_y = temp_y;
        end_x = temp_x;
    }
    if (row < start_y || row > end_y) {
        return false;
    }
    if (start_y == end_y) {
        return row == start_y && col >= start_x && col < end_x;
    }
    if (row == start_y) {
        return col >= start_x;
    }
    if (row == end_y) {
        return col < end_x;
    }
    return true;
}

static void selection_bounds(size_t *start_y, size_t *start_x,
                             size_t *end_y, size_t *end_x)
{
    *start_y = editor.selection_anchor_y;
    *start_x = editor.selection_anchor_x;
    *end_y = editor.cursor_y;
    *end_x = editor.cursor_x;
    if (position_before(*end_y, *end_x, *start_y, *start_x)) {
        size_t temp_y = *start_y;
        size_t temp_x = *start_x;

        *start_y = *end_y;
        *start_x = *end_x;
        *end_y = temp_y;
        *end_x = temp_x;
    }
}

static char *grow_selected_text(char *text, size_t *capacity, size_t length,
                                  size_t extra)
{
    size_t needed = length + extra + 1;
    char *grown;

    if (*capacity >= needed) {
        return text;
    }
    needed = needed < 64 ? 64 : needed;
    grown = realloc(text, needed);
    if (!grown) {
        free(text);
        return NULL;
    }
    *capacity = needed;
    return grown;
}

static char *selected_text(void)
{
    size_t start_y;
    size_t start_x;
    size_t end_y;
    size_t end_x;
    char *text;
    size_t length = 0;
    size_t capacity = 0;

    if (!editor.selection_active) {
        return NULL;
    }
    selection_bounds(&start_y, &start_x, &end_y, &end_x);
    text = NULL;
    for (size_t row = start_y; row <= end_y; row++) {
        const Row *line = row_at(row);
        size_t from = 0;
        size_t to = line ? line->length : 0;
        size_t chunk;

        if (!line) {
            continue;
        }
        if (row == start_y) {
            from = start_x;
        }
        if (row == end_y) {
            to = end_x;
        }
        if (to < from) {
            continue;
        }
        if (row > start_y) {
            text = grow_selected_text(text, &capacity, length, 1);
            if (!text) {
                return NULL;
            }
            text[length++] = '\n';
        }
        chunk = to - from;
        text = grow_selected_text(text, &capacity, length, chunk);
        if (!text) {
            return NULL;
        }
        memcpy(text + length, line->data + from, chunk);
        length += chunk;
    }
    if (!text) {
        text = malloc(1);
        if (!text) {
            return NULL;
        }
    }
    text[length] = '\0';
    return text;
}

static void delete_selection(void)
{
    size_t start_y;
    size_t start_x;
    size_t end_y;
    size_t end_x;
    Row *line;

    if (!editor.selection_active) {
        return;
    }
    history_record_edit();
    selection_bounds(&start_y, &start_x, &end_y, &end_x);
    if (start_y == end_y) {
        line = row_at(start_y);
        if (!line || start_x >= end_x) {
            return;
        }
        memmove(line->data + start_x, line->data + end_x,
                line->length - end_x + 1);
        line->length -= end_x - start_x;
    } else {
        Row *start = row_at(start_y);
        Row *end = row_at(end_y);

        if (!start || !end) {
            return;
        }
        ensure_row_capacity(start, start->length + 1);
        start->data[start_x] = '\0';
        start->length = start_x;
        if (end_x < end->length) {
            ensure_row_capacity(start, start->length + (end->length - end_x) + 1);
            memcpy(start->data + start->length, end->data + end_x,
                   end->length - end_x + 1);
            start->length += end->length - end_x;
        }
        for (size_t row = end_y; row > start_y; row--) {
            delete_row(row);
        }
    }
    editor.cursor_y = start_y;
    editor.cursor_x = start_x;
    editor.dirty = true;
    editor.quit_pending = false;
    mark_semantic_dirty();
}

static bool copy_selection(void)
{
    char *text = selected_text();

    if (!text) {
        set_message("Nothing selected to copy");
        return false;
    }
    if (!platform_set_clipboard(text)) {
        free(text);
        set_message("Copy failed");
        return false;
    }
    free(text);
    set_message("Copied selection");
    return true;
}

static bool cut_selection(void)
{
    if (!editor.selection_active) {
        return false;
    }
    if (!copy_selection()) {
        return false;
    }
    delete_selection();
    clear_selection();
    return true;
}

static bool format_row_inplace(Row *row)
{
    ssize_t formatted =
        cg_format_text_line(row->data, row->length, row->capacity);

    if (formatted >= 0) {
        row->length = (size_t) formatted;
        return true;
    }
    {
        size_t capacity = row->length + 64;
        char *grown = realloc(row->data, capacity);

        if (!grown) {
            return false;
        }
        row->data = grown;
        row->capacity = capacity;
    }
    formatted = cg_format_text_line(row->data, row->length, row->capacity);
    if (formatted < 0) {
        return false;
    }
    row->length = (size_t) formatted;
    return true;
}

static size_t clip_leading_spaces(const char *text, size_t at)
{
    size_t count = 0;

    while (text[at + count] == ' ') {
        count++;
    }
    return count;
}

static void paste_clipboard_line(const char *content, size_t length,
                                 size_t indent)
{
    size_t i;

    insert_newline_plain();
    for (i = 0; i < indent; i++) {
        insert_char(' ');
    }
    for (i = 0; i < length; i++) {
        insert_char(content[i]);
    }
}

static bool paste_clipboard(bool formatted)
{
    char *text;
    size_t at = 0;
    size_t start_row;
    size_t end_row;
    size_t target_base;
    size_t clip_base = 0;
    bool first_line = true;

    if (!platform_get_clipboard(&text)) {
        set_message("Paste failed");
        return false;
    }
    history_record_edit();
    editor.history.suspended = true;
    if (editor.selection_active) {
        delete_selection();
        clear_selection();
    }
    if (editor.cursor_y == editor.row_count) {
        insert_row(editor.row_count, "", 0);
    }
    start_row = editor.cursor_y;
    target_base = leading_spaces(&editor.rows[editor.cursor_y]);
    if (formatted) {
        clip_base = clip_leading_spaces(text, 0);
    }
    while (text[at]) {
        if (text[at] == '\r') {
            at++;
            continue;
        }
        if (!formatted) {
            if (text[at] == '\n') {
                insert_newline_plain();
            } else {
                insert_char(text[at]);
            }
            at++;
            continue;
        }
        {
            size_t line_indent = clip_leading_spaces(text, at);
            size_t content_start = at + line_indent;
            size_t content_end = content_start;

            while (text[content_end] && text[content_end] != '\n' &&
                   text[content_end] != '\r') {
                content_end++;
            }
            if (first_line) {
                size_t i;

                for (i = content_start; i < content_end; i++) {
                    insert_char(text[i]);
                }
            } else {
                size_t indent = target_base;

                if (line_indent >= clip_base) {
                    indent += line_indent - clip_base;
                }
                paste_clipboard_line(text + content_start,
                                     content_end - content_start, indent);
            }
            first_line = false;
            at = content_end;
            if (text[at] == '\r') {
                at++;
            }
            if (text[at] == '\n') {
                at++;
            }
        }
    }
    end_row = editor.cursor_y;
    if (formatted) {
        for (size_t row_index = start_row; row_index <= end_row; row_index++) {
            if (!format_row_inplace(&editor.rows[row_index])) {
                editor.history.suspended = false;
                free(text);
                set_message("Paste failed");
                return false;
            }
        }
        {
            Row *row = row_at(editor.cursor_y);

            if (row && editor.cursor_x > row->length) {
                editor.cursor_x = row->length;
            }
        }
    }
    editor.history.suspended = false;
    free(text);
    set_message(formatted ? "Pasted with formatting" : "Pasted from clipboard");
    return true;
}

static bool is_standalone_as(const Row *row, size_t at)
{
    size_t start;

    if (at + 1 < row->length &&
        row->data[at] == 'a' && row->data[at + 1] == 's') {
        start = at;
    } else if (at >= 1 &&
               row->data[at - 1] == 'a' && row->data[at] == 's') {
        start = at - 1;
    } else {
        return false;
    }
    if (start > 0 &&
        cg_name_char((unsigned char) row->data[start - 1])) {
        return false;
    }
    if (start + 2 < row->length &&
        cg_name_char((unsigned char) row->data[start + 2])) {
        return false;
    }
    return true;
}

static bool span_is_dotted_reference(const Row *row, size_t at, size_t *start,
                                     size_t *end)
{
    size_t seg_start;
    size_t seg_end;

    if (!row || at >= row->length) {
        return false;
    }
    if (!cg_name_start((unsigned char) row->data[at]) &&
        !(row->data[at] == '.' && at + 1 < row->length &&
          cg_name_start((unsigned char) row->data[at + 1])) &&
        !cg_name_char((unsigned char) row->data[at]) &&
        row->data[at] != '.') {
        return false;
    }
    seg_start = at;
    while (seg_start > 0) {
        unsigned char ch = (unsigned char) row->data[seg_start - 1];

        if (cg_name_char(ch) || row->data[seg_start - 1] == '.') {
            seg_start--;
            continue;
        }
        break;
    }
    seg_end = at + 1;
    while (seg_end < row->length) {
        unsigned char ch = (unsigned char) row->data[seg_end];

        if (cg_name_char(ch) || row->data[seg_end] == '.') {
            seg_end++;
            continue;
        }
        break;
    }
    if (!cg_name_start((unsigned char) row->data[seg_start])) {
        return false;
    }
    while (seg_end > seg_start && row->data[seg_end - 1] == '.') {
        seg_end--;
    }
    if (seg_end <= seg_start) {
        return false;
    }
    *start = seg_start;
    *end = seg_end;
    return true;
}

static bool span_is_self_reference(const Row *row, size_t at, size_t *start,
                                   size_t *end)
{
    if (!span_is_dotted_reference(row, at, start, end)) {
        return false;
    }
    if (*end - *start <= 5 || memcmp(row->data + *start, "self.", 5) != 0) {
        return false;
    }
    return cg_name_start((unsigned char) row->data[*start + 5]);
}

static bool dotted_ref_highlightable(const Row *row, const CgemSemantic *semantic,
                                     size_t at, size_t *start, size_t *end)
{
    char reference[256];

    if (!span_is_dotted_reference(row, at, start, end)) {
        return false;
    }
    if (*end - *start >= sizeof(reference)) {
        return false;
    }
    memcpy(reference, row->data + *start, *end - *start);
    reference[*end - *start] = '\0';
    return cgem_semantic_reference_known(semantic, reference, *end - *start,
                                         false);
}

static bool row_word_at(const Row *row, size_t at, const char *word)
{
    size_t length = strlen(word);
    char next;

    if (row->length - at < length) {
        return false;
    }
    if (memcmp(row->data + at, word, length) != 0) {
        return false;
    }
    next = at + length < row->length ? row->data[at + length] : '\0';
    return next == '\0' || next == ' ';
}

#define BODY_HIGHLIGHT_MAX 64

typedef enum {
    BODY_HL_KEYWORD,
    BODY_HL_NAME,
    BODY_HL_TYPE,
    BODY_HL_BUILTIN,
    BODY_HL_OPERATOR
} BodyHighlightKind;

typedef struct {
    size_t start;
    size_t end;
    BodyHighlightKind kind;
} BodyHighlightSpan;

static void body_highlight_add(BodyHighlightSpan *spans, size_t *count,
                               size_t start, size_t end,
                               BodyHighlightKind kind)
{
    if (!spans || !count || end <= start || *count >= BODY_HIGHLIGHT_MAX) {
        return;
    }
    spans[*count].start = start;
    spans[*count].end = end;
    spans[*count].kind = kind;
    (*count)++;
}

static bool body_highlight_at(const BodyHighlightSpan *spans, size_t count,
                              size_t at, BodyHighlightKind *kind,
                              size_t *span_start, size_t *span_end)
{
    for (size_t i = 0; i < count; i++) {
        if (at >= spans[i].start && at < spans[i].end) {
            if (kind) {
                *kind = spans[i].kind;
            }
            if (span_start) {
                *span_start = spans[i].start;
            }
            if (span_end) {
                *span_end = spans[i].end;
            }
            return true;
        }
    }
    return false;
}

static bool type_reference_highlightable(const Row *row,
                                         const CgemSemantic *semantic,
                                         size_t file_row, size_t start,
                                         size_t end)
{
    char reference[256];
    char qualified[256];
    char scope[256];
    IdeIndexRow rows[512];
    size_t row_count = editor.row_count;

    if (!row || !semantic || end <= start || end - start >= sizeof(reference)) {
        return false;
    }
    memcpy(reference, row->data + start, end - start);
    reference[end - start] = '\0';
    if (cgem_semantic_reference_known(semantic, reference, end - start, false)) {
        return true;
    }
    {
        const CgemSemanticSymbol *symbol = cgem_semantic_find(semantic, reference);

        if (symbol && cgem_semantic_symbol_is_type(symbol)) {
            return true;
        }
    }
    if (row_count == 0 || row_count > sizeof(rows) / sizeof(rows[0]) ||
        file_row >= row_count) {
        return false;
    }
    for (size_t i = 0; i < row_count; i++) {
        rows[i].data = editor.rows[i].data;
        rows[i].length = editor.rows[i].length;
    }
    if (!cgem_semantic_scope_path(rows, row_count, file_row, scope,
                                   sizeof(scope))) {
        return false;
    }
    if (!cgem_semantic_qualify_reference(reference, scope, semantic, qualified,
                                         sizeof(qualified))) {
        return false;
    }
    if (cgem_semantic_reference_known(semantic, qualified, strlen(qualified),
                                      false)) {
        return true;
    }
    {
        const CgemSemanticSymbol *symbol = cgem_semantic_find(semantic, qualified);

        if (symbol && cgem_semantic_symbol_is_type(symbol)) {
            return true;
        }
    }
    return false;
}

static bool field_as_param_context(const Row *row, size_t param_at)
{
    return param_at >= 3 && row->data[param_at - 3] == 'a' &&
           row->data[param_at - 2] == 's' && row->data[param_at - 1] == ' ';
}

static size_t scan_attrs_before(const Row *row, size_t before_at,
                                size_t line_first, BodyHighlightSpan *spans,
                                size_t *count)
{
    size_t at = before_at;

    for (;;) {
        while (at > line_first && row->data[at - 1] == ' ') {
            at--;
        }
        if (at < line_first + 1 || row->data[at - 1] != '@') {
            break;
        }
        {
            size_t attr_at = at - 1;
            size_t name_start = attr_at + 1;
            size_t name_end;

            if (name_start >= row->length ||
                !cg_name_start((unsigned char) row->data[name_start])) {
                break;
            }
            name_end = skip_identifier(row, name_start);
            body_highlight_add(spans, count, attr_at, name_end, BODY_HL_BUILTIN);
            at = attr_at;
        }
    }
    return at;
}

static void scan_line_attributes(const Row *row, size_t line_first,
                                 BodyHighlightSpan *spans, size_t *count)
{
    for (size_t at = line_first; at < row->length; at++) {
        size_t name_start;
        size_t name_end;

        if (row->data[at] != '@') {
            continue;
        }
        if (at > line_first &&
            cg_name_char((unsigned char) row->data[at - 1])) {
            continue;
        }
        name_start = at + 1;
        if (name_start >= row->length ||
            !cg_name_start((unsigned char) row->data[name_start])) {
            continue;
        }
        name_end = skip_identifier(row, name_start);
        body_highlight_add(spans, count, at, name_end, BODY_HL_BUILTIN);
    }
}

static void scan_param_as_clause(const Row *row, size_t line_first,
                                 BodyHighlightSpan *spans, size_t *count)
{
    size_t at = line_first;

    if (!row_word_at(row, at, "param")) {
        return;
    }
    body_highlight_add(spans, count, at, at + 5, BODY_HL_KEYWORD);
    at += 5;
    while (at < row->length && row->data[at] == ' ') {
        at++;
    }
    if (at < row->length &&
        cg_name_start((unsigned char) row->data[at])) {
        size_t name_start = at;

        at = skip_identifier(row, at);
        body_highlight_add(spans, count, name_start, at, BODY_HL_NAME);
    }
    while (at < row->length && row->data[at] == ' ') {
        at++;
    }
    if (at + 2 <= row->length && memcmp(row->data + at, "as", 2) == 0 &&
        (at + 2 == row->length || row->data[at + 2] == ' ')) {
        body_highlight_add(spans, count, at, at + 2, BODY_HL_KEYWORD);
        at += 2;
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
        if (at < row->length &&
            (cg_name_start((unsigned char) row->data[at]) ||
             row->data[at] == '.')) {
            size_t type_start = at;

            while (at < row->length &&
                   (cg_name_char((unsigned char) row->data[at]) ||
                    row->data[at] == '.')) {
                at++;
            }
            body_highlight_add(spans, count, type_start, at, BODY_HL_TYPE);
        }
    }
}

static void scan_inline_param_clauses(const Row *row, size_t line_first,
                                      BodyHighlightSpan *spans, size_t *count)
{
    for (size_t at = line_first; at + 5 <= row->length; at++) {
        if (!row_word_at(row, at, "param")) {
            continue;
        }
        if (field_as_param_context(row, at)) {
            continue;
        }
        if (at == line_first) {
            continue;
        }
        if (at > line_first &&
            cg_name_char((unsigned char) row->data[at - 1])) {
            continue;
        }
        scan_attrs_before(row, at, line_first, spans, count);
        body_highlight_add(spans, count, at, at + 5, BODY_HL_KEYWORD);
        at += 5;
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
        if (at < row->length &&
            cg_name_start((unsigned char) row->data[at])) {
            size_t name_start = at;

            at = skip_identifier(row, at);
            body_highlight_add(spans, count, name_start, at, BODY_HL_NAME);
        }
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
        if (at + 2 <= row->length && memcmp(row->data + at, "as", 2) == 0 &&
            (at + 2 == row->length || row->data[at + 2] == ' ')) {
            body_highlight_add(spans, count, at, at + 2, BODY_HL_KEYWORD);
            at += 2;
            while (at < row->length && row->data[at] == ' ') {
                at++;
            }
            if (at < row->length &&
                (cg_name_start((unsigned char) row->data[at]) ||
                 row->data[at] == '.')) {
                size_t type_start = at;

                while (at < row->length &&
                       (cg_name_char((unsigned char) row->data[at]) ||
                        row->data[at] == '.')) {
                    at++;
                }
                body_highlight_add(spans, count, type_start, at, BODY_HL_TYPE);
            }
        }
    }
}

static bool row_matches_op2(const Row *row, size_t at, char a, char b)
{
    return at + 2 <= row->length && row->data[at] == a && row->data[at + 1] == b;
}

static bool row_matches_op3(const Row *row, size_t at, char a, char b, char c)
{
    return at + 3 <= row->length && row->data[at] == a && row->data[at + 1] == b &&
           row->data[at + 2] == c;
}

static size_t row_elvis_operator_length(const Row *row, size_t at)
{
    size_t probe;

    if (!row || at >= row->length || row->data[at] != '?' ||
        (at + 1 < row->length && row->data[at + 1] == '?')) {
        return 0;
    }
    probe = at + 1;
    while (probe < row->length && row->data[probe] == ' ') {
        probe++;
    }
    if (probe < row->length && row->data[probe] == ':') {
        return probe - at + 1;
    }
    return 0;
}

static size_t row_ternary_colon_at(const Row *row, size_t question_at)
{
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    size_t colon = (size_t) -1;

    if (!row || question_at >= row->length) {
        return (size_t) -1;
    }
    for (size_t i = question_at + 1; i < row->length; i++) {
        char ch = row->data[i];

        if (in_string) {
            if (ch == '"' && !escaped) {
                in_string = false;
            }
            escaped = ch == '\\' && !escaped;
            if (ch != '\\') {
                escaped = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
            escaped = false;
            continue;
        }
        if (ch == '(') {
            depth++;
            continue;
        }
        if (ch == ')' && depth > 0) {
            depth--;
            continue;
        }
        if (depth == 0 && ch == ':') {
            colon = i;
        }
    }
    return colon;
}

static void scan_conditional_operators(const Row *row, size_t line_first,
                                       BodyHighlightSpan *spans, size_t *count)
{
    int depth = 0;
    bool in_string = false;
    bool escaped = false;

    if (!row) {
        return;
    }
    for (size_t i = line_first; i < row->length; i++) {
        char ch = row->data[i];

        if (in_string) {
            if (ch == '"' && !escaped) {
                in_string = false;
            }
            escaped = ch == '\\' && !escaped;
            if (ch != '\\') {
                escaped = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
            escaped = false;
            continue;
        }
        if (ch == '@') {
            while (i < row->length && row->data[i] != ' ' && row->data[i] != '(') {
                i++;
            }
            continue;
        }
        if (ch == '(') {
            depth++;
            continue;
        }
        if (ch == ')' && depth > 0) {
            depth--;
            continue;
        }
        if (depth != 0 || ch != '?' || (i + 1 < row->length && row->data[i + 1] == '?')) {
            continue;
        }
        {
            size_t elvis = row_elvis_operator_length(row, i);

            if (elvis > 0) {
                body_highlight_add(spans, count, i, i + elvis, BODY_HL_OPERATOR);
                i += elvis - 1;
                continue;
            }
        }
        {
            size_t probe = i + 1;
            size_t colon;

            while (probe < row->length && row->data[probe] == ' ') {
                probe++;
            }
            if (probe < row->length && row->data[probe] == ':') {
                continue;
            }
            colon = row_ternary_colon_at(row, i);
            if (colon != (size_t) -1) {
                body_highlight_add(spans, count, i, i + 1, BODY_HL_OPERATOR);
                body_highlight_add(spans, count, colon, colon + 1,
                                   BODY_HL_OPERATOR);
            }
        }
    }
}

static size_t operator_token_length(const Row *row, size_t at)
{
    size_t elvis;

    if (!row || at >= row->length) {
        return 0;
    }
    elvis = row_elvis_operator_length(row, at);
    if (elvis > 0) {
        return elvis;
    }
    if (row_matches_op3(row, at, '?', '?', '=')) {
        return 3;
    }
    if (row_matches_op3(row, at, '<', '<', '=')) {
        return 3;
    }
    if (row_matches_op3(row, at, '>', '>', '=')) {
        return 3;
    }
    if (row_matches_op2(row, at, '?', '?')) {
        return 2;
    }
    if (row_matches_op2(row, at, '?', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '=', '?')) {
        return 2;
    }
    if (row_matches_op2(row, at, '=', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '!', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '<', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '>', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '<', '<')) {
        return 2;
    }
    if (row_matches_op2(row, at, '>', '>')) {
        return 2;
    }
    if (row_matches_op2(row, at, '&', '&')) {
        return 2;
    }
    if (row_matches_op2(row, at, '|', '|')) {
        return 2;
    }
    if (row_matches_op2(row, at, '+', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '-', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '*', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '/', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '%', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '&', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '|', '=')) {
        return 2;
    }
    if (row_matches_op2(row, at, '^', '=')) {
        return 2;
    }
    switch (row->data[at]) {
    case '=':
    case '?':
    case ':':
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '&':
    case '|':
    case '^':
    case '~':
    case '!':
    case '<':
    case '>':
        return 1;
    default:
        return 0;
    }
}

static bool row_position_in_string(const Row *row, size_t at)
{
    bool in_string = false;
    bool escaped = false;

    if (!row) {
        return false;
    }
    for (size_t i = 0; i < at && i < row->length; i++) {
        char ch = row->data[i];

        if (in_string) {
            if (ch == '"' && !escaped) {
                in_string = false;
            }
            escaped = ch == '\\' && !escaped;
            if (ch != '\\') {
                escaped = false;
            }
        } else if (ch == '"') {
            in_string = true;
            escaped = false;
        }
    }
    return in_string;
}

static bool operator_highlight_at(const Row *row, size_t at)
{
    if (!row || at >= row->length || row_position_in_string(row, at)) {
        return false;
    }
    for (size_t back = 0; back < 3 && at >= back; back++) {
        size_t pos = at - back;
        size_t len = operator_token_length(row, pos);

        if (len > 0 && pos + len > at) {
            return true;
        }
    }
    return false;
}

static void scan_line_operators(const Row *row, size_t line_first,
                                BodyHighlightSpan *spans, size_t *count)
{
    size_t at = line_first;

    if (!row) {
        return;
    }
    while (at < row->length) {
        if (row->data[at] == ' ') {
            at++;
            continue;
        }
        if (row->data[at] == '"') {
            size_t string_start = 0;
            size_t string_end = 0;

            at = scan_highlight_string(row, at, &string_start, &string_end);
            continue;
        }
        if (row->data[at] == '@') {
            at++;
            at = skip_identifier(row, at);
            if (at < row->length && row->data[at] == '(') {
                at++;
                while (at < row->length && row->data[at] != ')') {
                    if (row->data[at] == '"') {
                        size_t string_start = 0;
                        size_t string_end = 0;

                        at = scan_highlight_string(row, at, &string_start,
                                                   &string_end);
                    } else {
                        at++;
                    }
                }
                if (at < row->length && row->data[at] == ')') {
                    at++;
                }
            }
            continue;
        }
        if (cg_name_start((unsigned char) row->data[at])) {
            at = skip_identifier(row, at);
            while (at + 1 < row->length && row->data[at] == '.' &&
                   cg_name_start((unsigned char) row->data[at + 1])) {
                at++;
                at = skip_identifier(row, at);
            }
            continue;
        }
        if (isdigit((unsigned char) row->data[at])) {
            while (at < row->length &&
                   isdigit((unsigned char) row->data[at])) {
                at++;
            }
            continue;
        }
        {
            size_t op_len = operator_token_length(row, at);

            if (op_len > 0) {
                body_highlight_add(spans, count, at, at + op_len,
                                   BODY_HL_OPERATOR);
                at += op_len;
                continue;
            }
        }
        if (row->data[at] == '(' || row->data[at] == ')' ||
            row->data[at] == ',' || row->data[at] == '[' ||
            row->data[at] == ']' || row->data[at] == '{' ||
            row->data[at] == '}') {
            body_highlight_add(spans, count, at, at + 1, BODY_HL_OPERATOR);
            at++;
            continue;
        }
        at++;
    }
}

static void scan_body_line_highlights(const Row *row, size_t line_first,
                                      BodyHighlightSpan *spans, size_t *count,
                                      size_t *function_start,
                                      size_t *function_end)
{
    *count = 0;
    *function_start = 0;
    *function_end = 0;

    if (row->length - line_first > 5 &&
        memcmp(row->data + line_first, "self.", 5) == 0) {
        size_t at = line_first + 5;

        if (at < row->length &&
            cg_name_start((unsigned char) row->data[at])) {
            *function_start = at;
            at = skip_identifier(row, at);
            *function_end = at;
            body_highlight_add(spans, count, line_first, at, BODY_HL_NAME);
            while (at < row->length && row->data[at] == ' ') {
                at++;
            }
            if (at < row->length && row->data[at] == ':') {
                body_highlight_add(spans, count, at, at + 1, BODY_HL_OPERATOR);
            }
        }
    }

    scan_line_attributes(row, line_first, spans, count);
    scan_param_as_clause(row, line_first, spans, count);
    scan_inline_param_clauses(row, line_first, spans, count);
    scan_line_operators(row, line_first, spans, count);
    scan_conditional_operators(row, line_first, spans, count);
}

static size_t highlight_declaration_as_clause(
    const Row *row, size_t at, size_t *second_keyword_start,
    size_t *second_keyword_end, size_t *third_keyword_start,
    size_t *third_keyword_end, size_t *type_start, size_t *type_end,
    size_t *colon);

static size_t highlight_require_spec_clause(
    const Row *row, size_t at, size_t *keyword_start, size_t *keyword_end,
    size_t *second_keyword_start, size_t *second_keyword_end,
    size_t *third_keyword_start, size_t *third_keyword_end,
    size_t *type_start, size_t *type_end)
{
    if (row_word_at(row, at, "value")) {
        *keyword_start = at;
        *keyword_end = at + 5;
        return at + 5;
    }
    if (!row_word_at(row, at, "type")) {
        return at;
    }
    *keyword_start = at;
    *keyword_end = at + 4;
    at += 4;
    while (at < row->length && row->data[at] == ' ') {
        at++;
    }
    if (at < row->length && row_word_at(row, at, "as")) {
        return highlight_declaration_as_clause(
            row, at, second_keyword_start, second_keyword_end,
            third_keyword_start, third_keyword_end, type_start, type_end, NULL);
    }
    return at;
}

static size_t highlight_declaration_as_clause(
    const Row *row, size_t at, size_t *second_keyword_start,
    size_t *second_keyword_end, size_t *third_keyword_start,
    size_t *third_keyword_end, size_t *type_start, size_t *type_end,
    size_t *colon)
{
    if (row->length - at < 2 || memcmp(row->data + at, "as", 2) != 0 ||
        (at + 2 < row->length && row->data[at + 2] != ' ' &&
         at + 2 != row->length)) {
        return at;
    }
    *second_keyword_start = at;
    *second_keyword_end = at + 2;
    at += 2;
    while (at < row->length && row->data[at] == ' ') {
        at++;
    }
    if (row_word_at(row, at, "param")) {
        *third_keyword_start = at;
        *third_keyword_end = at + 5;
        at += 5;
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
    }
    if (at < row->length &&
        cg_name_start((unsigned char) row->data[at])) {
        *type_start = at;
        at = skip_identifier(row, at);
        while (at < row->length &&
               (cg_name_char((unsigned char) row->data[at]) ||
                row->data[at] == '.')) {
            at++;
        }
        *type_end = at;
        if (at < row->length && row->data[at] == ':') {
            *colon = at;
        }
    }
    return at;
}

static void draw_editor_row(Buffer *buffer, const Row *row, int content_cols,
                            size_t file_row, bool active, bool mark_diagnostic,
                            DiagnosticSeverity line_diag)
{
    size_t first = 0;
    size_t keyword_start;
    size_t keyword_end;
    size_t second_keyword_start = 0;
    size_t second_keyword_end = 0;
    size_t third_keyword_start = 0;
    size_t third_keyword_end = 0;
    size_t attribute_start = 0;
    size_t attribute_end = 0;
    size_t name_start = 0;
    size_t name_end = 0;
    size_t type_start = 0;
    size_t type_end = 0;
    size_t colon = (size_t) -1;
    size_t builtin_start = 0;
    size_t builtin_end = 0;
    size_t function_start = 0;
    size_t function_end = 0;
    size_t string_start = 0;
    size_t string_end = 0;
    size_t call_punctuation[CALL_PUNCTUATION_MAX];
    size_t call_punctuation_count = 0;
    BodyHighlightSpan body_spans[BODY_HIGHLIGHT_MAX];
    size_t body_span_count = 0;
    size_t drawn;
    size_t row_index = row >= editor.rows &&
                               row < editor.rows + editor.row_count
                           ? (size_t) (row - editor.rows)
                           : SIZE_MAX;
    const char *ghost = keyword_ghost(row, row_index, NULL);
    bool ghost_inline = ghost && active && row_index == editor.cursor_y &&
                          editor.cursor_x < row->length &&
                          completion_suffix_is_punctuation(row, editor.cursor_x);
    size_t ghost_at = ghost_inline ? editor.cursor_x : (size_t) -1;
    bool dim_doc = row_index != SIZE_MAX &&
                   row_is_muted_attribute_line(row_index) && !active;
    const EditorTheme *palette = current_theme();
    const char *diagnostic_theme = NULL;
    bool stripe_row = !active && (file_row % 2 == 1);
    const char *editor_theme =
        stripe_row ? palette->stripe.editor
                   : (active ? palette->editor_active : palette->editor);
    const char *keyword_theme =
        stripe_row ? palette->stripe.keyword
                   : (active ? palette->keyword_active : palette->keyword);
    const char *name_theme =
        stripe_row ? palette->stripe.name_color
                   : (active ? palette->name_active : palette->name_color);
    const char *punctuation_theme =
        dim_doc ? (stripe_row ? palette->stripe.punct_muted
                              : palette->punct_muted)
                : (stripe_row ? palette->stripe.punctuation
                              : (active ? palette->punctuation_active
                                        : palette->punctuation));
    const char *builtin_theme =
        dim_doc ? (stripe_row ? palette->stripe.builtin_muted
                              : palette->builtin_muted)
                : (stripe_row ? palette->stripe.builtin
                              : (active ? palette->builtin_active
                                        : palette->builtin));
    const char *string_theme =
        dim_doc ? (stripe_row ? palette->stripe.string_muted
                              : palette->string_muted)
                : (stripe_row ? palette->stripe.string_color
                              : (active ? palette->string_active
                                        : palette->string_color));
    const char *ghost_theme =
        stripe_row ? palette->stripe.ghost
                   : (active ? palette->ghost_active : palette->ghost);
    const char *doc_muted_theme =
        dim_doc ? (stripe_row ? palette->stripe.doc_muted : palette->doc_muted)
                : editor_theme;
    const char *selection_theme = "\x1b[38;2;255;255;255;48;2;65;105;170m";
    const char *active_color = editor_theme;

    if (mark_diagnostic && line_diag == DIAG_ERROR) {
        diagnostic_theme = palette->error_line;
    } else if (mark_diagnostic &&
               (line_diag == DIAG_WARNING || line_diag == DIAG_NOTE)) {
        diagnostic_theme = palette->warning_line;
    }
    if (diagnostic_theme) {
        editor_theme = diagnostic_theme;
        active_color = diagnostic_theme;
        stripe_row = false;
    }

    while (first < row->length && row->data[first] == ' ') {
        first++;
    }
    keyword_start = first;
    keyword_end = first;

    size_t keyword_length = declaration_keyword_length(row, first);

    if (first < row->length && row->data[first] == '@') {
        colon = first;
        size_t at = first + 1;

        if (at < row->length &&
            cg_name_start((unsigned char) row->data[at])) {
            attribute_start = at++;
            while (at < row->length &&
                   cg_name_char((unsigned char) row->data[at])) {
                at++;
            }
            attribute_end = at;
            while (at < row->length && row->data[at] == ' ') at++;
            if (at < row->length && row->data[at] == ':') {
                if (call_punctuation_count < CALL_PUNCTUATION_MAX) {
                    call_punctuation[call_punctuation_count++] = at;
                }
            } else if (at < row->length && row->data[at] == '(') {
                if (call_punctuation_count < CALL_PUNCTUATION_MAX) {
                    call_punctuation[call_punctuation_count++] = at;
                }
                at++;
                while (at < row->length && row->data[at] == ' ') {
                    at++;
                }
                if (attribute_end - attribute_start == 7 &&
                    memcmp(row->data + attribute_start, "require", 7) == 0) {
                    highlight_require_spec_clause(
                        row, at, &keyword_start, &keyword_end,
                        &second_keyword_start, &second_keyword_end,
                        &third_keyword_start, &third_keyword_end, &type_start,
                        &type_end);
                    while (at < row->length && row->data[at] != ')') {
                        at++;
                    }
                } else if (at < row->length && row->data[at] == '"') {
                    scan_highlight_string(row, at, &string_start, &string_end);
                    while (at < row->length && row->data[at] != ')') {
                        at++;
                    }
                } else {
                    while (at < row->length && row->data[at] != ')') {
                        at++;
                    }
                }
                if (at < row->length && row->data[at] == ')' &&
                    call_punctuation_count < CALL_PUNCTUATION_MAX) {
                    call_punctuation[call_punctuation_count++] = at;
                }
            }
        }
    } else if (row_is_block_require_spec_line(row)) {
        highlight_require_spec_clause(row, first, &keyword_start, &keyword_end,
                                    &second_keyword_start, &second_keyword_end,
                                    &third_keyword_start, &third_keyword_end,
                                    &type_start, &type_end);
    } else if (row_is_block_attribute_string_line(row)) {
        scan_highlight_string(row, first, &string_start, &string_end);
    } else if (keyword_length) {
        size_t at = first + keyword_length;

        keyword_end = at;
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
        if (row_starts_with(row, first, "param")) {
            if (at < row->length &&
                cg_name_start((unsigned char) row->data[at])) {
                name_start = at++;
                while (at < row->length &&
                       cg_name_char((unsigned char) row->data[at])) {
                    at++;
                }
                name_end = at;
            }
            while (at < row->length && row->data[at] == ' ') {
                at++;
            }
            if (row->length - at >= 2 &&
                memcmp(row->data + at, "as", 2) == 0 &&
                (at + 2 == row->length || row->data[at + 2] == ' ')) {
                at = highlight_declaration_as_clause(
                    row, at, &second_keyword_start, &second_keyword_end,
                    &third_keyword_start, &third_keyword_end, &type_start,
                    &type_end, &colon);
            }
        } else {
            if (at < row->length &&
                cg_name_start((unsigned char) row->data[at])) {
                size_t ident_start = at;

                at = skip_identifier(row, at);
                while (at + 1 < row->length && row->data[at] == '.' &&
                       cg_name_start((unsigned char) row->data[at + 1])) {
                    at++;
                    at = skip_identifier(row, at);
                }
                if (span_is_module_export(row, ident_start, at)) {
                    name_start = ident_start;
                    name_end = at;
                }
            }
            while (at < row->length && row->data[at] == ' ') {
                at++;
            }
            if (row->length - at >= 2 &&
                memcmp(row->data + at, "as", 2) == 0 &&
                (at + 2 == row->length || row->data[at + 2] == ' ')) {
                at = highlight_declaration_as_clause(
                    row, at, &second_keyword_start, &second_keyword_end,
                    &third_keyword_start, &third_keyword_end, &type_start,
                    &type_end, &colon);
            } else if (at < row->length && row->data[at] == ':') {
                colon = at;
            }
        }
    } else if ((keyword_length = decl_as_keyword_length(row, first)) != 0) {
        size_t at = first + keyword_length;

        keyword_end = at;
        while (at < row->length && row->data[at] == ' ') {
            at++;
        }
        if (at < row->length &&
            cg_name_start((unsigned char) row->data[at]) &&
            !(row->length - at >= 2 &&
              memcmp(row->data + at, "as", 2) == 0 &&
              (at + 2 == row->length || row->data[at + 2] == ' '))) {
            size_t ident_start = at;

            at = skip_identifier(row, at);
            while (at + 1 < row->length && row->data[at] == '.' &&
                   cg_name_start((unsigned char) row->data[at + 1])) {
                at++;
                at = skip_identifier(row, at);
            }
            if (span_is_module_export(row, ident_start, at)) {
                name_start = ident_start;
                name_end = at;
            }
            while (at < row->length && row->data[at] == ' ') {
                at++;
            }
        }
        if (row->length - at >= 2 &&
            memcmp(row->data + at, "as", 2) == 0 &&
            (at + 2 == row->length || row->data[at + 2] == ' ')) {
            at = highlight_declaration_as_clause(
                row, at, &second_keyword_start, &second_keyword_end,
                &third_keyword_start, &third_keyword_end, &type_start,
                &type_end, &colon);
        }
    }

    scan_body_line_highlights(row, first, body_spans, &body_span_count,
                              &function_start, &function_end);

    buffer_append(buffer, editor_theme, strlen(editor_theme));
    {
        size_t at = editor.col_offset;

        if (at >= row->length && row->length > 0) {
            at = 0;
        }
        size_t screen_col = 0;

        while (at < row->length && screen_col < (size_t) content_cols) {
            const char *color = dim_doc ? doc_muted_theme : editor_theme;

            if (ghost_at != (size_t) -1 && at == ghost_at) {
                size_t ghost_length = strlen(ghost);
                size_t available = (size_t) content_cols - screen_col;

                if (ghost_length > available) {
                    ghost_length = available;
                }
                if (ghost_length > 0) {
                    buffer_append(buffer, ghost_theme, strlen(ghost_theme));
                    buffer_append(buffer, ghost, ghost_length);
                    screen_col += ghost_length;
                }
            }
            if (screen_col >= (size_t) content_cols) {
                break;
            }
            if (!diagnostic_theme) {
            if (at >= keyword_start && at < keyword_end) {
                color = keyword_theme;
            } else if (at >= second_keyword_start &&
                       at < second_keyword_end) {
                color = keyword_theme;
            } else if (third_keyword_end > third_keyword_start &&
                       at >= third_keyword_start &&
                       at < third_keyword_end) {
                color = keyword_theme;
            } else {
                BodyHighlightKind body_kind;
                size_t body_span_start = 0;
                size_t body_span_end = 0;

                if (body_highlight_at(body_spans, body_span_count, at, &body_kind,
                                      &body_span_start, &body_span_end)) {
                    switch (body_kind) {
                    case BODY_HL_KEYWORD:
                        color = keyword_theme;
                        break;
                    case BODY_HL_NAME:
                        color = name_theme;
                        break;
                    case BODY_HL_TYPE:
                        if (type_reference_highlightable(row, &editor.semantic,
                                                         file_row,
                                                         body_span_start,
                                                         body_span_end)) {
                            color = name_theme;
                        }
                        break;
                    case BODY_HL_BUILTIN:
                        color = builtin_theme;
                        break;
                    case BODY_HL_OPERATOR:
                        color = punctuation_theme;
                        break;
                    }
                } else if (attribute_end > attribute_start &&
                           at >= attribute_start && at < attribute_end) {
                    color = builtin_theme;
                } else if (name_end > name_start &&
                           at >= name_start && at < name_end) {
                    color = span_is_module_export(row, name_start, name_end)
                        ? keyword_theme : name_theme;
                } else if (type_end > type_start &&
                           at >= type_start && at < type_end) {
                    if (type_reference_highlightable(row, &editor.semantic,
                                                     file_row, type_start,
                                                     type_end)) {
                        color = name_theme;
                    }
                } else if (at >= builtin_start && at < builtin_end) {
                    color = builtin_theme;
                } else if (at >= function_start && at < function_end) {
                    color = name_theme;
                } else if (string_end > string_start &&
                           at >= string_start && at < string_end) {
                    color = string_theme;
                } else if (at == colon) {
                    color = punctuation_theme;
                } else if (dim_doc && at == first) {
                    color = doc_muted_theme;
                } else {
                    size_t ref_start = 0;
                    size_t ref_end = 0;

                    if (dotted_ref_highlightable(row, &editor.semantic, at,
                                                 &ref_start, &ref_end)) {
                        color = name_theme;
                    } else if (span_is_self_reference(row, at, &ref_start,
                                                      &ref_end)) {
                        color = name_theme;
                    } else if (is_standalone_as(row, at)) {
                        color = keyword_theme;
                    } else if (operator_highlight_at(row, at)) {
                        color = punctuation_theme;
                    } else if (row->data[at] == '(' || row->data[at] == ')' ||
                               row->data[at] == ',') {
                        color = punctuation_theme;
                    } else {
                        for (size_t i = 0; i < call_punctuation_count; i++) {
                            if (at == call_punctuation[i]) {
                                color = punctuation_theme;
                                break;
                            }
                        }
                    }
                }
            }
            }
            if (position_selected((size_t) (row - editor.rows), at)) {
                color = selection_theme;
            }
            if (color != active_color) {
                buffer_append(buffer, color, strlen(color));
                active_color = color;
            }
            buffer_append(buffer, row->data + at, 1);
            at++;
            screen_col++;
        }
        drawn = screen_col;
        if (ghost && !ghost_inline && screen_col < (size_t) content_cols) {
            size_t length = strlen(ghost);
            size_t available = (size_t) content_cols - screen_col;

            if (length > available) {
                length = available;
            }
            buffer_append(buffer, ghost_theme, strlen(ghost_theme));
            buffer_append(buffer, ghost, length);
            drawn += length;
        }
    }
    buffer_append(buffer, editor_theme, strlen(editor_theme));
    while (drawn < (size_t) content_cols) {
        buffer_append(buffer, " ", 1);
        drawn++;
    }
}

static void refresh_screen(void)
{
    Buffer output = {0};
    int content_rows;
    int scroll_rows;
    int content_cols;
    int gutter;
    StickyScroll sticky;
    size_t sticky_rows;
    char position[64];
    char status[512];
    const EditorTheme *palette = current_theme();

    update_window_size();
    content_rows = editor_content_rows();
    if (content_rows < 1) {
        content_rows = 1;
    }
    gutter = gutter_width();
    content_cols = editor.screen_cols - gutter;
    if (content_cols < 1) {
        content_cols = 1;
    }
    scroll_to_cursor();
    ensure_semantic_fresh(false);
    update_context_hint();
    ensure_diff_fresh();
    sticky_rows = prepare_sticky_scroll(editor.row_offset, content_rows, &sticky);
    scroll_rows = content_rows - (int) sticky_rows;
    if (scroll_rows < 1) {
        scroll_rows = 1;
    }

    BUFFER_LITERAL(&output, "\x1b[?2026h\x1b[?25l");
    screen_begin_row(&output, 1, palette->header);
    draw_centered_bar(&output, "CGEM", palette->header);
    screen_begin_row(&output, 2, palette->header);
    ide_menu_draw_bar((IdeMenuBuffer *) &output, palette, editor.screen_cols,
                      editor.filename, editor.dirty, &editor.menu);

    {
        int screen_row = 3;

        for (size_t s = 0; s < sticky.count; s++) {
            size_t file_row = sticky.rows[s];
            Row *row = &editor.rows[file_row];

            screen_begin_row(&output, screen_row++, palette->editor);
            draw_sticky_row(&output, row, file_row, content_cols);
            screen_end_row(&output, palette->editor);
        }

        for (int y = 0; y < scroll_rows; y++) {
            size_t file_row = editor.row_offset + (size_t) y;
            DiagnosticSeverity line_diag = DIAG_NOTE;
            bool has_line_diag = row_diagnostic(file_row, &line_diag);
            const char *row_bg =
                (file_row % 2 == 1) ? palette->stripe.editor : palette->editor;

            screen_begin_row(&output, screen_row++, row_bg);
            draw_gutter(&output, file_row, file_row < editor.row_count);
            if (file_row < editor.row_count) {
                Row *row = &editor.rows[file_row];

                draw_editor_row(&output, row, content_cols, file_row,
                                file_row == editor.cursor_y,
                                has_line_diag, line_diag);
            } else {
                const char *empty_theme = row_bg;

                BUFFER_LITERAL(&output, palette->muted);
                buffer_append(&output, "~", 1);
                buffer_append(&output, empty_theme, strlen(empty_theme));
                for (int i = 1; i < content_cols; i++) {
                    buffer_append(&output, " ", 1);
                }
            }
            screen_end_row(&output, row_bg);
        }

        {
            int last_content_row = screen_row - 1;
            int status_row = editor.screen_rows;

            for (int row = last_content_row + 1; row < status_row; row++) {
                screen_begin_row(&output, row, palette->editor);
                buffer_append(&output, palette->editor, strlen(palette->editor));
                for (int col = 0; col < editor.screen_cols; col++) {
                    buffer_append(&output, " ", 1);
                }
                screen_end_row(&output, palette->editor);
            }
        }
    }

    screen_begin_row(&output, editor.screen_rows, palette->header);
    if (editor.prompt == IDE_PROMPT_OPEN) {
        snprintf(position, sizeof(position), "Enter: open  Esc: cancel ");
        snprintf(status, sizeof(status), " Open: %s", editor.prompt_text);
        draw_bar(&output, status, position, palette->header);
    } else if (editor.prompt == IDE_PROMPT_SAVE_AS) {
        snprintf(position, sizeof(position), "Enter: save  Esc: cancel ");
        snprintf(status, sizeof(status), " Save As: %s", editor.prompt_text);
        draw_bar(&output, status, position, palette->header);
    } else if (editor.prompt == IDE_PROMPT_FIND) {
        snprintf(position, sizeof(position), "Enter: find  Esc: cancel ");
        snprintf(status, sizeof(status), " Find: %s", editor.prompt_text);
        draw_bar(&output, status, position, palette->header);
    } else if (editor.prompt == IDE_PROMPT_GOTO_LINE) {
        snprintf(position, sizeof(position), "Enter: go  Esc: cancel ");
        snprintf(status, sizeof(status), " Go to line: %s", editor.prompt_text);
        draw_bar(&output, status, position, palette->header);
    } else if (editor.prompt == IDE_PROMPT_RENAME) {
        const int rename_field_max =
            ((int) sizeof(status) - ((int) sizeof(" Rename ") - 1) -
             ((int) sizeof(" -> ") - 1)) /
            2;

        snprintf(position, sizeof(position), "Enter: rename  Esc: cancel ");
        snprintf(status, sizeof(status), " Rename %.*s -> %.*s",
                 rename_field_max, editor.rename_qualified, rename_field_max,
                 editor.prompt_text);
        draw_bar(&output, status, position, palette->header);
    } else if (editor.prompt == IDE_PROMPT_THEME) {
        const EditorTheme *live = current_theme();

        format_theme_display_name(live->name, editor.prompt_text,
                                  sizeof(editor.prompt_text));
        editor.prompt_length = strlen(editor.prompt_text);
        snprintf(position, sizeof(position),
                 "Enter: select  Esc: cancel  <- -> ");
        snprintf(status, sizeof(status), " Theme: %s", editor.prompt_text);
        draw_bar(&output, status, position, live->header);
    } else {
        const char *bar_text = editor.context_hint[0] ? editor.context_hint
                                : (editor.message[0] ? editor.message
                                                     : IDE_HELP_HINT);

        snprintf(position, sizeof(position), "Ln %zu  Col %zu ",
                 editor.cursor_y + 1, editor.cursor_x + 1);
        snprintf(status, sizeof(status), " %s", bar_text);
        draw_bar(&output, status, position, palette->header);
    }

    if (ide_menu_is_open(&editor.menu)) {
        ide_menu_draw_popup((IdeMenuBuffer *) &output, palette,
                            editor.screen_cols, editor.screen_rows,
                            &editor.menu);
    }

    {
        size_t cursor_screen_row = (size_t) editor_screen_top();
        bool show_cursor = !ide_menu_is_open(&editor.menu) &&
                           editor.prompt == IDE_PROMPT_NONE;

        if (editor.prompt != IDE_PROMPT_NONE) {
            size_t prefix;

            if (editor.prompt == IDE_PROMPT_OPEN) {
                prefix = strlen(" Open: ");
            } else if (editor.prompt == IDE_PROMPT_SAVE_AS) {
                prefix = strlen(" Save As: ");
            } else if (editor.prompt == IDE_PROMPT_FIND) {
                prefix = strlen(" Find: ");
            } else if (editor.prompt == IDE_PROMPT_GOTO_LINE) {
                prefix = strlen(" Go to line: ");
            } else if (editor.prompt == IDE_PROMPT_RENAME) {
                size_t label =
                    strlen(" Rename ") + strlen(editor.rename_qualified) +
                    strlen(" -> ");

                prefix = label;
            } else {
                prefix = strlen(" Theme: ");
            }
            buffer_printf(&output, "\x1b[%d;%zuH\x1b[?25h",
                          editor.screen_rows,
                          prefix + editor.prompt_length + 1);
        }

        if (!editor.follow_cursor) {
            if (editor.cursor_y < editor.row_offset ||
                editor.cursor_y >= editor.row_offset + (size_t) scroll_rows) {
                show_cursor = false;
            } else {
                cursor_screen_row +=
                    sticky_rows + (editor.cursor_y - editor.row_offset);
            }
        } else if (editor.cursor_y >= editor.row_offset) {
            cursor_screen_row +=
                sticky_rows + (editor.cursor_y - editor.row_offset);
        } else {
            show_cursor = false;
            for (size_t s = 0; s < sticky.count; s++) {
                if (sticky.rows[s] == editor.cursor_y) {
                    cursor_screen_row += s;
                    show_cursor = true;
                    break;
                }
            }
        }
        if (ide_menu_is_open(&editor.menu)) {
            show_cursor = false;
        }
        if (show_cursor) {
            buffer_printf(&output, "\x1b[%zu;%zuH\x1b[?25h",
                          cursor_screen_row,
                          editor.cursor_x - editor.col_offset + (size_t) gutter + 1);
        }
    }
    BUFFER_LITERAL(&output, "\x1b[?2026l");
    platform_terminal_write(output.data, output.length);
    free(output.data);
}

static bool format_editor_buffer(void)
{
    EditorSnapshot before;
    bool captured = capture_snapshot(&before);
    bool changed = false;

    for (size_t i = 0; i < editor.row_count; i++) {
        Row *row = &editor.rows[i];
        ssize_t formatted =
            cg_format_text_line(row->data, row->length, row->capacity);

        if (formatted < 0) {
            size_t capacity = row->length + 64;
            char *grown = realloc(row->data, capacity);

            if (!grown) {
                if (captured) {
                    free_snapshot(&before);
                }
                return false;
            }
            row->data = grown;
            row->capacity = capacity;
            formatted = cg_format_text_line(row->data, row->length,
                                            row->capacity);
            if (formatted < 0) {
                if (captured) {
                    free_snapshot(&before);
                }
                return false;
            }
        }
        if (captured &&
            ((size_t) formatted != before.rows[i].length ||
             memcmp(row->data, before.rows[i].data,
                    (size_t) formatted) != 0)) {
            changed = true;
        }
        row->length = (size_t) formatted;
    }
    if (captured && changed) {
        push_owned_snapshot(editor.history.undo, &editor.history.undo_count,
                            before);
        clear_snapshot_stack(editor.history.redo,
                             &editor.history.redo_count);
        editor.revision = ++editor.next_revision;
        editor.dirty = true;
        mark_semantic_dirty();
    } else if (captured) {
        free_snapshot(&before);
    }
    {
        Row *row = row_at(editor.cursor_y);

        if (row && editor.cursor_x > row->length) {
            editor.cursor_x = row->length;
        }
        if (row && row->length > 0 && editor.col_offset >= row->length) {
            editor.col_offset = 0;
        }
    }
    editor.follow_cursor = true;
    return true;
}

static bool format_document(void)
{
    size_t revision_before = editor.revision;

    if (!format_editor_buffer()) {
        set_message("Format failed");
        return true;
    }
    if (editor.revision != revision_before) {
        set_message("Formatted");
    } else {
        set_message("Already formatted");
    }
    return true;
}

static bool save_file(void)
{
    FILE *output;

    if (!editor.filename) {
        begin_prompt(IDE_PROMPT_SAVE_AS);
        return false;
    }
    format_editor_buffer();
    output = fopen(editor.filename, "wb");
    if (!output) {
        set_message("Save failed: %s", strerror(errno));
        return false;
    }
    for (size_t i = 0; i < editor.row_count; i++) {
        if (fwrite(editor.rows[i].data, 1, editor.rows[i].length, output) !=
                editor.rows[i].length ||
            (i + 1 < editor.row_count && fputc('\n', output) == EOF)) {
            set_message("Save failed: %s", strerror(errno));
            fclose(output);
            return false;
        }
    }
    if (fclose(output) != 0) {
        set_message("Save failed: %s", strerror(errno));
        return false;
    }
    update_saved_snapshot();
    editor.dirty = false;
    editor.saved_revision = editor.revision;
    editor.quit_pending = false;
    editor.open_pending = false;
    set_message("Saved %s", editor.filename);
    save_ide_settings();
    return true;
}

static void diagnostics_copy(DiagnosticList *dest, const DiagnosticList *src)
{
    cg_diagnostic_clear(dest);
    if (!src) {
        return;
    }
    for (size_t i = 0; i < src->count; i++) {
        const Diagnostic *item = &src->items[i];

        cg_diagnostic_push(dest, item->severity, item->line, item->column,
                           item->code, "%s", item->message);
    }
}

static bool generate_output(void)
{
    FILE *input = tmpfile();
    char error[512];
    char warning[1024];
    DiagnosticList compile_diagnostics;

    if (!input) {
        set_message("Generate failed: %s", strerror(errno));
        return false;
    }
    cg_diagnostic_init(&compile_diagnostics);
    format_editor_buffer();
    for (size_t i = 0; i < editor.row_count; i++) {
        if (fwrite(editor.rows[i].data, 1, editor.rows[i].length, input) !=
                editor.rows[i].length ||
            (i + 1 < editor.row_count && fputc('\n', input) == EOF)) {
            set_message("Generate failed: cannot prepare input");
            fclose(input);
            return false;
        }
    }
    if (fflush(input) != 0 || fseek(input, 0, SEEK_SET) != 0) {
        set_message("Generate failed: cannot prepare input");
        fclose(input);
        return false;
    }
    if (cgem_compile(input, editor.include_path, editor.source_path,
                     cgem_generated_output_style(editor.include_path,
                                                 editor.source_path),
                     editor.compiler, true,
                     warning, sizeof(warning),
                     error, sizeof(error),
                     &compile_diagnostics) != 0) {
        diagnostics_copy(&editor.diagnostics, &compile_diagnostics);
        focus_first_diagnostic(DIAG_ERROR);
        set_message("Generate failed: %s", error);
        fclose(input);
        cg_diagnostic_free(&compile_diagnostics);
        return false;
    }
    fclose(input);
    cg_diagnostic_clear(&editor.diagnostics);
    cg_diagnostic_free(&compile_diagnostics);
    mark_semantic_dirty();
    if (warning[0]) {
        set_message("Generated with warning: %s", warning);
    } else {
        set_message("Generated include: %s  source: %s",
                    editor.include_path, editor.source_path);
    }
    return true;
}

static bool has_cgem_extension(const char *path)
{
    size_t length = strlen(path);

    return length >= 5 && strcmp(path + length - 5, ".cgem") == 0;
}

static bool has_bmp_extension(const char *path)
{
    size_t length = strlen(path);

    return length >= 4 && path[length - 4] == '.' &&
           tolower((unsigned char) path[length - 3]) == 'b' &&
           tolower((unsigned char) path[length - 2]) == 'm' &&
           tolower((unsigned char) path[length - 1]) == 'p';
}

static bool load_editor_file(FILE *input)
{
    char buffer[4096];

    while (fgets(buffer, sizeof(buffer), input)) {
        size_t length = strlen(buffer);

        while (length > 0 &&
               (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
            length--;
        }
        insert_row(editor.row_count, buffer, length);
    }
    return !ferror(input);
}

static bool path_is_absolute(const char *path)
{
    return path[0] == '/' || path[0] == '\\' ||
           (isalpha((unsigned char) path[0]) && path[1] == ':');
}

static char *workspace_path(const char *path)
{
    return path_is_absolute(path) ? strdup(path)
                                  : cg_join_path(editor.workspace_root, path);
}

static bool save_file_as(const char *path)
{
    char *new_filename = workspace_path(path);
    char *old_filename;

    if (!new_filename) {
        set_message("Save As failed: out of memory");
        return false;
    }
    old_filename = editor.filename;
    editor.filename = new_filename;
    if (!save_file()) {
        editor.filename = old_filename;
        free(new_filename);
        return false;
    }
    free(old_filename);
    return true;
}

static bool open_file_path(const char *path)
{
    char *open_path;
    FILE *input;
    Row *old_rows;
    size_t old_row_count;
    size_t old_row_capacity;

    open_path = workspace_path(path);
    if (!open_path) {
        set_message("Open failed: out of memory");
        return false;
    }
    if (has_bmp_extension(open_path)) {
        char error[256];

        update_window_size();
        if (cgem_bmp_preview(open_path, editor.screen_rows,
                             editor.screen_cols, error, sizeof(error)) != 0) {
            set_message("Open failed: %s", error);
            free(open_path);
            return false;
        }
        platform_read_event();
        set_message("Previewed %s", open_path);
        free(open_path);
        return true;
    }
    input = fopen(open_path, "r");
    if (!input) {
        set_message("Open failed: %s", strerror(errno));
        free(open_path);
        return false;
    }

    old_rows = editor.rows;
    old_row_count = editor.row_count;
    old_row_capacity = editor.row_capacity;
    editor.rows = NULL;
    editor.row_count = 0;
    editor.row_capacity = 0;
    if (!load_editor_file(input)) {
        for (size_t i = 0; i < editor.row_count; i++) {
            free(editor.rows[i].data);
        }
        free(editor.rows);
        editor.rows = old_rows;
        editor.row_count = old_row_count;
        editor.row_capacity = old_row_capacity;
        set_message("Open failed: %s", strerror(errno));
        fclose(input);
        free(open_path);
        return false;
    }
    fclose(input);

    for (size_t i = 0; i < old_row_count; i++) {
        free(old_rows[i].data);
    }
    free(old_rows);
    free(editor.filename);
    editor.filename = open_path;
    clear_snapshot_stack(editor.history.undo, &editor.history.undo_count);
    clear_snapshot_stack(editor.history.redo, &editor.history.redo_count);
    editor.cursor_x = 0;
    editor.cursor_y = 0;
    editor.row_offset = 0;
    editor.col_offset = 0;
    editor.revision = ++editor.next_revision;
    editor.saved_revision = editor.revision;
    editor.dirty = false;
    editor.quit_pending = false;
    editor.open_pending = false;
    editor.follow_cursor = true;
    clear_selection();
    update_saved_snapshot();
    mark_semantic_dirty();
    apply_pending_goto();
    save_ide_settings();
    set_message("Opened %s", editor.filename);
    return true;
}

static bool open_editor_input(const char *path)
{
    FILE *input;
    char *computed_file = NULL;
    const char *open_path;
    bool directory_input = platform_path_is_directory(path);

    free(editor.workspace_root);
    if (directory_input) {
        editor.workspace_root = strdup(path);
    } else {
        editor.workspace_root = path_dirname(path);
    }
    if (!editor.workspace_root) {
        die("strdup");
    }

    themes_init();
    load_ide_settings();

    if (directory_input) {
        const char *rel = ide_settings.last_file[0] ? ide_settings.last_file
                                                    : "main.cgem";
        computed_file = cg_join_path(editor.workspace_root, rel);
        if (!computed_file) {
            die("malloc");
        }
        open_path = computed_file;
    } else if (platform_path_is_regular_file(path)) {
        open_path = path;
    } else if (!platform_path_exists(path) && has_cgem_extension(path)) {
        open_path = path;
    } else {
        fprintf(stderr, "cgem: %s is not a file or directory\n", path);
        free(computed_file);
        return false;
    }

    editor.filename = strdup(open_path);
    free(computed_file);
    if (!editor.filename) {
        die("strdup");
    }

    input = fopen(editor.filename, "r");
    if (!input) {
        if (errno == ENOENT) {
            update_saved_snapshot();
            set_message("New file: %s", editor.filename);
            return true;
        }
        fprintf(stderr, "cgem: %s: %s\n",
                editor.filename, strerror(errno));
        return false;
    }
    if (!load_editor_file(input)) {
        fprintf(stderr, "cgem: %s: read failed\n", editor.filename);
        fclose(input);
        return false;
    }
    fclose(input);
    editor.dirty = false;
    update_saved_snapshot();
    cg_diagnostic_clear(&editor.diagnostics);
    mark_semantic_dirty();
    set_message("Opened %s", editor.filename);
    return true;
}

static int menu_index_for_key_action(IdeKeyAction action)
{
    switch (action) {
    case IDE_KEY_MENU_FILE:
        return IDE_MENU_FILE;
    case IDE_KEY_MENU_EDIT:
        return IDE_MENU_EDIT;
    case IDE_KEY_MENU_BUILD:
        return IDE_MENU_BUILD;
    case IDE_KEY_MENU_OPTIONS:
        return IDE_MENU_OPTIONS;
    case IDE_KEY_MENU_HELP:
        return IDE_MENU_HELP;
    default:
        return -1;
    }
}

static bool handle_key_binding(IdeKeyAction action)
{
    switch (action) {
    case IDE_KEY_OPEN:
        return handle_menu_action(IDE_MENU_ACTION_OPEN);
    case IDE_KEY_SAVE:
        save_file();
        return true;
    case IDE_KEY_GENERATE:
        generate_output();
        return true;
    case IDE_KEY_THEME:
        begin_theme_prompt();
        return true;
    case IDE_KEY_HELP:
        set_message(IDE_HELP_HINT);
        return true;
    case IDE_KEY_COPY:
        copy_selection();
        return true;
    case IDE_KEY_CUT:
        cut_selection();
        return true;
    case IDE_KEY_PASTE:
        paste_clipboard(false);
        return true;
    case IDE_KEY_PASTE_FORMATTED:
        paste_clipboard(true);
        return true;
    case IDE_KEY_SELECT_ALL:
        select_all();
        return true;
    case IDE_KEY_UNDO:
        history_step(false);
        return true;
    case IDE_KEY_REDO:
        history_step(true);
        return true;
    case IDE_KEY_FIND:
        begin_prompt(IDE_PROMPT_FIND);
        return true;
    case IDE_KEY_FIND_NEXT:
        find_next_match(editor.search_text);
        return true;
    case IDE_KEY_GOTO_LINE:
        begin_prompt(IDE_PROMPT_GOTO_LINE);
        return true;
    case IDE_KEY_GOTO_DEFINITION:
        goto_definition_at_cursor();
        return true;
    case IDE_KEY_RENAME:
        begin_rename_at_cursor();
        return true;
    case IDE_KEY_FORMAT:
        format_document();
        return true;
    case IDE_KEY_QUIT:
        if (editor.dirty && !editor.quit_pending) {
            editor.quit_pending = true;
            set_message("Unsaved changes. Press Quit again to discard.");
            return true;
        }
        return false;
    case IDE_KEY_NONE:
    case IDE_KEY_MENU_FILE:
    case IDE_KEY_MENU_EDIT:
    case IDE_KEY_MENU_BUILD:
    case IDE_KEY_MENU_OPTIONS:
    case IDE_KEY_MENU_HELP:
        break;
    }
    return false;
}

static bool process_key(void)
{
    PlatformEvent event = platform_read_event();
    int key;
    Row *row;

    if (event.kind != PLATFORM_EVENT_KEY) {
        return handle_mouse_event(&event);
    }
    key = event.key;

    if (editor.prompt != IDE_PROMPT_NONE) {
        handle_prompt_key(key);
        return true;
    }

    {
        IdeKeyAction binding = ide_keymap_lookup(key);
        int menu_index = menu_index_for_key_action(binding);

        if (menu_index >= 0) {
            IdeMenuAction immediate =
                ide_menu_open_bar(&editor.menu, menu_index);

            if (immediate != IDE_MENU_ACTION_NONE &&
                !handle_menu_action(immediate)) {
                return false;
            }
            return true;
        }
    }

    if (ide_menu_is_open(&editor.menu)) {
        IdeKeyAction binding = ide_keymap_lookup(key);
        int menu_index = menu_index_for_key_action(binding);

        if (menu_index < 0 && binding != IDE_KEY_NONE) {
            if (binding == IDE_KEY_QUIT) {
                ide_menu_close(&editor.menu);
                return handle_key_binding(binding);
            }
            if (handle_key_binding(binding)) {
                ide_menu_close(&editor.menu);
                return true;
            }
        }

        IdeMenuKeyResult menu = ide_menu_handle_key(&editor.menu, key);

        if (menu.quit_request) {
            if (editor.dirty && !editor.quit_pending) {
                editor.quit_pending = true;
                set_message(
                    "Unsaved changes. Press Quit again to discard.");
                return true;
            }
            return false;
        }
        if (menu.action != IDE_MENU_ACTION_NONE &&
            !handle_menu_action(menu.action)) {
            return false;
        }
        return true;
    }

    {
        IdeKeyAction binding = ide_keymap_lookup(key);

        if (binding == IDE_KEY_QUIT) {
            return handle_key_binding(binding);
        }
        if (binding != IDE_KEY_NONE && handle_key_binding(binding)) {
            return true;
        }
    }

    if (key != '\t' && key != KEY_SHIFT_TAB &&
        key != KEY_SHIFT_ARROW_UP && key != KEY_SHIFT_ARROW_DOWN &&
        key != KEY_SHIFT_ARROW_LEFT && key != KEY_SHIFT_ARROW_RIGHT &&
        key != KEY_BACKSPACE && key != 8 && key != KEY_DELETE) {
        clear_selection();
    }

    switch (key) {
    case '\r':
        insert_newline();
        break;
    case '\t':
        editor_indent_action();
        break;
    case KEY_SHIFT_TAB:
        editor_unindent_action();
        break;
    case KEY_BACKSPACE:
    case 8:
        if (editor.selection_active) {
            delete_selection();
            clear_selection();
        } else {
            delete_char();
        }
        break;
    case KEY_DELETE:
        if (editor.selection_active) {
            delete_selection();
            clear_selection();
        } else {
            delete_char_forward();
        }
        break;
    case KEY_HOME:
        editor.cursor_x = 0;
        break;
    case KEY_END:
        row = row_at(editor.cursor_y);
        editor.cursor_x = row ? row->length : 0;
        break;
    case KEY_PAGE_UP:
        page_editor(true);
        break;
    case KEY_PAGE_DOWN:
        page_editor(false);
        break;
    case KEY_ARROW_UP:
    case KEY_ARROW_DOWN:
    case KEY_ARROW_LEFT:
    case KEY_ARROW_RIGHT:
        move_cursor(key);
        break;
    case KEY_SHIFT_ARROW_UP:
    case KEY_SHIFT_ARROW_DOWN:
    case KEY_SHIFT_ARROW_LEFT:
    case KEY_SHIFT_ARROW_RIGHT:
        if (!editor.selection_active) {
            editor.selection_anchor_x = editor.cursor_x;
            editor.selection_anchor_y = editor.cursor_y;
        }
        move_cursor(key);
        update_selection_active();
        break;
    case '\x1b':
        break;
    default:
        if (key >= 32 && key <= 126) {
            insert_char((char) key);
        }
        break;
    }
    return true;
}

static void free_editor(void)
{
    clear_snapshot_stack(editor.history.undo, &editor.history.undo_count);
    clear_snapshot_stack(editor.history.redo, &editor.history.redo_count);
    for (size_t i = 0; i < editor.row_count; i++) {
        free(editor.rows[i].data);
    }
    free(editor.rows);
    free_snapshot(&editor.saved_snapshot);
    free(editor.line_changes);
    free(editor.deletion_before);
    free(editor.filename);
    free(editor.workspace_root);
    cg_diagnostic_free(&editor.diagnostics);
    cgem_semantic_free(&editor.semantic);
}

int ide_run(const char *input_path, const char *include_path,
            const char *source_path, const char *compiler)
{
    editor.include_path = include_path;
    editor.source_path = source_path;
    editor.compiler = compiler;

    if (!platform_terminal_is_interactive()) {
        fprintf(stderr, "cgem requires an interactive terminal\n");
        return EXIT_FAILURE;
    }

    if (!open_editor_input(input_path ? input_path : ".")) {
        free_editor();
        return EXIT_FAILURE;
    }
    cg_diagnostic_init(&editor.diagnostics);
    cgem_semantic_init(&editor.semantic);
    editor.semantic_dirty = true;
    editor.follow_cursor = true;
    ide_menu_init(&editor.menu);
    ide_keymap_init(editor.workspace_root, editor.include_path);
    set_message("Include: %s  Source: %s",
                editor.include_path, editor.source_path);

    platform_terminal_init();
    ensure_semantic_fresh(true);
    while (true) {
        refresh_screen();
        if (!process_key()) {
            break;
        }
    }

    platform_terminal_clear();
    save_ide_settings();
    free_editor();
    return EXIT_SUCCESS;
}
