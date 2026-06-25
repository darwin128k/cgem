# Changelog

All notable changes to CGEM are documented in this file.

## 0.2.0 - 2026-06-25

### Added

- Live semantic analysis in the IDE: symbol table adoption from compile,
  debounced re-analysis (~400 ms), and diagnostics without a full generate.
- Lint rules E001–E003 for unknown references and related issues during
  analysis.
- `SymbolKind` and `type_dsl_name` on semantic symbols; completion uses the
  symbol table.
- Type-check pass (`typecheck.c`): E101–E102 for param, field, return, and
  `let` type references against known symbols.
- Workspace definition index (`semantic_nav.c`): go-to-definition, rename,
  cross-file navigation, and merged workspace symbols.
- IDE key bindings: `Ctrl+D` go to definition, `Ctrl+P` rename, `Ctrl+N` find
  next; `F1` help, `F2`–`F6` menus; `Ctrl+O` open file.
- Inline `param` in struct method assignments — declare a typed parameter at
  the point of use (`self.field = param …`, `self.field =? param …`,
  `@pointer @mutable param … ?= rhs`); lowers to the same C signature and body
  as separate `param` lines.

### Changed

- `lh.version` `pack`, `unpack`, and `set_*` in `main.cgem` use inline `param`
  syntax; `unpack` writes through optional out-pointers with `?= self.field`.
- README documents CGEM direction, inline `param`, and updated IDE shortcuts.

### Fixed

- Segfault when optional parser out-parameters were `NULL` (`cg_parse_case`,
  `cg_parse_type`, `cg_parse_let`) during IDE analysis.
- `semantic_nav.c` definition indexing after a corrupted `cg_parse_fn` guard.

## 0.1.0 - 2026-06-24

### Added

- `case name = expr` inside `enum` for explicit enumerator values; implicit
  cases follow C enum auto-increment rules.
- Enum case expressions accept references to earlier enumerators in the same
  enum (for example `case open = lh.interval.flags.lopen | lh.interval.flags.ropen`)
  and evaluate full C constant-integer expressions with standard operator
  precedence: arithmetic (`+`, `-`, `*`, `/`, `%`), shifts (`<<`, `>>`),
  comparisons (`<`, `>`, `<=`, `>=`, `==`, `!=`), bitwise (`&`, `|`, `^`,
  `~`), logical (`&&`, `||`, `!`), unary `+`/`-`, and parentheses.
- `@initializer` flag on struct methods marks the single constructor function
  and registers `lh.module(...)` call syntax in expressions.
- Added a dedicated color strip to the right of the IDE line-number gutter for
  saved-file differences (added, modified, and deleted lines); it resets after
  saving.
- Added terminal previews for uncompressed 24-bit and 32-bit BMP files opened
  through the IDE, with aspect-preserving scaling and ANSI color fallback.
- Added IDE **Open** through the **File** menu and `Ctrl+O`, with relative and
  absolute path support, unsaved-change confirmation, and non-destructive error
  handling.
- Added IDE **Save As** through the **File** menu; after a successful save the
  selected path becomes the current file for subsequent saves.
- Added **Increase** and **Decrease** to the IDE **Edit** menu with `Tab` and
  `Shift+Tab` labels, grouped under section headers (History, Clipboard,
  Section, Search); **File**, **Build**, and **Options** use the same style.
- IDE dropdown menus show non-selectable section titles (`menu_section`) on a
  darker band derived from the header palette (ANSI truecolor, no Unicode rules).
- Added a bounded per-document edit history with undo/redo, a text-search
  prompt with wrapped next-match navigation, and a go-to-line prompt.
- Added **Format** to the IDE **Edit** menu (`Ctrl+K` by default), reusing
  the DSL formatter to normalize indentation and trim trailing whitespace.
- Added IDE menu bar and dropdown menus (`File`, `Edit`, `Build`, `Options`,
  `Help`) with mouse, arrow-key, Enter/Esc, and hotkey labels on menu items.
- Added configurable IDE key bindings via `keymap/default.keymap` and optional
  overrides in `.cgem/keymap` (`menu.file F2`, `action.save Ctrl+S`, and similar
  entries).
- Added `menu_active` theme color derived from the header palette for menu
  selection highlighting.
- Added `stripe` editor colors for subtle alternating row backgrounds in the IDE.
- Added field-macro invocation inside `struct module:` bodies (for example
  `lh.version.fields(lh.version.major, lh.version.minor, lh.version.patch)`),
  with preprocessor expansion by default and inline field expansion via `@expand`.
- Added `lh.version` struct generation from `main.cgem`, matching the `lh 2`
  versioning layout (`major`, `minor`, `patch` modules and a `fields` template).
- Added IDE syntax highlighting for dotted references on unconstrained lines
  (struct macro calls and similar expressions).
- Added scope-aware IDE ghost completion (`version.` → `lh.version.` inside the
  `lh` package) and indexer support for package/module child relationships and
  parenthesized macro calls.
- Added IDE mouse clipboard shortcuts: right-click copies the selection (or
  pastes when nothing is selected), middle-click pastes.
- Added generated-package cleanup before generation, with an explicit
  `--clean-output false` opt-out.
- Added `if`, `elif`, and `else` blocks inside modules for conditional C
  output (`#if`, `#elif`, `#else`, `#endif`). `@define` before `if:` applies
  to declarations in every branch of the chain.
- Added inline `@doc "…"` and `@include "…"` attributes for a single quoted
  string; multi-string values still use the `@doc:` and `@include:` block forms.
- Added `return c.initializer(...)` for C initializer lists; variadic parameters
  become `__VA_ARGS__`.
- Added module-level `fn name:` functions without parameters. Functions without
  `return` emit `void`; return values require `fn name as type:` or
  `return expression as type`, which emits a C cast.
- Added local `let` inside functions, with `const` by default, `@mutable`,
  unused-local errors, and `@used` for intentional unused locals.
- Added variadic template parameters with `param name as ...`; only one `...`
  is allowed per template and it must be last. Generated macros use C99 `...`
  and `__VA_ARGS__`.
- Added `-f`/`--format` to format `.cgem` files in place (trailing whitespace,
  blank lines, and leading indentation aligned to four spaces).
- Added `@include:` to emit explicit `#include` directives in generated headers
  or sources.
- Added module-level `let` bindings with `let name as type = value` syntax,
  const by default, with `@mutable`, `@public`, `@private`, `@extern`, and
  `@define` support.
- Added `let` inside `enum` blocks; `case` now lowers through the same binding
  path with auto-incremented values.
- Added IDE highlighting for diagnostic lines after generate, with per-theme
  error and warning colors in the editor and gutter.
- Added `@internal` support for types, packages, and modules, with cascading
  suppression of C generation and `@public` opt-out on children.
- Added a `packages/` directory at the project root for optional package inputs.
- Added `CGEM_IDE` CMake option to build CGEM without the terminal IDE and theme
  sources.
- Split the compiler into separate modules for diagnostics, parsing, symbols,
  code generation, and compile utilities.

### Changed

- Removed `let` inside `enum`; use `case` or `case name = expr` instead (no
  backward compatibility).
- `@initializer` is a flag attribute only; `@initializer(...)` is no longer
  supported (no backward compatibility).
- Added struct method call blocks (`self.method:` + indented `param` lines) as
  sugar for declaring parameters and invoking the method in one form.
- Added support for **multiple consecutive struct method call blocks** inside a
  single struct method body. Each `self.method:` line followed by indented
  `param` lines now generates an independent method call statement, enabling
  composable DSL wrappers such as:

  ```text
  @mutable
  fn set:
      self.set_major:
          param major as lh.version.major
      self.set_minor:
          param minor as lh.version.minor
      self.set_patch:
          param patch as lh.version.patch
  ```

  This lowers to three independent `lh_version_set_minor(self, minor);`-style
  statements inside the generated C function.
- Replaced inline `@doc "..."` and `@include "..."` with parenthesis form only
  (`@doc("...")`, `@include("...")`; no backward compatibility).
- Replaced inline `c.ptr.of(...)` pointer syntax with the `@pointer` attribute
  on `type`, `field`, `let`, `param`, and `fn ... as type` declarations.
- Consolidated the 11-line attribute-reset sequence duplicated across 9 sites
  in `compiler.c` into a single `reset_compiler_attributes()` helper, removing
  ~100 lines of mechanical duplication without changing any generated output.
- `write_source_function()` in `codegen.c` now checks the return value of
  every `fprintf` call and reports a write failure via `cg_set_error()` instead
  of silently truncating the generated source on disk-full or I/O errors.
- Temporarily removed the IDE file explorer (`Ctrl+E`); source remains in
  `src/cgem/ide_file_tree.c` until Unicode path display is ready.
- Reorganized project files: CGEM implementation sources now live in
  `src/cgem`, project headers in `include/cgem`, and the CMake target uses
  `include` as its sole include root.
- Changed generated-output cleanup to remove only root packages declared by
  the input (for example `include/lh` and `src/lh`) instead of clearing entire
  output roots. Cleanup is now enabled by default, preserves unrelated manual
  packages such as `cgem`, and can be disabled with `--clean-output false`.
- Source package directories are now created lazily only when a module emits a
  `.c` implementation, avoiding empty directory trees for header-only modules.
- Documented that `F10` should not be used in custom IDE keymaps: Windows
  consoles (including XP) reserve it for the system menu.
- IDE **Format** default shortcut moved from `Ctrl+O` to `Ctrl+K`; `Ctrl+O` is
  now assigned to **Open**.
- IDE shortcuts now follow common editor conventions: `Ctrl+F`/`F7` for
  search, `Ctrl+G` for line navigation, `Ctrl+Z`/`Ctrl+Y` for undo/redo,
  `Ctrl+A` for select all, `Ctrl+B` for generation,
  `F2`–`F6` for menus, and compile will reuse `Ctrl+B` when added.
- Redesigned IDE chrome into a two-line header: centered `CGEM` title on row 1
  and `File Edit Build Options Help` plus the filename on row 2; the status bar
  no longer shows the old `F2 Save F3 Theme` shortcut strip.
- IDE dropdown menus now use the header palette (no ASCII `+`/`-` frame) with
  theme-aware `menu_active` highlighting that stays distinct from the active
  editor line.
- IDE menu items show configurable hotkey labels (for example `Ctrl+S`) on the
  right; function keys open top-level menus instead of being listed inside
  submenu entries.
- Moved **Theme** to **Options** and made **Help** a separate top-level menu
  item; removed the long keyboard-shortcut hint from the status bar.
- IDE theme selection is now a dedicated picker (**Options → Theme**):
  browse with `←`/`→`, apply with Enter, cancel with Esc; the status bar shows
  `Theme selected: …` with readable names (for example `Windows XP`, `CMD`).
- IDE status bar now uses the header palette, matching the title bar for a
  consistent top/bottom chrome layout.
- IDE line-number gutter uses the header palette with symmetric padding on both
  sides of the digits; removed the vertical bar between gutter and editor.
- IDE editor rows use subtle alternating stripe backgrounds derived from each
  theme; active and diagnostic rows keep their own highlighting.
- IDE startup now switches the terminal to the alternate screen buffer and draws
  each screen row with explicit CSI positioning.
- Removed `c.type` without compatibility aliases. Fundamental C types are now
  built in as `c.void`, `c.char`, `c.uchar`, `c.int`, `c.float`, and the rest
  of the standard signed, unsigned, and floating-point family.
- Generation and IDE startup now require `-c`/`--compiler`; its predefined
  macros are exposed through `c.compiler.*`.
- Untyped `let` now emits a `#define`, permits an omitted value, accepts a
  redundant `@define`, and classifies integer, floating-point, and string
  values.
- Moved `param` declarations inside `struct` and `fn` bodies and removed
  `@template` entirely (no backward compatibility).
- Added typed function parameters; concrete parameter types emit C functions,
  while bare `param` names and `param name as ...` infer a function-like macro.
- Replaced `param name as type` with bare `param name` metaparameters and
  optional `@require type`, `@require value`, or `@require type or value`
  (no backward compatibility).
- Removed inline `c.type` and block-form `c.ptr.of:`; pointer types use
  `c.ptr.of(...)` (no backward compatibility).
- Replaced `c.expr`, `raw`, and placeholder interpolation with semantic
  built-in C types and the `c.initializer(...)` call (no backward compatibility).
- Replaced the `template:` keyword block with `@template:` block attribute
  syntax (no backward compatibility).
- Replaced template parameter syntax with bare `param name`,
  `param name as ...` for variadics, and optional `@require` constraints
  (no backward compatibility).
- Replaced parenthesized `@doc("…")` and `@include("…")` attributes with block
  syntax: `@doc:` and `@include:` followed by one or more indented quoted
  strings (no backward compatibility).
- Removed `return callee:` call blocks, the `use` keyword, and `@ref` for call
  arguments; calls use inline `callee(args)` syntax (no backward compatibility).
- Unified inline parenthesis syntax for `c.ptr.of(...)`, `c.initializer(...)`,
  and `return callee(...)`.
- Replaced `@canonical` with primary export syntax using the reserved name
  `module`: `type module as`, `enum module as`, `let module as`, and
  `struct module:` (no backward compatibility).
- Changed `c.expr` template placeholders from `${name}` to `{name}`.
- Extended `cgem_compile` with an optional `DiagnosticList` output for IDE and
  tooling integration.
- Extracted IDE mode into `ide.c` and shared CLI helpers into `common.c`.
- Refactored `compiler.c` into a smaller orchestration layer over the new
  compiler modules.

### Fixed

- Fixed primary-export `enum module as` members registering under a duplicated
  module segment in the DSL path (`lh.interval.flags.flags.lopen` instead of
  `lh.interval.flags.lopen`), which broke enumerator references inside case
  expressions.
- Fixed IDE scroll leaving stale background fragments between the editor area
  and the status bar on Windows and Linux.
- Fixed IDE copy crashing on large selections due to an undersized
  `selected_text` buffer, and added `wl-copy`/`xclip` fallback when OSC 52
  clipboard is unavailable.
- Fixed IDE caret overlapping the menu popup on the first editor line (including
  sticky `package` headers) by keeping the terminal cursor hidden while a menu
  is open.
- Fixed `cg_parse_type()` in `parse.c` not initializing `*reference` before the
  `fail` label, which could free an uninitialized pointer when callers did not
  pre-initialize it. All other parsers in the file (`cg_parse_param`,
  `cg_parse_field`, `cg_parse_let`, `cg_parse_fn`, `cg_parse_paren_call`,
  `cg_parse_as_field_type`) were verified to initialize their outputs correctly.
- Fixed IDE syntax highlighting leaving struct method names uncolored in
  block-form self-calls (`self.method:` followed by indented `param` lines).
  The IDE index now records `self.<method>` entries for every `fn` declared
  inside a `struct` body, mirroring the existing `self.<field>` treatment, so
  `dotted_ref_highlightable()` recognizes the method name as a known reference
  and the highlighter applies the same color used for `self.field` reads.
- Fixed compiler diagnostics missing line numbers for unknown struct type
  references, so IDE error highlighting can jump to the correct row after
  generate.
- Fixed IDE ghost completion and inline hints before `)` in dotted macro calls.
- Fixed IDE cursor drifting down a line after `Delete`.
- Fixed IDE scroll-wheel panning fighting the cursor on sticky header rows;
  manual scrolling now disables follow-cursor until the cursor moves again.
- Fixed IDE chrome rendering on Linux raw terminals: the title bar and menu no
  longer overwrite each other when `OPOST` is disabled.
- Fixed `clang-format` not running when generation starts outside the directory
  that contains `.clang-format` (for example from `build/`).
- Fixed IDE indexer DSL paths for nested modules (for example
  `lh.version.fields` instead of `lh.version.fields.fields`).
- Fixed IDE ghost completion token extraction inside `(` and `,` so partial
  dotted names complete correctly in macro argument lists.
- Fixed IDE dotted-reference highlighting treating arbitrary text in macro calls
  as valid symbols; references are now checked against declared scope children
  and indexed definitions only.
- Fixed macro call arguments being indexed as definitions, which made typos such
  as `lh.version.path` appear valid in syntax highlighting and ghost text.
- Fixed ghost completion suggesting a trailing character when the typed prefix
  is one character short of a different symbol (for example `path` completing to
  `patch`).
- Fixed IDE right-click on a selection clearing the highlight without copying;
  Linux mouse parsing now recognizes the right and middle buttons instead of
  treating right-click as Escape.
- Fixed IDE **Format** corrupting buffer lines and crashing on heap errors; the
  screen now refreshes immediately after formatting.
- Fixed IDE theme picker status text staying in selection mode after Enter;
  confirmation now shows `Theme selected: …` with a formatted theme name.
- Fixed IDE explorer and gutter colors: gutter has no separate chrome background
  (line numbers sit on the editor row).

## 2026-06-16

### Added

- Added mouse wheel scrolling in IDE mode.
- Added mouse support in IDE mode, including click, drag, and release handling.
- Added pointer expansion support with `@expand` and `c.ptr` based pointer type handling.
- Added line duplication in the editor.
- Added muted documentation-line rendering and theme color blending helpers.
- Added Windows XP cross-compilation support with MinGW i686 helper scripts and a CMake toolchain file.
- Added additional editor themes, including CMD, Monokai Light, Xcode, Windows, Ubuntu, Android Holo, and other light/dark variants.
- Added workspace file handling for remembering editor state.
- Added generated documentation support through repeated `@doc` attributes and package README generation.
- Added reusable structure field templates, mutable fields, enums, visibility attributes, macro generation with `@define`, and fixed-width numeric definitions.
- Added CMake build support and cross-platform terminal backends for Linux, modern Windows terminals, and legacy Windows consoles.

### Changed

- Renamed and standardized DSL attributes and declarations, including `alias` to `type`, `@brief` to `@doc`, and `@flat`/`@transparent` to `@noscope`.
- Refactored pointer and void type definitions for clearer generated C output.
- Refactored terminal output and mode handling to share more behavior across platforms.
- Simplified the Windows XP build flow to use the regular `build` directory.
- Improved editor parsing for dotted references, declarations, fields, parameters, and identifiers.
- Improved generated C formatting by integrating `clang-format` when available.

### Removed

- Removed obsolete test targets and the old argument validation script.
- Removed deprecated tracked workspace settings from `.cgem`.
- Removed deprecated Windows theme color handling paths after terminal rendering was simplified.
