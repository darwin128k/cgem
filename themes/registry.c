#include "cgem/theme.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "cgem/palette.h"

extern const ThemePalette theme_palette_atom;
extern const ThemePalette theme_palette_monokai_light;
extern const ThemePalette theme_palette_monokai;
extern const ThemePalette theme_palette_dracula;
extern const ThemePalette theme_palette_one_light;
extern const ThemePalette theme_palette_onedark;
extern const ThemePalette theme_palette_gruvbox_light;
extern const ThemePalette theme_palette_gruvbox_dark;
extern const ThemePalette theme_palette_nord;
extern const ThemePalette theme_palette_catppuccin;
extern const ThemePalette theme_palette_tokyo_night;
extern const ThemePalette theme_palette_solarized_light;
extern const ThemePalette theme_palette_solarized_dark;
extern const ThemePalette theme_palette_github_light;
extern const ThemePalette theme_palette_android_holo;
extern const ThemePalette theme_palette_xcode;
extern const ThemePalette theme_palette_xcode_dark;
extern const ThemePalette theme_palette_cmd;
extern const ThemePalette theme_palette_cmd_dark;
extern const ThemePalette theme_palette_windows_xp;
extern const ThemePalette theme_palette_windows_xp_dark;
extern const ThemePalette theme_palette_windows_aero;
extern const ThemePalette theme_palette_windows_aero_dark;
extern const ThemePalette theme_palette_windows_modern;
extern const ThemePalette theme_palette_windows_modern_dark;
extern const ThemePalette theme_palette_ubuntu_light;
extern const ThemePalette theme_palette_ubuntu;
extern const ThemePalette theme_palette_far;
extern const ThemePalette theme_palette_far_dark;

static const ThemePalette *const palettes[] = {
    &theme_palette_atom,
    &theme_palette_monokai_light,
    &theme_palette_monokai,
    &theme_palette_dracula,
    &theme_palette_one_light,
    &theme_palette_onedark,
    &theme_palette_gruvbox_light,
    &theme_palette_gruvbox_dark,
    &theme_palette_nord,
    &theme_palette_catppuccin,
    &theme_palette_tokyo_night,
    &theme_palette_solarized_light,
    &theme_palette_solarized_dark,
    &theme_palette_github_light,
    &theme_palette_android_holo,
    &theme_palette_xcode,
    &theme_palette_xcode_dark,
    &theme_palette_cmd,
    &theme_palette_cmd_dark,
    &theme_palette_windows_xp,
    &theme_palette_windows_xp_dark,
    &theme_palette_windows_aero,
    &theme_palette_windows_aero_dark,
    &theme_palette_windows_modern,
    &theme_palette_windows_modern_dark,
    &theme_palette_ubuntu_light,
    &theme_palette_ubuntu,
    &theme_palette_far,
    &theme_palette_far_dark,
};

static EditorTheme built[sizeof(palettes) / sizeof(palettes[0])];
static bool ready;

static uint8_t blend_channel(uint8_t from, uint8_t to, int from_percent)
{
    int to_percent = 100 - from_percent;

    return (uint8_t) (((int) from * from_percent +
                      (int) to * to_percent + 50) / 100);
}

static ThemeColor blend_toward(const ThemeColor *from, const ThemeColor *to,
                               int from_percent)
{
    ThemeColor out = *from;

    out.fr = blend_channel(from->fr, to->fr, from_percent);
    out.fg = blend_channel(from->fg, to->fg, from_percent);
    out.fb = blend_channel(from->fb, to->fb, from_percent);
    out.br = to->br;
    out.bg = to->bg;
    out.bb = to->bb;
    out.bold = false;
    return out;
}

static ThemeColor quiet_doc_base(const ThemePalette *palette)
{
    ThemeColor background = palette->editor;

    background.fr = background.br;
    background.fg = background.bg;
    background.fb = background.bb;
    return blend_toward(&palette->muted, &background, 58);
}

static void theme_color_format_doc(const ThemePalette *palette,
                                   const ThemeColor *color,
                                   char *buf,
                                   size_t bufsize)
{
    ThemeColor doc_base;
    ThemeColor quiet;

    if (strcmp(palette->name, "monokai-dark") == 0) {
        theme_color_format(color, buf, bufsize);
        return;
    }

    doc_base = quiet_doc_base(palette);
    quiet = blend_toward(color, &doc_base, 22);
    theme_color_format(&quiet, buf, bufsize);
}

static ThemeColor line_tint_bg(const ThemeColor *base, const ThemeColor *accent,
                               int base_percent)
{
    ThemeColor out = *base;

    out.br = blend_channel(base->br, accent->fr, base_percent);
    out.bg = blend_channel(base->bg, accent->fg, base_percent);
    out.bb = blend_channel(base->bb, accent->fb, base_percent);
    return out;
}

static ThemeColor diff_bar_color(const ThemeColor *accent)
{
    ThemeColor out = *accent;

    out.bold = false;
    return out;
}

static ThemeColor menu_active_from_header(const ThemeColor *header)
{
    ThemeColor out;

    out.br = blend_channel(header->br, header->fr, 32);
    out.bg = blend_channel(header->bg, header->fg, 32);
    out.bb = blend_channel(header->bb, header->fb, 32);
    out.fr = header->br;
    out.fg = header->bg;
    out.fb = header->bb;
    out.bold = false;
    return out;
}

static ThemeColor menu_section_from_active(const ThemeColor *active)
{
    ThemeColor out = *active;
    uint8_t darker_br = active->br > 24 ? (uint8_t) (active->br - 24) : 0;
    uint8_t darker_bg = active->bg > 24 ? (uint8_t) (active->bg - 24) : 0;
    uint8_t darker_bb = active->bb > 24 ? (uint8_t) (active->bb - 24) : 0;

    out.br = blend_channel(active->br, darker_br, 72);
    out.bg = blend_channel(active->bg, darker_bg, 72);
    out.bb = blend_channel(active->bb, darker_bb, 72);
    out.fr = blend_channel(active->fr, out.br, 48);
    out.fg = blend_channel(active->fg, out.bg, 48);
    out.fb = blend_channel(active->fb, out.bb, 48);
    out.bold = false;
    return out;
}

static ThemeColor editor_stripe_bg(const ThemeColor *editor)
{
    ThemeColor out = *editor;
    int lum = ((int) editor->br + (int) editor->bg + (int) editor->bb) / 3;
    ThemeColor shift = *editor;

    if (lum >= 140) {
        shift.br = editor->br > 18 ? (uint8_t) (editor->br - 18) : 0;
        shift.bg = editor->bg > 18 ? (uint8_t) (editor->bg - 18) : 0;
        shift.bb = editor->bb > 18 ? (uint8_t) (editor->bb - 18) : 0;
    } else {
        shift.br = editor->br < 237 ? (uint8_t) (editor->br + 18) : 255;
        shift.bg = editor->bg < 237 ? (uint8_t) (editor->bg + 18) : 255;
        shift.bb = editor->bb < 237 ? (uint8_t) (editor->bb + 18) : 255;
    }
    out.br = blend_channel(editor->br, shift.br, 85);
    out.bg = blend_channel(editor->bg, shift.bg, 85);
    out.bb = blend_channel(editor->bb, shift.bb, 85);
    return out;
}

static void theme_color_on_bg(const ThemeColor *fg, const ThemeColor *bg,
                                char *buf, size_t bufsize)
{
    ThemeColor merged = *fg;

    merged.br = bg->br;
    merged.bg = bg->bg;
    merged.bb = bg->bb;
    theme_color_format(&merged, buf, bufsize);
}

static void theme_color_format_doc_on_bg(const ThemePalette *palette,
                                         const ThemeColor *color,
                                         const ThemeColor *bg,
                                         char *buf, size_t bufsize)
{
    ThemeColor doc_base;
    ThemeColor quiet;

    if (strcmp(palette->name, "monokai-dark") == 0) {
        theme_color_on_bg(color, bg, buf, bufsize);
        return;
    }

    doc_base = quiet_doc_base(palette);
    doc_base.br = bg->br;
    doc_base.bg = bg->bg;
    doc_base.bb = bg->bb;
    quiet = blend_toward(color, &doc_base, 22);
    quiet.br = bg->br;
    quiet.bg = bg->bg;
    quiet.bb = bg->bb;
    theme_color_format(&quiet, buf, bufsize);
}

static void build_stripe_theme(const ThemePalette *palette,
                               const ThemeColor *stripe_bg,
                               EditorTheme *out)
{
    theme_color_on_bg(&palette->editor, stripe_bg, out->stripe.editor,
                      sizeof(out->stripe.editor));
    theme_color_on_bg(&palette->keyword, stripe_bg, out->stripe.keyword,
                      sizeof(out->stripe.keyword));
    theme_color_on_bg(&palette->name_color, stripe_bg, out->stripe.name_color,
                      sizeof(out->stripe.name_color));
    theme_color_on_bg(&palette->punctuation, stripe_bg,
                      out->stripe.punctuation,
                      sizeof(out->stripe.punctuation));
    theme_color_on_bg(&palette->builtin, stripe_bg, out->stripe.builtin,
                      sizeof(out->stripe.builtin));
    theme_color_on_bg(&palette->string_color, stripe_bg,
                      out->stripe.string_color,
                      sizeof(out->stripe.string_color));
    theme_color_on_bg(&palette->ghost, stripe_bg, out->stripe.ghost,
                      sizeof(out->stripe.ghost));
    theme_color_format_doc_on_bg(palette, &palette->doc_muted, stripe_bg,
                                 out->stripe.doc_muted,
                                 sizeof(out->stripe.doc_muted));
    theme_color_format_doc_on_bg(palette, &palette->builtin_muted, stripe_bg,
                                 out->stripe.builtin_muted,
                                 sizeof(out->stripe.builtin_muted));
    theme_color_format_doc_on_bg(palette, &palette->string_muted, stripe_bg,
                                 out->stripe.string_muted,
                                 sizeof(out->stripe.string_muted));
    theme_color_format_doc_on_bg(palette, &palette->punct_muted, stripe_bg,
                                 out->stripe.punct_muted,
                                 sizeof(out->stripe.punct_muted));
}

static void theme_build(const ThemePalette *palette, EditorTheme *out)
{
    out->name = palette->name;
    theme_color_format(&palette->editor, out->editor, sizeof(out->editor));
    theme_color_format(&palette->editor_active, out->editor_active,
                       sizeof(out->editor_active));
    theme_color_format(&palette->keyword, out->keyword, sizeof(out->keyword));
    theme_color_format(&palette->keyword_active, out->keyword_active,
                       sizeof(out->keyword_active));
    theme_color_format(&palette->name_color, out->name_color,
                       sizeof(out->name_color));
    theme_color_format(&palette->name_active, out->name_active,
                       sizeof(out->name_active));
    theme_color_format(&palette->punctuation, out->punctuation,
                       sizeof(out->punctuation));
    theme_color_format(&palette->punctuation_active, out->punctuation_active,
                       sizeof(out->punctuation_active));
    theme_color_format(&palette->builtin, out->builtin, sizeof(out->builtin));
    theme_color_format(&palette->builtin_active, out->builtin_active,
                       sizeof(out->builtin_active));
    theme_color_format(&palette->string_color, out->string_color,
                       sizeof(out->string_color));
    theme_color_format(&palette->string_active, out->string_active,
                       sizeof(out->string_active));
    theme_color_format(&palette->muted, out->muted, sizeof(out->muted));
    theme_color_format(&palette->header, out->header, sizeof(out->header));
    theme_color_format(&palette->status, out->status, sizeof(out->status));
    theme_color_format(&palette->gutter, out->gutter, sizeof(out->gutter));
    theme_color_format(&palette->ghost, out->ghost, sizeof(out->ghost));
    theme_color_format(&palette->ghost_active, out->ghost_active,
                       sizeof(out->ghost_active));
    theme_color_format_doc(palette, &palette->doc_muted, out->doc_muted,
                           sizeof(out->doc_muted));
    theme_color_format_doc(palette, &palette->builtin_muted,
                           out->builtin_muted, sizeof(out->builtin_muted));
    theme_color_format_doc(palette, &palette->string_muted,
                           out->string_muted, sizeof(out->string_muted));
    theme_color_format_doc(palette, &palette->punct_muted,
                           out->punct_muted, sizeof(out->punct_muted));
    theme_color_format(&palette->gutter_active_row, out->gutter_active_row,
                       sizeof(out->gutter_active_row));
    ThemeColor menu_active;
    ThemeColor menu_section;
    ThemeColor stripe_bg;
    ThemeColor error_line;
    ThemeColor warning_line;

    menu_active = menu_active_from_header(&palette->header);
    theme_color_format(&menu_active, out->menu_active, sizeof(out->menu_active));
    menu_section = menu_section_from_active(&menu_active);
    theme_color_format(&menu_section, out->menu_section,
                       sizeof(out->menu_section));
    stripe_bg = editor_stripe_bg(&palette->editor);
    error_line = line_tint_bg(&palette->editor, &palette->keyword, 58);
    warning_line = line_tint_bg(&palette->editor, &palette->string_color, 58);
    theme_color_on_bg(&palette->gutter, &palette->editor, out->gutter_line,
                      sizeof(out->gutter_line));
    theme_color_on_bg(&palette->gutter, &stripe_bg, out->gutter_line_stripe,
                      sizeof(out->gutter_line_stripe));
    theme_color_on_bg(&palette->gutter, &palette->editor_active,
                      out->gutter_line_active,
                      sizeof(out->gutter_line_active));
    {
        ThemeColor error_gutter_fg = palette->gutter;
        ThemeColor warning_gutter_fg = palette->gutter;

        error_gutter_fg.fr = palette->keyword.fr;
        error_gutter_fg.fg = palette->keyword.fg;
        error_gutter_fg.fb = palette->keyword.fb;
        warning_gutter_fg.fr = palette->string_color.fr;
        warning_gutter_fg.fg = palette->string_color.fg;
        warning_gutter_fg.fb = palette->string_color.fb;
        theme_color_on_bg(&error_gutter_fg, &error_line, out->error_gutter,
                          sizeof(out->error_gutter));
        theme_color_on_bg(&warning_gutter_fg, &warning_line,
                          out->warning_gutter, sizeof(out->warning_gutter));
    }
    theme_color_format(&error_line, out->error_line, sizeof(out->error_line));
    theme_color_format(&warning_line, out->warning_line,
                       sizeof(out->warning_line));
    {
        ThemeColor diff_added;
        ThemeColor diff_modified;
        ThemeColor diff_deleted;

        diff_added = diff_bar_color(&palette->name_color);
        diff_modified = diff_bar_color(&palette->string_color);
        diff_deleted = diff_bar_color(&palette->keyword);
        theme_color_fg(&diff_added, out->diff_added, sizeof(out->diff_added));
        theme_color_fg(&diff_modified, out->diff_modified,
                       sizeof(out->diff_modified));
        theme_color_fg(&diff_deleted, out->diff_deleted,
                       sizeof(out->diff_deleted));
    }
    {
        theme_color_format(&palette->header, out->sidebar, sizeof(out->sidebar));
        theme_color_format(&menu_active, out->sidebar_active,
                           sizeof(out->sidebar_active));
    }
    build_stripe_theme(palette, &stripe_bg, out);
}

void themes_init(void)
{
    if (ready) {
        return;
    }
    for (size_t i = 0; i < sizeof(palettes) / sizeof(palettes[0]); i++) {
        theme_build(palettes[i], &built[i]);
    }
    ready = true;
}

size_t theme_count(void)
{
    return sizeof(palettes) / sizeof(palettes[0]);
}

const EditorTheme *theme_get(size_t index)
{
    if (!ready) {
        themes_init();
    }
    if (index >= theme_count()) {
        return &built[0];
    }
    return &built[index];
}

size_t theme_find_index(const char *name)
{
    if (!name || !name[0]) {
        return SIZE_MAX;
    }
    if (!ready) {
        themes_init();
    }
    if (strcmp(name, "monokai") == 0) {
        name = "monokai-dark";
    }
    if (strcmp(name, "ubuntu") == 0) {
        name = "ubuntu-dark";
    }
    for (size_t i = 0; i < theme_count(); i++) {
        if (strcmp(built[i].name, name) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}
