#include "cgem/palette.h"

/* Monokai Pro Light — warm paper, classic Monokai accents */
const ThemePalette theme_palette_monokai_light = {
    .name = "monokai-light",
    .editor = TC(39, 40, 34, 253, 253, 251),
    .editor_active = TC(39, 40, 34, 245, 245, 240),
    .keyword = TCB(249, 38, 114, 253, 253, 251),
    .keyword_active = TCB(249, 38, 114, 245, 245, 240),
    .name_color = TC(110, 150, 0, 253, 253, 251),
    .name_active = TC(110, 150, 0, 245, 245, 240),
    .punctuation = TC(0, 140, 160, 253, 253, 251),
    .punctuation_active = TC(0, 140, 160, 245, 245, 240),
    .builtin = TC(130, 80, 200, 253, 253, 251),
    .builtin_active = TC(130, 80, 200, 245, 245, 240),
    .string_color = TC(160, 120, 0, 253, 253, 251),
    .string_active = TC(160, 120, 0, 245, 245, 240),
    .muted = TC(117, 113, 94, 253, 253, 251),
    .header = TC(39, 40, 34, 245, 245, 240),
    .status = TC(117, 113, 94, 235, 235, 230),
    .gutter = TC(117, 113, 94, 253, 253, 251),
    .ghost = TC(117, 113, 94, 253, 253, 251),
    .ghost_active = TC(117, 113, 94, 245, 245, 240),
    .doc_muted = TC(98, 96, 86, 253, 253, 251),
    .builtin_muted = TC(105, 100, 115, 253, 253, 251),
    .string_muted = TC(110, 106, 90, 253, 253, 251),
    .punct_muted = TC(92, 98, 100, 253, 253, 251),
    .gutter_active_row = TCB(249, 38, 114, 245, 245, 240),
};
