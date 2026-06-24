#include "cgem/palette.h"

/* Far Manager black scheme — gray on black, navy selection bar */
const ThemePalette theme_palette_far_dark = {
    .name = "far-dark",
    .editor = TC(192, 192, 192, 0, 0, 0),
    .editor_active = TC(255, 255, 255, 0, 0, 128),
    .keyword = TCB(255, 255, 255, 0, 0, 0),
    .keyword_active = TCB(255, 255, 255, 0, 0, 128),
    .name_color = TC(0, 255, 0, 0, 0, 0),
    .name_active = TC(0, 255, 0, 0, 0, 128),
    .punctuation = TC(0, 255, 255, 0, 0, 0),
    .punctuation_active = TC(0, 255, 255, 0, 0, 128),
    .builtin = TC(255, 0, 255, 0, 0, 0),
    .builtin_active = TC(255, 0, 255, 0, 0, 128),
    .string_color = TC(255, 255, 0, 0, 0, 0),
    .string_active = TC(255, 255, 0, 0, 0, 128),
    .muted = TC(128, 128, 128, 0, 0, 0),
    .header = TC(192, 192, 192, 0, 0, 0),
    .status = TC(192, 192, 192, 0, 0, 0),
    .gutter = TC(0, 255, 255, 0, 0, 0),
    .ghost = TC(128, 128, 128, 0, 0, 0),
    .ghost_active = TC(128, 128, 128, 0, 0, 128),
    .doc_muted = TC(0, 200, 0, 0, 0, 0),
    .builtin_muted = TC(200, 0, 200, 0, 0, 0),
    .string_muted = TC(200, 200, 0, 0, 0, 0),
    .punct_muted = TC(0, 200, 200, 0, 0, 0),
    .gutter_active_row = TCB(255, 255, 255, 0, 0, 128),
};
