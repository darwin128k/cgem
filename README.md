<div align="center">
<img src="assets/gem-logo.svg" alt="CGEM" width="50%" vspace="12"><br><br>
<a href="#linux"><img src="https://img.shields.io/badge/Linux-supported-009688?style=for-the-badge&logo=linux&logoColor=white" alt="Linux"></a><a href="#windows-1011"><img src="https://img.shields.io/badge/Windows%2010%2F11-supported-0078D6?style=for-the-badge&logo=windows&logoColor=white" alt="Windows 10/11"></a><a href="#windows-xp"><img src="https://img.shields.io/badge/Windows%20XP-supported-3C8D0D?style=for-the-badge&logo=windows&logoColor=white" alt="Windows XP"></a>
</div>

## About

CGEM is a DSL that transpiles to C, bundled with a small DOS-style terminal
IDE written in C. It runs on Linux and Windows 10/11 terminals. Windows XP
is supported through the legacy Console API fallback when built with MinGW.

## Direction

CGEM is not trying to replace C, fix memory safety, or hide the platform from
you. It is not Rust, not Zig, and not a framework on top of C — the developer
stays in charge; CGEM only makes some chores cheaper.

The goal is **fast, immediate description of C entities**: types, macros,
modules, and functions — with names, scopes, and structure kept in the DSL
instead of scattered across headers and implementation files.

That does not mean a dumb text transformer. CGEM is a real language with its
own semantics: symbol tables, scoped paths, enum constant evaluation, template
field macros, IDE analysis, and diagnostics. The "brains" live where they are
cheap — naming, lookup, and codegen — not in a runtime or borrow checker.

CGEM does not chase novelty for its own sake. Indentation replaces braces
because declarations read better when structure is visible. Parameters can live
**inside** the declaration they belong to (`field x as param T`, inline
`param` at the point of use) because a long-standing pain is not only writing
C, but **writing declarations while you are still shaping the body**.

Function parameters illustrate this. A linear `param` block at the top of `fn`
is the familiar style. A `self.method:` call block declares parameters where
the call happens. Inline `param` in an assignment (`self.major =? param major as …`)
declares a parameter exactly where it is consumed. All three lower to the same
C signature; the DSL only changes *when* you decide the shape.

C remains the engine. CGEM is the descriptive layer: describe the entity once,
in context, and generate ordinary C you can read, diff, and ship.

## Build

### Linux

```sh
cmake -B build
cmake --build build
./build/cgem -g -c cc -i program.cgem -I include -s src --ide
```

### Windows 10/11

Build with MinGW-w64:

```sh
cmake -B build -G "MinGW Makefiles"
cmake --build build
build\cgem.exe -g -c gcc -i program.cgem -I include -s src --ide
```

On Windows 10/11 the editor uses Virtual Terminal sequences when the console
supports them. This gives the IDE the same ANSI true-color rendering path used
on Linux in Windows Terminal, PowerShell, and modern `cmd.exe`.

### Windows XP

Build with MinGW-w64 32-bit and the XP API target:

```sh
cmake -B build -G "MinGW Makefiles" -DCMAKE_C_FLAGS="-m32 -D_WIN32_WINNT=0x0501"
cmake --build build
build\cgem.exe -g -c i686-w64-mingw32-gcc -i program.cgem -I include -s src --ide
```

On Windows XP the editor falls back to the classic Console API, so it works in
old `cmd.exe` without Virtual Terminal support.

### Continuous integration

Pushes and pull requests to `master` trigger [GitHub Actions](.github/workflows/build.yml).
Each configuration is cross-built on Ubuntu and published as a workflow artifact
(`cgem-<target>-<arch>`):

| Artifact | Platform |
|----------|----------|
| `cgem-linux-x86_64` | Linux x86_64 |
| `cgem-windows-xp-i686.exe` | Windows XP 32-bit (`_WIN32_WINNT=0x0501`) |
| `cgem-windows-xp-x86_64.exe` | Windows XP 64-bit (`0x0502`) |
| `cgem-windows-vista-*.exe` | Windows Vista (`0x0600`) |
| `cgem-windows-7-*.exe` | Windows 7 (`0x0601`) |
| `cgem-windows-8-*.exe` | Windows 8 (`0x0602`) |
| `cgem-windows-10-*.exe` | Windows 10 (`0x0A00`) |

Local smoke build (same entry point as CI):

```sh
./scripts/ci-build.sh linux x86_64
./scripts/ci-build.sh windows-xp i686 0x0501
./scripts/ci-build.sh windows-10 x86_64 0x0A00
```

Windows targets need `gcc-mingw-w64-i686` and `gcc-mingw-w64-x86-64` (installed
automatically in CI). Outputs land in `dist/`.

`-g`/`--generate` enables generation. It requires at least one input, an
include destination, and a source destination. `-I`/`--include` and
`-s`/`--source` must point at **generated output** directories (for example
`include/` and `source/`), not at the CGEM compiler sources in `src/`.
Generation removes only the package directories declared at the root of the
input (for example `include/lh` and `source/lh`) before recreating them. Other
contents of the output roots are preserved. This cleanup is on by default and
can be disabled with `--clean-output false`:

```sh
./build/cgem --generate --input program.cgem \
    --include generated/include --source generated/src --compiler cc
./build/cgem -g -i program.cgem -I include -s source -c gcc --clean-output true
```

`-c`/`--compiler` is required for generation and IDE sessions; pure formatting
does not require it. CGEM asks that compiler for its predefined macros and
makes every object-like and function-like macro available under
`c.compiler.*`. For example, `c.compiler.__CHAR_BIT__` lowers to
`__CHAR_BIT__`, and `c.compiler.__GNUC__` lowers to `__GNUC__`. This keeps the
generated code tied to the actual target compiler rather than CGEM's host
assumptions.

When `.clang-format` (or `_clang-format`) exists in the working directory,
an ancestor directory, or next to the include or source paths, and
`clang-format` is available in `PATH`, generated C headers and sources are
formatted automatically. Generation continues without formatting when either
is absent.

`-f`/`--format` formats `.cgem` input files in place: it trims trailing
whitespace, turns whitespace-only lines into blank lines, and snaps leading
indentation to groups of four spaces. Combine with `--generate` to format
inputs before compilation.

In IDE mode, the buffer is formatted automatically on save and before generate.

Add `--ide` to start the interactive mini IDE:

```sh
./build/cgem -g -c cc -i program.cgem -I generated/include \
    -s generated/src --ide
```

`-i`/`--input` accepts a `.cgem` file or a directory. Generation mode allows
the option to be repeated and recursively reads `.cgem` files from
directories:

```sh
./build/cgem -g -c cc -i core.cgem -i packages/ -I include -s src
```

IDE mode accepts zero or one input. A file is opened directly; a directory
opens or creates its `main.cgem`. Without `--input`, the IDE opens
`./main.cgem` in the current working directory:

```sh
./build/cgem -g -c cc -I include -s src --ide
```

## Keys

- Arrow keys, `Home`, `End`, `Page Up`, `Page Down`: move
- `Enter`, `Delete`: edit
- `Backspace`: delete a character or a complete indentation step
- `Tab`: accept a visible keyword hint or indent the current line or selection
  by one block level (4 spaces); changes which DSL section a line belongs to
- `Shift+Tab`: unindent the current line or selection by one block level
  (4 spaces); also available as **Edit → Decrease**
- `Ctrl+S`: save
- `Ctrl+O`: open a file
- `Ctrl+B`: generate C output (compile will reuse this key when added)
- `F1`: help
- `F2`–`F6`: open `File`, `Edit`, `Build`, `Options`, and `Help` menus
- `F8`, `F9`, `F11`, and `F12` are free for custom bindings in
  `keymap/default.keymap` or `.cgem/keymap`; do **not** bind `F10` — on Windows
  consoles (including XP) it opens the system menu and may never reach the
  editor (rebind at your own risk; Linux terminals are usually fine)
- `Ctrl+C`, `Ctrl+X`, `Ctrl+V`, `Ctrl+A`: copy, cut, paste, select all
- `Ctrl+Z`, `Ctrl+Y`: undo and redo edits
- `Ctrl+F`, `F7`: find text; `Ctrl+N`: find the next match
- `Ctrl+G`: go to a line number
- `Ctrl+D`: go to definition
- `Ctrl+P`: rename symbol
- Format is available from **Edit → Format** or `Ctrl+K`
- **Edit → Increase** (`Tab`) and **Edit → Decrease** (`Shift+Tab`) move lines
  between DSL section levels (4-space steps); the **Edit** menu groups commands
  under section headers (History, Clipboard, Section, Search).
- Mouse wheel: scroll the buffer
- `Ctrl+Q`: quit

The editor uses standard ANSI terminal sequences and has no external
dependencies.

The editor uses a Monokai-style true-color theme and highlights language
declarations as they are typed.

In IDE mode, typing the beginning of a keyword such as `scope`, `module`, or
`package` shows its remaining letters as ghost text. Press `Tab` to accept the
keyword and insert a following space.

Blocks use exactly 4 spaces instead of braces. `Enter` preserves indentation
and adds one level after a line ending in `:`.

## Language

The language currently has three structural constructs and one built-in
package:

```text
package graphics:
    module image:
        scope color:
```

A scope reserves a prefix for declarations that will be added later. Nested
scopes combine their names.

A module is a named output unit. `module image:` will generate `image.h` in
the configured include path and `image.c` in the configured source path when
the generator is implemented.

A package creates an isolated namespace and matching subdirectories in both
configured output paths. In the example, the module will generate
`include/graphics/image.h` and `source/graphics/image.c`.

The built-in `c` package exposes the fundamental C types directly:

```text
c.void c.bool c.char c.schar c.uchar
c.short c.sshort c.ushort c.int c.sint c.uint
c.long c.slong c.ulong c.llong c.sllong c.ullong
c.float c.double c.ldouble
```

Types bind a DSL name to a raw C type:

```text
package lh:
    package numeric:
        module types:
            type uchar as c.uchar
```

This generates:

```c
typedef unsigned char lh_numeric_types_uchar_t;
```

Every DSL object can have any number of attributes. Attributes are written on
the lines immediately before the object at the same indentation:

```text
@public
@deprecated
type uchar as c.uchar
```

Attributes with values use a parenthesis call (`@doc("summary")`) or a block
after a colon. The inline call form accepts one string argument; the block form
accepts one or more indented quoted strings:

```text
@doc("summary line")

@include("<stdint.h>")

@doc:
    "summary line"
    "additional detail"

@include:
    "lh/void.h"
    "<stdint.h>"
```

Valued inline attributes such as `@doc(...)` and `@include(...)` use the same
call syntax as `c.initializer(...)`. Future attributes follow the same pattern.

Flag attributes stay on a single line with no colon or parentheses.

Attributes are parsed and attached to the following object. Their individual
semantics will be introduced as the corresponding features are designed.

`@doc` attaches documentation to the following object. The first string in the
block becomes the summary; later strings add detail. Context determines the
output:

```text
@doc:
    "The C void type and pointer aliases."
module void:
    @doc:
        "Unsigned byte"
        "Stores a value from 0 to 255."
    type byte as c.uchar

@doc:
    "C integer type aliases and fixed-width names."
package numeric:
    module types:
        ...
```

Before a `module`, documentation is emitted at the top of the generated
header. Before a `package`, it is written to `README.md` in the package
directory under the include path. Before declarations inside a module, it
becomes a Doxygen comment:

```c
/**
 * @brief The C void type and pointer aliases.
 */
#ifndef LH_VOID_H
...
/**
 * @brief Unsigned byte
 *
 * Stores a value from 0 to 255.
 */
typedef unsigned char byte_t;
```

`README.md`:

```markdown
C integer type aliases and fixed-width names.
```

Parameters live inside the declaration they parameterize. A bare `param name`
introduces a metaparameter that may stand for a type or a value depending on
where it is used. Optional `@require type`, `@require value`, or
`@require type or value` on the line above a parameter narrows that when
needed.

A `struct` with parameters generates a field macro:

```text
struct module:
    param first_type
    param second_type

    field first as first_type
    field second as second_type
```

Parameters may also be introduced on the same line as a field:

```text
@doc:
    "Half-open interval with typed lower and upper bounds."
struct module:
    @doc("Lower bound (inclusive).")
    field lower as param lower_type
    @doc("Upper bound (exclusive).")
    field upper as param upper_type
```

`@doc` before `struct module:` becomes the macro `@brief`. `@doc` before a
`field ... as param ...` line becomes that parameter's `@param` description in
the generated header comment; it does not document the struct field itself.
Explicit `param` lines accept `@doc` the same way.

`@doc` before a plain `field` line documents the struct member. In a typedef
struct the comment is emitted immediately before the member; in a field macro
it is emitted as a Doxygen `/**< ... */` suffix on the macro line.

A declaration may contain one variadic parameter with `param name as ...`.
It must be the last parameter in the block, and only one `...` is allowed
per declaration:

```text
struct module:
    param first_type
    param values as ...

    field first as first_type
    field rest as values
```

This generates a C99 variadic macro. The variadic slot is `...` in the
parameter list; references to the DSL name in fields expand to `__VA_ARGS__`:

```c
#define lh_example_fields(first_type, ...) \
    first_type first; \
    __VA_ARGS__ rest
```

`@noscope` keeps a container in the physical file structure but omits it
from both logical DSL paths and generated C symbols:

```text
package lh:
    @noscope
    package numeric:
        @noscope
        module types:
            type uchar as c.uchar
```

The logical DSL name becomes `lh.uchar`, while the generated C name is:

```c
typedef unsigned char lh_uchar_t;
```

Output directories and module filenames still use the complete DSL path.
In IDE mode, typing `@n` shows `@noscope` as ghost text; press `Tab` to
accept the attribute.

Each module may declare one primary export with the reserved name `module`.
The export name comes from the module itself:

```text
module void:
    type module as c.void
    @pointer
    type ptr as lh.void
```

The primary export gets the full DSL path (`lh.void.void`) and a short alias
at the module level (`lh.void`). Additional exports in the same module use an
explicit name before `as`:

```text
module bool:
    enum module as lh.byte:
        case false
        case true
```

The same rule applies to `let` (`let module as type = value`) and template
structures (`struct module:`).

`@define` emits a macro instead of a typedef:

```text
module void:
    @define
    type module as c.void
    @define
    @expand
    @pointer
    type ptr as lh.void
```

```c
#define lh_void void
#define lh_void_ptr void *
```

`c.initializer(...)` is an inline call that constructs a C initializer and
supplies the braces itself. Arguments are comma-separated expressions;
variadic parameters become `__VA_ARGS__`:

```text
fn module:
    @require type or value
    param values as ...
    return c.initializer(values)
```

The former `return c.initializer:` block form is not supported.

Function parameter kinds determine how a function is emitted. Parameters with
a concrete DSL type, such as `param value as lh.int`, produce an ordinary C
function parameter. A bare `param name` or `param name as ...` is a
metaparameter; if a function contains at least one of them, the whole function
is emitted as a C function-like macro. No separate template attribute is
required.

Use `@pointer` before a typed declaration to make the C type a pointer to the
referenced DSL type. It preserves the referenced C type symbol by default:

```text
@pointer
type ptr as lh.void    # #define lh_void_ptr lh_void *
```

Add `@expand` to emit the referenced type's expanded C expression:

```text
@expand
@pointer
type ptr as lh.void    # #define lh_void_ptr void *
```

`@pointer` applies to `type`, `field`, `let`, `param`, and `fn ... as type`
return declarations.

Conditional compilation uses `if`, `elif`, and `else` blocks inside a module.
Each branch is indented by four spaces; the chain ends when the indentation
returns to the module level (a `#endif` is emitted automatically). Conditions
are ordinary expressions; DSL names such as `c.compiler._WIN32` lower to their
C spellings:

```text
module platform:
    @define
    if c.compiler._WIN32:
        type win as c.void.ptr
    elif c.compiler.__linux__:
        @pointer
        type unix as lh.void
    else:
        @pointer
        type generic as lh.void
```

`@define` before `if:` applies to every `type` and typed `let` in all branches
of that chain. Without `@define`, branches emit `typedef` declarations instead.
Each branch is still parsed by CGEM, so use distinct names per branch when both
would register the same DSL symbol.

On an `enum`, `@define` keeps the typedef and turns case values into macros:

```text
module bool:
    @define
    enum module as lh.byte:
        case false
        case true
```

```c
typedef lh_byte_t lh_bool_t;

#define lh_bool_false 0
#define lh_bool_true 1
```

Visibility and storage are separate from namespace attributes:

- `@public` is the default and emits a declaration into the module header.
- `@private` emits a declaration only into the module implementation.
- `@internal` registers objects in the DSL symbol table but emits no C code.
  On a `package` or `module`, it applies to the container and everything
  inside it. On a `type`, it applies to that type alone. Use `@public` on a
  child to opt out and emit it normally inside an `@internal` container.
- `@extern` declares a binding in the header and defines it in the module
  source file.
- `@opaque` currently behaves like `@extern` for `let` bindings.
- `@include:` adds `#include` directives to the generated module header, or to
  the module source file for `@private` declarations and `@internal` modules.
  Each quoted string in the block becomes one include. Use `"<stdint.h>"` for
  system headers with angle brackets.

Modules containing only public declarations do not generate an empty `.c`
file. `@extern` and `@opaque` currently produce an error on types because a
`typedef` has neither external linkage nor a separate body.

Enums use an explicit underlying DSL type:

```text
module bool:
    enum module as lh.byte:
        case false
        case true
```

This generates a typedef using the referenced type and prefixed constants:

```c
#include "lh/char.h"

typedef lh_char_t lh_bool_t;

static const lh_bool_t lh_bool_false = 0;
static const lh_bool_t lh_bool_true = 1;
```

Inside an `enum`, `case` declares a member with an auto-incremented value.
Use `case name = expr` for an explicit constant integer expression:

```text
enum module as lh.byte:
    case false
    case true
    case read = 1 << 0
    case write = 1 << 1
    case rw = read | write
    case auto_a
```

Expressions use C operator precedence. Supported operators:

- arithmetic: `+`, `-`, `*`, `/`, `%`
- shifts: `<<`, `>>`
- comparisons: `<`, `>`, `<=`, `>=`, `==`, `!=`
- bitwise: `&`, `|`, `^`, `~`
- logical: `&&`, `||`, `!`
- unary `+` and `-`, and parentheses

A case value may reference earlier enumerators from the same enum by DSL name.
For a primary export (`enum module as`), members live at the module path
(`lh.interval.flags.lopen`), not under a repeated module segment:

```text
module flags:
    enum module as lh.byte:
        case closed
        case ropen = 1 << 0
        case lopen = 1 << 1
        case open = lh.interval.flags.lopen | lh.interval.flags.ropen
```

The compiler evaluates the expression to determine the next implicit case value;
the generated C keeps the lowered expression as written.

Implicit cases continue from the previous value using C enum rules.
Only `case` is allowed inside an `enum`; `let` is not supported there.

Module-level variables use `let`:

```text
module counter:
    type value as c.int
    let zero as counter.value = 0
    @mutable
    let current as counter.value = 0
```

Typed variables are `const` by default. `@mutable` removes `const`. `@public` and
`@private` control whether the binding is emitted into the module header or
implementation. `@extern` and `@opaque` emit an `extern` declaration in the
header and the definition in the module source file. An untyped binding is a
macro immediately:

```text
let answer = 42
let message = "hello"
let ratio = 1.25
@define
let enabled
```

The generator classifies macro values as integer, floating-point, or string.
`@define` on an untyped `let` is accepted but redundant. A valueless untyped
`let` emits an empty macro.

Module-level functions use `fn`. A function without a `return` emits `void`.
Functions with a return value must declare the result type on `fn` or cast the
return expression with `as type`:

```text
module math:
    type int as c.int
    fn ping:
    fn answer as math.int:
        return 42
    fn identity as math.int:
        param value as math.int
        return value
    fn casted:
        return 7 as math.int
```

This generates C prototypes in the module header and definitions in the module
source. `return value as type` emits a C cast before returning the value.

Inside a function, `let` declares a local variable. Local variables are `const`
by default; `@mutable` removes `const`. Unused locals are errors. Use `@used`
to keep a local declaration that is intentionally not referenced by the
function body:

```text
fn example as math.int:
    let value as math.int = 1
    return value

fn reserved:
    @used
    let slot as math.int = 0
```

Calls use the same inline parenthesis syntax as `c.initializer(...)`:

```text
fn allocate as math.int:
    @mutable
    let flags as math.int = 0
    return allocator(pool, &size, flags)
```

Function calls to known DSL functions are emitted with their C symbol names.
Arguments are ordinary expressions inside the parentheses.

Struct methods also accept a block form that declares parameters and performs
the call in one place. Parameters become part of the enclosing function
signature:

```text
@mutable
fn init:
    self.set:
        param major as lh.version.major
        param minor as lh.version.minor
        param patch as lh.version.patch
```

This lowers to the same C as separate `param` lines followed by
`self.set(major, minor, patch)`.

Several call blocks can be chained to compose a wrapper method from existing
ones. Each `self.method:` line opens a new call block; the next line at the
function body indent closes the previous block and emits its statement:

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

This lowers to three independent statements inside `lh_version_set`:

```c
void lh_version_set(lh_version_t *self,
                    const lh_version_major_t major,
                    const lh_version_minor_t minor,
                    const lh_version_patch_t patch)
{
    lh_version_set_major(self, major);
    lh_version_set_minor(self, minor);
    lh_version_set_patch(self, patch);
}
```

Mark exactly one struct method with `@initializer` to register the struct
constructor. Calls such as `lh.version(1, 9, 0)` lower to the generated
inline constructor, which delegates to that initializer method:

```text
@initializer
@mutable
fn init:
    self.set:
        param major as lh.version.major
        param minor as lh.version.minor
        param patch as lh.version.patch
```

`@initializer` is a flag attribute with no arguments. A struct may have only
one initializer function.

Struct methods also accept **inline `param`** in assignments. A typed parameter
is declared at the point of use and added to the enclosing function signature:

```text
@mutable
fn pack:
    self.major =? param major as lh.version.major

fn unpack:
    @pointer @mutable param major as lh.version.major ?= self.major

@mutable
fn set_major:
    self.major = param value as lh.version.major
```

`=?` implies `@pointer` on the parameter when it is omitted. Attributes such as
`@pointer` and `@mutable` attach to the `param` clause, not to a separate line
above the statement. This lowers to the same C as separate `param` lines
followed by the assignment. Standalone `param` lines and `self.method:` call
blocks remain supported.

### Operators

CGEM borrows a few PHP-style operators and adds pointer-specific assignment
forms for optional out-parameters.

**`??` — null coalescing (pointer operands only).** Returns the left operand
when it is not `NULL`; otherwise evaluates to the right operand. This matches
PHP's `??` for nullable pointers: numeric zero and other non-null values are
kept. The left operand must be a simple lvalue (a parameter, local, or
`self.field`) whose C type is a pointer.

```text
fn example:
    @pointer
    param path as lh.char
    return path ?? default_path
```

**`??=` — null coalescing assignment (pointer lvalues only).** Assigns the
right-hand side only when the left-hand pointer is `NULL`, equivalent to
`lhs = lhs ?? rhs` in PHP. The left operand must be a pointer lvalue.

```text
@mutable
fn ensure:
    @pointer
    param slot as lh.int
    slot ??= fallback
```

**`?:` — Elvis operator.** Shorthand ternary: `left ?: right` means
`left ? left : right`. The condition uses C truthiness (non-zero integers and
non-null pointers are kept).

```text
return name ?: "default"
```

**`=?` — optional pointer read (CGEM-specific).** When the right-hand pointer
is not `NULL`, assigns its dereferenced value to the left-hand side:

```text
self.major =? param major as lh.version.major
```

lowers to `if (major) self->major = *major`. The inline `param` form adds
`major` to the function signature; `=?` implies `@pointer` when omitted.

**`?=` — optional pointer write (CGEM-specific).** When the left-hand pointer
is not `NULL`, writes the right-hand value through it:

```text
@pointer @mutable param major as lh.version.major ?= self.major
```

lowers to `if (major) *major = self->major`.

Use `=?` and `?=` for optional out-parameters in struct methods, for example
when packing or unpacking through caller-provided pointers.

Template parameters generate reusable structure field macros:

```text
package lh:
    package pair:
        module fields:
            @mutable
            struct module:
                param first_type
                param second_type

                field first as first_type
                field second as second_type
```

Fields are `const` by default. `@mutable` on a field allows its value to be
changed.

Place `@mutable` on the structure to make every field mutable:

```text
@mutable
struct module:
    field first as first_type
    field second as second_type
```

This generates:

```c
#define lh_pair_fields(first_type, second_type) \
    const first_type first; \
    second_type second
```

Template structure fields can also use `@pointer`:

```text
@mutable
struct module:
    param T

    @pointer
    field a as T
    @pointer
    field b as T
```

This generates:

```c
#define pointers(T) \
    T * a; \
    T * b
```

On fields, `@expand` controls whether a pointer to a known DSL type uses its
C symbol or expanded C expression. Template parameters such as `T` are emitted
as-is because they are macro parameters, not DSL symbols.
