#include "cgem/palette.h"

#include <stdio.h>

void theme_color_format(const ThemeColor *color, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize,
             "\x1b[%d;24;38;2;%u;%u;%u;48;2;%u;%u;%um",
             color->bold ? 1 : 22,
             color->fr, color->fg, color->fb,
             color->br, color->bg, color->bb);
}

void theme_color_fg(const ThemeColor *color, char *buf, size_t bufsize)
{
    snprintf(buf, bufsize, "\x1b[%d;24;38;2;%u;%u;%um",
             color->bold ? 1 : 22,
             color->fr, color->fg, color->fb);
}
