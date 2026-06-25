#ifndef CGEM_COMPILER_INTERNAL_H
#define CGEM_COMPILER_INTERNAL_H

#include "cgem/diagnostic.h"

#include <stdbool.h>

extern bool cg_compile_analyze_only;
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

typedef enum {
    BLOCK_PACKAGE,
    BLOCK_MODULE,
    BLOCK_SCOPE
} BlockKind;

typedef enum {
    DOC_ENTRY_TEXT
} DocEntryKind;

typedef enum {
    BLOCK_ATTR_NONE,
    BLOCK_ATTR_DOC,
    BLOCK_ATTR_INCLUDE,
    BLOCK_ATTR_REQUIRE
} BlockAttributeKind;

typedef struct {
    DocEntryKind kind;
    char *text;
} DocEntry;

typedef struct {
    DocEntry *entries;
    size_t entry_count;
    size_t entry_capacity;
} DocAttributes;

typedef struct {
    BlockKind kind;
    size_t indent;
    char *name;
    bool noscope;
    bool internal;
    bool has_primary_export;
    DocAttributes docs;
    char *readme_path;
} Block;

typedef struct {
    FILE *file;
    FILE *source_file;
    char *path;
    char *source_path;
    char *source_directory;
    char *relative_header;
    char *guard;
    char *body;
    size_t body_length;
    size_t body_capacity;
    char **includes;
    size_t include_count;
    size_t include_capacity;
    char **source_includes;
    size_t source_include_count;
    size_t source_include_capacity;
    size_t pending_blank_lines;
    bool header_has_declaration;
    bool source_has_declaration;
    DocAttributes module_docs;
} ModuleOutput;

typedef struct {
    char *header;
    char **includes;
    size_t include_count;
    size_t include_capacity;
} HeaderDeps;

typedef enum {
    SYMBOL_VALUE_UNKNOWN,
    SYMBOL_VALUE_INTEGER,
    SYMBOL_VALUE_FLOATING,
    SYMBOL_VALUE_STRING
} SymbolValueKind;

typedef enum {
    SYMBOL_KIND_UNKNOWN,
    SYMBOL_KIND_TYPE,
    SYMBOL_KIND_VALUE,
    SYMBOL_KIND_MACRO,
    SYMBOL_KIND_FN
} SymbolKind;

typedef struct {
    char *dsl_name;
    char *c_name;
    char *header;
    char *c_expr;
    bool is_define;
    bool is_internal;
    bool is_mutable;
    SymbolValueKind value_kind;
    SymbolKind kind;
    char *type_dsl_name;
} Symbol;

typedef struct {
    char **paths;
    size_t count;
    size_t capacity;
} IncludeAttributes;

typedef struct {
    char *dsl_name;
} ExprArg;

typedef struct {
    size_t indent;
    bool branch_open;
    bool define_mode;
} IfFrame;

typedef struct {
    bool active;
    size_t indent;
    char *c_name;
    char *type_name;
    ModuleOutput *module;
    bool use_source;
    bool use_define;
    bool emit;
    long long next_implicit_value;
    char *local_name;
    char *dsl_name;
} EnumOutput;

typedef struct {
    char *dsl_name;
    char *c_call_name;
} StructConstructor;

typedef struct {
    char *name;
    char *type;
    bool is_mutable;
    char *doc;
} StructField;

typedef struct {
    char *name;
    bool is_mutable;
} StructKnownField;

typedef struct {
    char *expr;
} StructMacroLine;

typedef enum {
    PARAM_REQUIRE_ANY,
    PARAM_REQUIRE_TYPE,
    PARAM_REQUIRE_VALUE
} ParamRequireKind;

typedef struct {
    ParamRequireKind kind;
    char *constraint_dsl;
} ParamRequire;

typedef struct {
    char *dsl_name;
    char *c_name;
    char *header;
    char **params;
    size_t param_count;
    ParamRequire *param_requires;
    bool all_mutable;
    StructField *fields;
    size_t field_count;
} StructTemplate;

typedef struct {
    char *name;
    bool is_ptr;
    bool is_param_ref;
} FieldType;

typedef struct {
    char *name;
    char *c_type;
    char *value;
    bool is_mutable;
    bool force_used;
    bool used;
    size_t line_number;
} FunctionLocal;

typedef struct {
    char *value;
    bool is_ref;
} FunctionArg;

typedef struct {
    bool active;
    size_t indent;
    char *c_name;
    char *dsl_name;
    char *return_type;
    char *return_expr;
    char *return_cast_type;
    ModuleOutput *module;
    DocAttributes docs;
    FunctionLocal *locals;
    size_t local_count;
    size_t local_capacity;
    FunctionArg *args;
    size_t arg_count;
    size_t arg_capacity;
    bool use_source;
    bool emit;
    bool has_return;
    bool return_is_call;
    bool local_mutable;
    bool local_used;
    bool param_mutable_pending;
    bool param_pointer_pending;
    bool local_pointer_pending;
    bool return_is_initializer;
    size_t initializer_value_count;
    bool is_initializer;
    char *call_block_method;
    size_t call_block_param_start;
    char **params;
    char **param_types;
    bool *param_variadic;
    ParamRequire *param_requires;
    size_t param_count;
    bool has_meta_params;
    bool pending_param_require;
    ParamRequire pending_param_require_value;
    bool is_method;
    bool self_mutable;
    char *struct_dsl_name;
    char *self_struct_tag;
    const StructKnownField *struct_known_fields;
    size_t struct_known_field_count;
    const StructField *struct_fields;
    size_t struct_field_count;
    char **statements;
    size_t statement_count;
    size_t statement_capacity;
} FunctionOutput;

typedef struct {
    bool active;
    size_t indent;
    char *c_name;
    char *dsl_name;
    char *header;
    char **params;
    size_t param_count;
    bool *param_variadic;
    ParamRequire *param_requires;
    bool pending_param_require;
    ParamRequire pending_param_require_value;
    StructField *fields;
    size_t field_count;
    size_t field_capacity;
    StructField *registry_fields;
    size_t registry_field_count;
    size_t registry_field_capacity;
    StructKnownField *known_fields;
    size_t known_field_count;
    size_t known_field_capacity;
    StructMacroLine *macro_lines;
    size_t macro_line_count;
    size_t macro_line_capacity;
    ModuleOutput *module;
    bool all_mutable;
    bool field_expand;
    bool field_pointer;
    bool emit;
    DocAttributes docs;
    char **param_docs;
    FunctionOutput *methods;
    size_t method_count;
    size_t method_capacity;
    char *initializer_init_c_name;
    char **initializer_param_names;
    char **initializer_param_c_types;
    size_t initializer_param_count;
} StructOutput;

bool cg_blocks_are_internal(const Block *blocks, size_t count);
bool cg_should_emit_c(const Block *blocks, size_t block_count,
                      bool attribute_internal, bool attribute_public);
bool cg_name_start(unsigned char ch);
bool cg_name_char(unsigned char ch);
char *cg_copy_text(const char *text, size_t length);
char *cg_join_path(const char *left, const char *right);
void cg_free_names(char **names, size_t count);
const char *cg_param_c_type(const char **params,
                                     const bool *param_variadic, size_t count,
                                     const char *name);
ssize_t cg_param_index(const char **params, size_t count, const char *name);

char *cg_build_package_path(const char *base, Block *blocks, size_t count);
char *cg_build_symbol_prefix(Block *blocks, size_t count, const char *name,
                             bool type_suffix);
char *cg_build_dsl_name(Block *blocks, size_t count, const char *name);
bool cg_is_module_export_name(const char *decl_name);
char *cg_resolve_export_name(Block *blocks, size_t count, const char *decl_name);
const char *cg_export_c_suffix(Block *blocks, size_t count,
                               const char *decl_name);
char *cg_build_export_symbol(Block *blocks, size_t count, const char *decl_name,
                             bool type_suffix);
int cg_note_primary_export(Block *blocks, size_t count, const char *decl_name,
                           size_t line_number, char *error, size_t error_size);
int cg_add_primary_export_alias(Block *blocks, size_t block_count,
                                Symbol **symbols, size_t *symbol_count,
                                size_t *symbol_capacity);
int cg_add_symbol(Symbol **symbols, size_t *count, size_t *capacity,
                  char *dsl_name, const char *c_name, const char *header,
                  const char *c_expr, bool is_define, bool is_internal);
int cg_add_symbol_ex(Symbol **symbols, size_t *count, size_t *capacity,
                     char *dsl_name, const char *c_name, const char *header,
                     const char *c_expr, bool is_define, bool is_internal,
                     bool is_mutable, SymbolKind kind, char *type_dsl_name);
Symbol *cg_find_symbol(Symbol *symbols, size_t count, const char *name);
Symbol *cg_find_symbol_by_c_name(Symbol *symbols, size_t count,
                                 const char *name);
int cg_add_builtin_c_types(Symbol **symbols, size_t *count, size_t *capacity);
int cg_add_compiler_macros(Symbol **symbols, size_t *count, size_t *capacity,
                           const char *compiler, char *error,
                           size_t error_size);
const char *cg_binding_value(Symbol *symbol, bool expand);
char *cg_make_pointer_type(const char *base);

void cg_close_enum(EnumOutput *output);
int cg_emit_let(ModuleOutput *module, bool use_source, bool use_define,
                bool emit, bool is_mutable, bool is_extern, bool is_opaque,
                const DocAttributes *docs, const char *c_type,
                const char *c_symbol, const char *value, char *error,
                size_t error_size);
void cg_clear_struct(StructOutput *output);
int cg_close_struct(StructOutput *output, char *error, size_t error_size);
void cg_clear_function(FunctionOutput *output);
int cg_emit_function(FunctionOutput *output, char *error, size_t error_size);
int cg_close_function(FunctionOutput *output, StructOutput *struct_owner,
                      char *error, size_t error_size);
int cg_module_body_append(ModuleOutput *module, const char *text, size_t length);
int cg_module_body_printf(ModuleOutput *module, const char *format, ...);
void cg_free_doc_entry(DocEntry *entry);
void cg_clear_doc_attributes(DocAttributes *attributes);
void cg_clear_include_attributes(IncludeAttributes *attributes);
int cg_add_include_attribute(IncludeAttributes *attributes, char *path);
int cg_apply_include_attributes(ModuleOutput *module,
                                const IncludeAttributes *attributes,
                                HeaderDeps **deps, size_t *deps_count,
                                size_t *deps_capacity, bool to_source,
                                char *error, size_t error_size);
int cg_add_doc_entry(DocAttributes *attributes, DocEntry *entry);
int cg_emit_doc_comment(ModuleOutput *module, FILE *file,
                            const DocAttributes *attributes);
int cg_emit_struct_macro_doc_comment(ModuleOutput *module,
                                     const DocAttributes *brief_docs,
                                     const char **params,
                                     const char **param_docs,
                                     const bool *param_variadic,
                                     size_t param_count);
int cg_emit_struct_member_doc_comment(ModuleOutput *module,
                                      const char *indent,
                                      const char *doc_text);
int cg_append_struct_member_suffix_doc(ModuleOutput *module,
                                       const char *doc_text);
int cg_write_package_readme(const Block *block, char *error, size_t error_size);
void cg_free_block(Block *block);
int cg_pop_block(Block *blocks, size_t *block_count, ModuleOutput *module,
                 HeaderDeps *deps, size_t deps_count, const char *format_style,
                 char *error, size_t error_size);
int cg_module_require_include(ModuleOutput *module, const char *header,
                              HeaderDeps **deps, size_t *deps_count,
                              size_t *deps_capacity);
void cg_flush_module_blank_lines(ModuleOutput *module);
int cg_close_module(ModuleOutput *module, HeaderDeps *deps, size_t deps_count,
                    const char *format_style, char *error, size_t error_size);
void cg_free_header_deps(HeaderDeps *deps, size_t count);
int cg_ensure_module_source(ModuleOutput *module, char *error,
                            size_t error_size);
int cg_ensure_module_header(ModuleOutput *module, char *error, size_t error_size);
int cg_open_module(ModuleOutput *module, const char *include_path,
                   const char *source_path, Block *blocks, size_t count,
                   char *error, size_t error_size);

bool cg_parse_named_block(const char *text, const char *keyword, char **name);
bool cg_parse_block(const char *text, const char *keyword);
void cg_free_expr_args(ExprArg *args, size_t count);
bool cg_parse_struct_module(const char *text);
bool cg_parse_type(const char *text, char **name, const char **expression,
                   size_t *expression_length, char **reference,
                   ExprArg **expr_args, size_t *expr_arg_count);
bool cg_parse_enum(const char *text, char **name, char **base);
bool cg_parse_case(const char *text, char **name, char **value);
bool cg_parse_simple_name(const char *text, const char *keyword, char **name);
bool cg_parse_param(const char *text, char **name, FieldType *type,
                    bool *is_meta, bool *is_variadic);
bool cg_parse_inline_param(const char *text, size_t *consumed, bool *pointer,
                           bool *mutable_attr, char **name, FieldType *type);
ParamRequire cg_param_require_any(void);
void cg_param_require_free(ParamRequire *require);
ParamRequire cg_param_require_copy(ParamRequire require);
void cg_param_require_free_array(ParamRequire *requires, size_t count);
bool cg_parse_require_spec(const char *spec, ParamRequire *require);
bool cg_parse_require_attribute(const char *text, ParamRequire *require);
void cg_free_field_type(FieldType *type);
bool cg_parse_field(const char *text, char **name, FieldType *type);
bool cg_parse_let(const char *text, char **name, FieldType *type, char **value);
bool cg_parse_fn(const char *text, char **name, FieldType *return_type);
bool cg_parse_return(const char *text, char **expression, FieldType *cast_type);
bool cg_parse_dsl_reference(const char *text, char **name);
bool cg_parse_paren_call(const char *expression, char **callee, char ***args,
                         size_t *arg_count);
void cg_free_cstr_array(char **items, size_t count);
bool cg_parse_as_field_type(const char *text, size_t *at, FieldType *type);
bool cg_name_in_list(char **names, size_t count, const char *name);
bool cg_parse_escaped_string(const char *text, size_t *at, char **value);
bool cg_parse_block_attribute_opener(const char *text,
                                     BlockAttributeKind *kind);
bool cg_parse_block_attribute_string(const char *text, char **value);
bool cg_parse_inline_block_attribute(const char *text, BlockAttributeKind *kind,
                                     char **value);
bool cg_parse_attribute_call(const char *text, const char **name,
                             size_t *name_length, char ***args,
                             size_t *arg_count);
bool cg_parse_self_method_call_block_opener(const char *text, char **method_name);
bool cg_parse_attribute(const char *text, const char **name,
                        size_t *name_length);
bool cg_parse_if_block(const char *text, char **condition);
bool cg_parse_elif_block(const char *text, char **condition);
bool cg_parse_else_block(const char *text);
int cg_emit_preprocessor(ModuleOutput *module, const char *directive,
                         char *error, size_t error_size);
int cg_close_if_frames(IfFrame **frames, size_t *depth, size_t until_indent,
                       ModuleOutput *module, char *error, size_t error_size);
ssize_t cg_read_input_line(FILE *input, char **line, size_t *capacity);

#endif
