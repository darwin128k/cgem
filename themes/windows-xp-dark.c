#include "cgem/palette.h"

/* Windows XP — stylized dark (Luna blue on charcoal) */
const ThemePalette theme_palette_windows_xp_dark = {
    .name = "windows-xp-dark",
    .editor = TC(220, 220, 210, 28, 28, 26),
    .editor_active = TC(220, 220, 210, 38, 38, 35),
    .keyword = TCB(49, 106, 197, 28, 28, 26),
    .keyword_active = TCB(49, 106, 197, 38, 38, 35),
    .name_color = TC(80, 200, 80, 28, 28, 26),
    .name_active = TC(80, 200, 80, 38, 38, 35),
    .punctuation = TC(0, 84, 227, 28, 28, 26),
    .punctuation_active = TC(0, 84, 227, 38, 38, 35),
    .builtin = TC(200, 100, 200, 28, 28, 26),
    .builtin_active = TC(200, 100, 200, 38, 38, 35),
    .string_color = TC(255, 140, 140, 28, 28, 26),
    .string_active = TC(255, 140, 140, 38, 38, 35),
    .muted = TC(140, 140, 130, 28, 28, 26),
    .header = TC(255, 255, 255, 0, 60, 180),
    .status = TC(200, 200, 190, 22, 22, 20),
    .gutter = TC(140, 140, 130, 28, 28, 26),
    .ghost = TC(120, 120, 110, 28, 28, 26),
    .ghost_active = TC(120, 120, 110, 38, 38, 35),
    .doc_muted = TC(100, 180, 100, 28, 28, 26),
    .builtin_muted = TC(160, 80, 160, 28, 28, 26),
    .string_muted = TC(200, 110, 110, 28, 28, 26),
    .punct_muted = TC(80, 120, 200, 28, 28, 26),
    .gutter_active_row = TCB(49, 106, 197, 38, 38, 35),
};
