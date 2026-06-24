#include "cgem/palette.h"

/* Windows 8.1 / 10 / 11 — system dark mode */
const ThemePalette theme_palette_windows_modern_dark = {
    .name = "windows-modern-dark",
    .editor = TC(204, 204, 204, 32, 32, 32),
    .editor_active = TC(204, 204, 204, 40, 40, 40),
    .keyword = TCB(96, 205, 255, 32, 32, 32),
    .keyword_active = TCB(96, 205, 255, 40, 40, 40),
    .name_color = TC(78, 201, 176, 32, 32, 32),
    .name_active = TC(78, 201, 176, 40, 40, 40),
    .punctuation = TC(0, 120, 215, 32, 32, 32),
    .punctuation_active = TC(0, 120, 215, 40, 40, 40),
    .builtin = TC(180, 160, 255, 32, 32, 32),
    .builtin_active = TC(180, 160, 255, 40, 40, 40),
    .string_color = TC(206, 145, 120, 32, 32, 32),
    .string_active = TC(206, 145, 120, 40, 40, 40),
    .muted = TC(133, 133, 133, 32, 32, 32),
    .header = TC(255, 255, 255, 0, 120, 215),
    .status = TC(204, 204, 204, 25, 25, 25),
    .gutter = TC(133, 133, 133, 32, 32, 32),
    .ghost = TC(110, 110, 110, 32, 32, 32),
    .ghost_active = TC(110, 110, 110, 40, 40, 40),
    .doc_muted = TC(106, 153, 85, 32, 32, 32),
    .builtin_muted = TC(150, 135, 210, 32, 32, 32),
    .string_muted = TC(175, 125, 100, 32, 32, 32),
    .punct_muted = TC(90, 150, 200, 32, 32, 32),
    .gutter_active_row = TCB(0, 120, 215, 40, 40, 40),
};
