#include "cgem/palette.h"

/* Ubuntu Ambiance / Radiance — aubergine + orange, not generic GNOME */
const ThemePalette theme_palette_ubuntu = {
    .name = "ubuntu-dark",
    .editor = TC(238, 238, 236, 48, 48, 48),
    .editor_active = TC(238, 238, 236, 58, 58, 58),
    .keyword = TCB(233, 84, 32, 48, 48, 48),
    .keyword_active = TCB(233, 84, 32, 58, 58, 58),
    .name_color = TC(138, 226, 52, 48, 48, 48),
    .name_active = TC(138, 226, 52, 58, 58, 58),
    .punctuation = TC(114, 159, 207, 48, 48, 48),
    .punctuation_active = TC(114, 159, 207, 58, 58, 58),
    .builtin = TC(173, 127, 168, 48, 48, 48),
    .builtin_active = TC(173, 127, 168, 58, 58, 58),
    .string_color = TC(210, 155, 85, 48, 48, 48),
    .string_active = TC(210, 155, 85, 58, 58, 58),
    .muted = TC(136, 136, 136, 48, 48, 48),
    .header = TC(238, 238, 236, 119, 33, 111),
    .status = TC(200, 200, 200, 44, 44, 44),
    .gutter = TC(136, 136, 136, 48, 48, 48),
    .ghost = TC(136, 136, 136, 48, 48, 48),
    .ghost_active = TC(136, 136, 136, 58, 58, 58),
    .doc_muted = TC(114, 159, 207, 48, 48, 48),
    .builtin_muted = TC(140, 100, 135, 48, 48, 48),
    .string_muted = TC(175, 130, 70, 48, 48, 48),
    .punct_muted = TC(90, 130, 175, 48, 48, 48),
    .gutter_active_row = TCB(233, 84, 32, 58, 58, 58),
};
