#include "cgem/palette.h"

/* Windows 8.1 / 10 Fluent — flat shell, #0078D7 accent */
const ThemePalette theme_palette_windows_modern = {
    .name = "windows-modern",
    .editor = TC(0, 0, 0, 255, 255, 255),
    .editor_active = TC(0, 0, 0, 243, 243, 243),
    .keyword = TCB(0, 120, 215, 255, 255, 255),
    .keyword_active = TCB(0, 120, 215, 243, 243, 243),
    .name_color = TC(16, 124, 16, 255, 255, 255),
    .name_active = TC(16, 124, 16, 243, 243, 243),
    .punctuation = TC(0, 90, 158, 255, 255, 255),
    .punctuation_active = TC(0, 90, 158, 243, 243, 243),
    .builtin = TC(107, 105, 214, 255, 255, 255),
    .builtin_active = TC(107, 105, 214, 243, 243, 243),
    .string_color = TC(163, 21, 21, 255, 255, 255),
    .string_active = TC(163, 21, 21, 243, 243, 243),
    .muted = TC(96, 96, 96, 255, 255, 255),
    .header = TC(255, 255, 255, 0, 120, 215),
    .status = TC(0, 0, 0, 243, 243, 243),
    .gutter = TC(96, 96, 96, 255, 255, 255),
    .ghost = TC(118, 118, 118, 255, 255, 255),
    .ghost_active = TC(118, 118, 118, 243, 243, 243),
    .doc_muted = TC(0, 130, 114, 255, 255, 255),
    .builtin_muted = TC(85, 85, 170, 255, 255, 255),
    .string_muted = TC(130, 40, 40, 255, 255, 255),
    .punct_muted = TC(70, 110, 150, 255, 255, 255),
    .gutter_active_row = TCB(0, 120, 215, 243, 243, 243),
};
