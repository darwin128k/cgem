#include "cgem/palette.h"

/* Ubuntu Radiance — warm light shell, orange + aubergine */
const ThemePalette theme_palette_ubuntu_light = {
    .name = "ubuntu-light",
    .editor = TC(46, 52, 54, 253, 253, 252),
    .editor_active = TC(46, 52, 54, 245, 245, 243),
    .keyword = TCB(233, 84, 32, 253, 253, 252),
    .keyword_active = TCB(233, 84, 32, 245, 245, 243),
    .name_color = TC(78, 154, 6, 253, 253, 252),
    .name_active = TC(78, 154, 6, 245, 245, 243),
    .punctuation = TC(52, 101, 164, 253, 253, 252),
    .punctuation_active = TC(52, 101, 164, 245, 245, 243),
    .builtin = TC(119, 33, 111, 253, 253, 252),
    .builtin_active = TC(119, 33, 111, 245, 245, 243),
    .string_color = TC(145, 100, 52, 253, 253, 252),
    .string_active = TC(145, 100, 52, 245, 245, 243),
    .muted = TC(136, 136, 136, 253, 253, 252),
    .header = TC(255, 255, 255, 119, 33, 111),
    .status = TC(80, 80, 80, 235, 235, 233),
    .gutter = TC(136, 136, 136, 253, 253, 252),
    .ghost = TC(136, 136, 136, 253, 253, 252),
    .ghost_active = TC(136, 136, 136, 245, 245, 243),
    .doc_muted = TC(52, 101, 164, 253, 253, 252),
    .builtin_muted = TC(100, 50, 95, 253, 253, 252),
    .string_muted = TC(125, 90, 50, 253, 253, 252),
    .punct_muted = TC(70, 120, 170, 253, 253, 252),
    .gutter_active_row = TCB(233, 84, 32, 245, 245, 243),
};
