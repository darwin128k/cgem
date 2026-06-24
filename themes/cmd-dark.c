#include "cgem/palette.h"

/* Classic cmd.exe — silver on black, default Windows console */
const ThemePalette theme_palette_cmd_dark = {
    .name = "cmd-dark",
    .editor = TC(192, 192, 192, 0, 0, 0),
    .editor_active = TC(255, 255, 255, 16, 16, 16),
    .keyword = TCB(255, 255, 255, 0, 0, 0),
    .keyword_active = TCB(255, 255, 255, 16, 16, 16),
    .name_color = TC(0, 255, 0, 0, 0, 0),
    .name_active = TC(0, 255, 0, 16, 16, 16),
    .punctuation = TC(192, 192, 192, 0, 0, 0),
    .punctuation_active = TC(192, 192, 192, 16, 16, 16),
    .builtin = TC(255, 255, 0, 0, 0, 0),
    .builtin_active = TC(255, 255, 0, 16, 16, 16),
    .string_color = TC(255, 0, 0, 0, 0, 0),
    .string_active = TC(255, 0, 0, 16, 16, 16),
    .muted = TC(128, 128, 128, 0, 0, 0),
    .header = TC(192, 192, 192, 0, 0, 128),
    .status = TC(192, 192, 192, 0, 0, 0),
    .gutter = TC(128, 128, 128, 0, 0, 0),
    .ghost = TC(128, 128, 128, 0, 0, 0),
    .ghost_active = TC(128, 128, 128, 16, 16, 16),
    .doc_muted = TC(0, 200, 0, 0, 0, 0),
    .builtin_muted = TC(200, 200, 0, 0, 0, 0),
    .string_muted = TC(200, 80, 80, 0, 0, 0),
    .punct_muted = TC(160, 160, 160, 0, 0, 0),
    .gutter_active_row = TCB(255, 255, 255, 16, 16, 16),
};
