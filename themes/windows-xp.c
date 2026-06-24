#include "cgem/palette.h"

/* Windows XP Luna — classic blue title bar, warm gray chrome */
const ThemePalette theme_palette_windows_xp = {
    .name = "windows-xp",
    .editor = TC(0, 0, 0, 255, 255, 255),
    .editor_active = TC(0, 0, 0, 245, 245, 240),
    .keyword = TCB(0, 84, 227, 255, 255, 255),
    .keyword_active = TCB(0, 84, 227, 245, 245, 240),
    .name_color = TC(0, 100, 0, 255, 255, 255),
    .name_active = TC(0, 100, 0, 245, 245, 240),
    .punctuation = TC(49, 106, 197, 255, 255, 255),
    .punctuation_active = TC(49, 106, 197, 245, 245, 240),
    .builtin = TC(128, 0, 128, 255, 255, 255),
    .builtin_active = TC(128, 0, 128, 245, 245, 240),
    .string_color = TC(128, 0, 0, 255, 255, 255),
    .string_active = TC(128, 0, 0, 245, 245, 240),
    .muted = TC(96, 96, 96, 255, 255, 255),
    .header = TC(255, 255, 255, 0, 84, 227),
    .status = TC(0, 0, 0, 236, 233, 216),
    .gutter = TC(96, 96, 96, 255, 255, 255),
    .ghost = TC(128, 128, 128, 255, 255, 255),
    .ghost_active = TC(128, 128, 128, 245, 245, 240),
    .doc_muted = TC(0, 128, 0, 255, 255, 255),
    .builtin_muted = TC(100, 0, 100, 255, 255, 255),
    .string_muted = TC(100, 0, 0, 255, 255, 255),
    .punct_muted = TC(80, 90, 150, 255, 255, 255),
    .gutter_active_row = TCB(49, 106, 197, 245, 245, 240),
};
