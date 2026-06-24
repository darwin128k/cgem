#include "cgem/palette.h"

/* Xcode dark — default since Xcode 9+ */
const ThemePalette theme_palette_xcode_dark = {
    .name = "xcode-dark",
    .editor = TC(223, 223, 224, 30, 30, 30),
    .editor_active = TC(223, 223, 224, 38, 38, 38),
    .keyword = TCB(252, 95, 163, 30, 30, 30),
    .keyword_active = TCB(252, 95, 163, 38, 38, 38),
    .name_color = TC(172, 242, 228, 30, 30, 30),
    .name_active = TC(172, 242, 228, 38, 38, 38),
    .punctuation = TC(223, 223, 224, 30, 30, 30),
    .punctuation_active = TC(223, 223, 224, 38, 38, 38),
    .builtin = TC(253, 143, 63, 30, 30, 30),
    .builtin_active = TC(253, 143, 63, 38, 38, 38),
    .string_color = TC(252, 106, 93, 30, 30, 30),
    .string_active = TC(252, 106, 93, 38, 38, 38),
    .muted = TC(108, 121, 134, 30, 30, 30),
    .header = TC(223, 223, 224, 38, 38, 38),
    .status = TC(108, 121, 134, 25, 25, 25),
    .gutter = TC(108, 121, 134, 30, 30, 30),
    .ghost = TC(108, 121, 134, 30, 30, 30),
    .ghost_active = TC(108, 121, 134, 38, 38, 38),
    .doc_muted = TC(108, 121, 134, 30, 30, 30),
    .builtin_muted = TC(210, 125, 55, 30, 30, 30),
    .string_muted = TC(210, 95, 85, 30, 30, 30),
    .punct_muted = TC(150, 150, 155, 30, 30, 30),
    .gutter_active_row = TCB(252, 95, 163, 38, 38, 38),
};
