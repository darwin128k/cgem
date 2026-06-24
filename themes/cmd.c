#include "cgem/palette.h"

/* Classic cmd.exe light — black on silver, 16-color DOS accents */
const ThemePalette theme_palette_cmd = {
    .name = "cmd",
    .editor = TC(0, 0, 0, 192, 192, 192),
    .editor_active = TC(0, 0, 0, 180, 180, 180),
    .keyword = TCB(0, 0, 128, 192, 192, 192),
    .keyword_active = TCB(0, 0, 128, 180, 180, 180),
    .name_color = TC(0, 128, 0, 192, 192, 192),
    .name_active = TC(0, 128, 0, 180, 180, 180),
    .punctuation = TC(0, 0, 0, 192, 192, 192),
    .punctuation_active = TC(0, 0, 0, 180, 180, 180),
    .builtin = TC(128, 0, 128, 192, 192, 192),
    .builtin_active = TC(128, 0, 128, 180, 180, 180),
    .string_color = TC(128, 0, 0, 192, 192, 192),
    .string_active = TC(128, 0, 0, 180, 180, 180),
    .muted = TC(128, 128, 128, 192, 192, 192),
    .header = TC(255, 255, 255, 0, 0, 128),
    .status = TC(0, 0, 0, 168, 168, 168),
    .gutter = TC(128, 128, 128, 192, 192, 192),
    .ghost = TC(128, 128, 128, 192, 192, 192),
    .ghost_active = TC(128, 128, 128, 180, 180, 180),
    .doc_muted = TC(0, 100, 0, 192, 192, 192),
    .builtin_muted = TC(100, 0, 100, 192, 192, 192),
    .string_muted = TC(100, 0, 0, 192, 192, 192),
    .punct_muted = TC(80, 80, 80, 192, 192, 192),
    .gutter_active_row = TCB(0, 0, 128, 180, 180, 180),
};
