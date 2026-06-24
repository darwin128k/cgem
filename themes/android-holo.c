#include "cgem/palette.h"

/* Android Holo Dark — ICS 4.0 / Jelly Bean 4.1 palette */
const ThemePalette theme_palette_android_holo = {
    .name = "android-holo",
    .editor = TC(255, 255, 255, 0, 0, 0),
    .editor_active = TC(255, 255, 255, 24, 24, 24),
    .keyword = TCB(51, 181, 229, 0, 0, 0),
    .keyword_active = TCB(51, 181, 229, 24, 24, 24),
    .name_color = TC(153, 204, 0, 0, 0, 0),
    .name_active = TC(153, 204, 0, 24, 24, 24),
    .punctuation = TC(0, 153, 204, 0, 0, 0),
    .punctuation_active = TC(0, 153, 204, 24, 24, 24),
    .builtin = TC(170, 102, 204, 0, 0, 0),
    .builtin_active = TC(170, 102, 204, 24, 24, 24),
    .string_color = TC(255, 187, 51, 0, 0, 0),
    .string_active = TC(255, 187, 51, 24, 24, 24),
    .muted = TC(170, 170, 170, 0, 0, 0),
    .header = TC(255, 255, 255, 34, 34, 34),
    .status = TC(170, 170, 170, 18, 18, 18),
    .gutter = TC(170, 170, 170, 0, 0, 0),
    .ghost = TC(102, 102, 102, 0, 0, 0),
    .ghost_active = TC(102, 102, 102, 24, 24, 24),
    .doc_muted = TC(102, 153, 0, 0, 0, 0),
    .builtin_muted = TC(130, 80, 160, 0, 0, 0),
    .string_muted = TC(200, 150, 40, 0, 0, 0),
    .punct_muted = TC(0, 120, 160, 0, 0, 0),
    .gutter_active_row = TCB(51, 181, 229, 24, 24, 24),
};
