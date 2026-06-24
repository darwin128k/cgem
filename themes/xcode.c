#include "cgem/palette.h"

/* Classic Xcode light editor colors */
const ThemePalette theme_palette_xcode = {
    .name = "xcode",
    .editor = TC(0, 0, 0, 255, 255, 255),
    .editor_active = TC(0, 0, 0, 245, 245, 247),
    .keyword = TCB(173, 61, 164, 255, 255, 255),
    .keyword_active = TCB(173, 61, 164, 245, 245, 247),
    .name_color = TC(28, 0, 207, 255, 255, 255),
    .name_active = TC(28, 0, 207, 245, 245, 247),
    .punctuation = TC(0, 0, 0, 255, 255, 255),
    .punctuation_active = TC(0, 0, 0, 245, 245, 247),
    .builtin = TC(170, 85, 0, 255, 255, 255),
    .builtin_active = TC(170, 85, 0, 245, 245, 247),
    .string_color = TC(196, 26, 22, 255, 255, 255),
    .string_active = TC(196, 26, 22, 245, 245, 247),
    .muted = TC(106, 115, 125, 255, 255, 255),
    .header = TC(0, 0, 0, 245, 245, 247),
    .status = TC(106, 115, 125, 235, 235, 237),
    .gutter = TC(106, 115, 125, 255, 255, 255),
    .ghost = TC(106, 115, 125, 255, 255, 255),
    .ghost_active = TC(106, 115, 125, 245, 245, 247),
    .doc_muted = TC(0, 116, 0, 255, 255, 255),
    .builtin_muted = TC(120, 73, 42, 255, 255, 255),
    .string_muted = TC(160, 50, 45, 255, 255, 255),
    .punct_muted = TC(80, 80, 80, 255, 255, 255),
    .gutter_active_row = TCB(173, 61, 164, 245, 245, 247),
};
