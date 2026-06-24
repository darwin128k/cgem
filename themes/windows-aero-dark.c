#include "cgem/palette.h"

/* Windows Vista / 7 Aero — dark glass shell */
const ThemePalette theme_palette_windows_aero_dark = {
    .name = "windows-aero-dark",
    .editor = TC(220, 230, 240, 30, 40, 55),
    .editor_active = TC(220, 230, 240, 38, 50, 68),
    .keyword = TCB(102, 178, 255, 30, 40, 55),
    .keyword_active = TCB(102, 178, 255, 38, 50, 68),
    .name_color = TC(120, 210, 100, 30, 40, 55),
    .name_active = TC(120, 210, 100, 38, 50, 68),
    .punctuation = TC(140, 180, 220, 30, 40, 55),
    .punctuation_active = TC(140, 180, 220, 38, 50, 68),
    .builtin = TC(180, 140, 220, 30, 40, 55),
    .builtin_active = TC(180, 140, 220, 38, 50, 68),
    .string_color = TC(255, 130, 130, 30, 40, 55),
    .string_active = TC(255, 130, 130, 38, 50, 68),
    .muted = TC(130, 145, 165, 30, 40, 55),
    .header = TC(255, 255, 255, 20, 35, 55),
    .status = TC(180, 195, 215, 24, 32, 48),
    .gutter = TC(130, 145, 165, 30, 40, 55),
    .ghost = TC(110, 125, 145, 30, 40, 55),
    .ghost_active = TC(110, 125, 145, 38, 50, 68),
    .doc_muted = TC(100, 180, 140, 30, 40, 55),
    .builtin_muted = TC(150, 115, 185, 30, 40, 55),
    .string_muted = TC(210, 110, 110, 30, 40, 55),
    .punct_muted = TC(110, 150, 190, 30, 40, 55),
    .gutter_active_row = TCB(102, 178, 255, 38, 50, 68),
};
