#include "cgem/palette.h"

/* Norton Commander / Far Manager — classic navy panel, cyan borders */
const ThemePalette theme_palette_far = {
    .name = "far",
    .editor = TC(255, 255, 255, 0, 0, 128),
    .editor_active = TC(255, 255, 255, 0, 0, 170),
    .keyword = TCB(255, 255, 255, 0, 0, 128),
    .keyword_active = TCB(255, 255, 255, 0, 0, 170),
    .name_color = TC(0, 255, 0, 0, 0, 128),
    .name_active = TC(0, 255, 0, 0, 0, 170),
    .punctuation = TC(0, 255, 255, 0, 0, 128),
    .punctuation_active = TC(0, 255, 255, 0, 0, 170),
    .builtin = TC(255, 0, 255, 0, 0, 128),
    .builtin_active = TC(255, 0, 255, 0, 0, 170),
    .string_color = TC(255, 255, 0, 0, 0, 128),
    .string_active = TC(255, 255, 0, 0, 0, 170),
    .muted = TC(192, 192, 192, 0, 0, 128),
    .header = TC(255, 255, 255, 0, 0, 128),
    .status = TC(0, 0, 0, 192, 192, 192),
    .gutter = TC(0, 255, 255, 0, 0, 128),
    .ghost = TC(192, 192, 192, 0, 0, 128),
    .ghost_active = TC(192, 192, 192, 0, 0, 170),
    .doc_muted = TC(0, 200, 0, 0, 0, 128),
    .builtin_muted = TC(200, 0, 200, 0, 0, 128),
    .string_muted = TC(200, 200, 0, 0, 0, 128),
    .punct_muted = TC(0, 200, 200, 0, 0, 128),
    .gutter_active_row = TCB(255, 255, 255, 0, 0, 170),
};
