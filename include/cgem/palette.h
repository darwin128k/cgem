#ifndef CGEM_THEME_PALETTE_H
#define CGEM_THEME_PALETTE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t fr, fg, fb;
    uint8_t br, bg, bb;
    bool bold;
} ThemeColor;

#define TC(fr, fg, fb, br, bg, bb) \
    { (uint8_t)(fr), (uint8_t)(fg), (uint8_t)(fb), \
      (uint8_t)(br), (uint8_t)(bg), (uint8_t)(bb), false }

#define TCB(fr, fg, fb, br, bg, bb) \
    { (uint8_t)(fr), (uint8_t)(fg), (uint8_t)(fb), \
      (uint8_t)(br), (uint8_t)(bg), (uint8_t)(bb), true }

typedef struct {
    const char *name;
    ThemeColor editor;
    ThemeColor editor_active;
    ThemeColor keyword;
    ThemeColor keyword_active;
    ThemeColor name_color;
    ThemeColor name_active;
    ThemeColor punctuation;
    ThemeColor punctuation_active;
    ThemeColor builtin;
    ThemeColor builtin_active;
    ThemeColor string_color;
    ThemeColor string_active;
    ThemeColor muted;
    ThemeColor header;
    ThemeColor status;
    ThemeColor gutter;
    ThemeColor ghost;
    ThemeColor ghost_active;
    ThemeColor doc_muted;
    ThemeColor builtin_muted;
    ThemeColor string_muted;
    ThemeColor punct_muted;
    ThemeColor gutter_active_row;
} ThemePalette;

void theme_color_format(const ThemeColor *color, char *buf, size_t bufsize);
void theme_color_fg(const ThemeColor *color, char *buf, size_t bufsize);

#endif
