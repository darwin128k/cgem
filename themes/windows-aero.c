#include "cgem/palette.h"

/* Windows Vista / 7 Aero — glass blue accent, cool gray chrome */
const ThemePalette theme_palette_windows_aero = {
    .name = "windows-aero",
    .editor = TC(0, 0, 0, 255, 255, 255),
    .editor_active = TC(0, 0, 0, 240, 248, 255),
    .keyword = TCB(51, 153, 255, 255, 255, 255),
    .keyword_active = TCB(51, 153, 255, 240, 248, 255),
    .name_color = TC(38, 127, 0, 255, 255, 255),
    .name_active = TC(38, 127, 0, 240, 248, 255),
    .punctuation = TC(43, 87, 154, 255, 255, 255),
    .punctuation_active = TC(43, 87, 154, 240, 248, 255),
    .builtin = TC(112, 48, 160, 255, 255, 255),
    .builtin_active = TC(112, 48, 160, 240, 248, 255),
    .string_color = TC(192, 0, 0, 255, 255, 255),
    .string_active = TC(192, 0, 0, 240, 248, 255),
    .muted = TC(96, 96, 96, 255, 255, 255),
    .header = TC(255, 255, 255, 30, 57, 85),
    .status = TC(30, 57, 85, 232, 236, 241),
    .gutter = TC(96, 96, 96, 255, 255, 255),
    .ghost = TC(128, 128, 128, 255, 255, 255),
    .ghost_active = TC(128, 128, 128, 240, 248, 255),
    .doc_muted = TC(0, 128, 64, 255, 255, 255),
    .builtin_muted = TC(90, 40, 130, 255, 255, 255),
    .string_muted = TC(150, 0, 0, 255, 255, 255),
    .punct_muted = TC(60, 100, 160, 255, 255, 255),
    .gutter_active_row = TCB(51, 153, 255, 240, 248, 255),
};
