#include "cgem/palette.h"

/* Atom Material — atom-material-syntax + atom-material-ui palette */
const ThemePalette theme_palette_atom = {
    .name = "atom",
    .editor = TC(238, 255, 255, 38, 50, 56),
    .editor_active = TC(238, 255, 255, 48, 69, 76),
    .keyword = TCB(199, 146, 234, 38, 50, 56),
    .keyword_active = TCB(199, 146, 234, 48, 69, 76),
    .name_color = TC(130, 170, 255, 38, 50, 56),
    .name_active = TC(130, 170, 255, 48, 69, 76),
    .punctuation = TC(137, 221, 243, 38, 50, 56),
    .punctuation_active = TC(137, 221, 243, 48, 69, 76),
    .builtin = TC(247, 140, 106, 38, 50, 56),
    .builtin_active = TC(247, 140, 106, 48, 69, 76),
    .string_color = TC(195, 232, 141, 38, 50, 56),
    .string_active = TC(195, 232, 141, 48, 69, 76),
    .muted = TC(84, 110, 122, 38, 50, 56),
    .header = TC(238, 255, 255, 0, 150, 136),
    .status = TC(178, 204, 214, 38, 50, 56),
    .gutter = TC(74, 96, 106, 38, 50, 56),
    .ghost = TC(178, 204, 214, 38, 50, 56),
    .ghost_active = TC(178, 204, 214, 48, 69, 76),
    .doc_muted = TC(100, 125, 135, 38, 50, 56),
    .builtin_muted = TC(200, 125, 95, 38, 50, 56),
    .string_muted = TC(155, 185, 115, 38, 50, 56),
    .punct_muted = TC(115, 185, 200, 38, 50, 56),
    .gutter_active_row = TCB(255, 255, 255, 0, 150, 136),
};
