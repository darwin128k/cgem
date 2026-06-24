#ifndef CGEM_THEME_H
#define CGEM_THEME_H

#include <stddef.h>

typedef struct {
    const char *name;
    char editor[56];
    char editor_active[56];
    char keyword[56];
    char keyword_active[56];
    char name_color[56];
    char name_active[56];
    char punctuation[56];
    char punctuation_active[56];
    char builtin[56];
    char builtin_active[56];
    char string_color[56];
    char string_active[56];
    char muted[56];
    char header[56];
    char status[56];
    char gutter[56];
    char ghost[56];
    char ghost_active[56];
    char doc_muted[56];
    char builtin_muted[56];
    char string_muted[56];
    char punct_muted[56];
    char gutter_active_row[56];
    char gutter_line[56];
    char gutter_line_stripe[56];
    char gutter_line_active[56];
    char menu_active[56];
    char menu_section[56];
    char error_line[56];
    char warning_line[56];
    char error_gutter[56];
    char warning_gutter[56];
    char diff_added[56];
    char diff_modified[56];
    char diff_deleted[56];
    char sidebar[56];
    char sidebar_active[56];
    struct {
        char editor[56];
        char keyword[56];
        char name_color[56];
        char punctuation[56];
        char builtin[56];
        char string_color[56];
        char ghost[56];
        char doc_muted[56];
        char builtin_muted[56];
        char string_muted[56];
        char punct_muted[56];
    } stripe;
} EditorTheme;

void themes_init(void);
size_t theme_count(void);
const EditorTheme *theme_get(size_t index);
size_t theme_find_index(const char *name);

#endif
