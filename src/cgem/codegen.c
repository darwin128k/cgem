#define _POSIX_C_SOURCE 200809L

#include "cgem/compiler_internal.h"
#include "cgem/platform.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool cg_compile_analyze_only = false;

char *cg_build_package_path(const char *base, Block *blocks, size_t count)
{
    char *path = strdup(base);

    if (!path) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        char *next;

        if (blocks[i].kind != BLOCK_PACKAGE) {
            continue;
        }
        next = cg_join_path(path, blocks[i].name);
        free(path);
        if (!next) {
            return NULL;
        }
        path = next;
    }
    return path;
}
void cg_close_enum(EnumOutput *output)
{
    free(output->c_name);
    free(output->type_name);
    free(output->local_name);
    free(output->dsl_name);
    *output = (EnumOutput) {0};
}

int cg_emit_let(ModuleOutput *module, bool use_source, bool use_define,
                bool emit, bool is_mutable, bool is_extern, bool is_opaque,
                const DocAttributes *docs, const char *c_type,
                const char *c_symbol, const char *value, char *error,
                size_t error_size)
{
    const char *storage = is_mutable ? "" : "const ";

    if (cg_compile_analyze_only) {
        return 0;
    }
    if (!emit) {
        return 0;
    }
    if (use_define) {
        const char *define_value = value ? value : "";
        if (use_source) {
            if (module->source_has_declaration) {
                while (module->pending_blank_lines > 0) {
                    fputc('\n', module->source_file);
                    module->pending_blank_lines--;
                }
            } else {
                module->pending_blank_lines = 0;
            }
            if (cg_emit_doc_comment(module, module->source_file, docs) != 0) {
                snprintf(error, error_size, "out of memory");
                return -1;
            }
            fprintf(module->source_file, "#define %s%s%s\n", c_symbol,
                    define_value[0] ? " " : "", define_value);
            module->source_has_declaration = true;
        } else {
            cg_flush_module_blank_lines(module);
            if (cg_emit_doc_comment(module, NULL, docs) != 0) {
                snprintf(error, error_size, "out of memory");
                return -1;
            }
            if (cg_module_body_printf(module, "#define %s%s%s\n", c_symbol,
                                      define_value[0] ? " " : "",
                                      define_value) != 0) {
                snprintf(error, error_size, "out of memory");
                return -1;
            }
            module->header_has_declaration = true;
        }
        return 0;
    }
    if ((is_extern || is_opaque) && !use_source) {
        cg_flush_module_blank_lines(module);
        if (cg_emit_doc_comment(module, NULL, docs) != 0) {
            snprintf(error, error_size, "out of memory");
            return -1;
        }
        if (cg_module_body_printf(module, "extern %s%s %s;\n", storage, c_type,
                                  c_symbol) != 0) {
            snprintf(error, error_size, "out of memory");
            return -1;
        }
        module->header_has_declaration = true;
        if (cg_ensure_module_source(module, error, error_size) != 0) {
            return -1;
        }
        if (module->source_has_declaration) {
            while (module->pending_blank_lines > 0) {
                fputc('\n', module->source_file);
                module->pending_blank_lines--;
            }
        } else {
            module->pending_blank_lines = 0;
        }
        fprintf(module->source_file, "%s%s %s = %s;\n", storage, c_type,
                c_symbol, value);
        module->source_has_declaration = true;
        return 0;
    }
    if (use_source) {
        if (cg_ensure_module_source(module, error, error_size) != 0) {
            return -1;
        }
        if (module->source_has_declaration) {
            while (module->pending_blank_lines > 0) {
                fputc('\n', module->source_file);
                module->pending_blank_lines--;
            }
        } else {
            module->pending_blank_lines = 0;
        }
        if (cg_emit_doc_comment(module, module->source_file, docs) != 0) {
            snprintf(error, error_size, "out of memory");
            return -1;
        }
        fprintf(module->source_file, "static %s%s %s = %s;\n", storage,
                c_type, c_symbol, value);
        module->source_has_declaration = true;
    } else {
        cg_flush_module_blank_lines(module);
        if (cg_emit_doc_comment(module, NULL, docs) != 0) {
            snprintf(error, error_size, "out of memory");
            return -1;
        }
        if (cg_module_body_printf(module, "static %s%s %s = %s;\n", storage,
                                  c_type, c_symbol, value) != 0) {
            snprintf(error, error_size, "out of memory");
            return -1;
        }
        module->header_has_declaration = true;
    }
    return 0;
}

void cg_clear_struct(StructOutput *output)
{
    free(output->c_name);
    free(output->dsl_name);
    free(output->header);
    cg_free_names(output->params, output->param_count);
    free(output->param_variadic);
    free(output->param_requires);
    for (size_t i = 0; i < output->field_count; i++) {
        free(output->fields[i].name);
        free(output->fields[i].type);
        free(output->fields[i].doc);
    }
    free(output->fields);
    for (size_t i = 0; i < output->known_field_count; i++) {
        free(output->known_fields[i].name);
    }
    free(output->known_fields);
    for (size_t i = 0; i < output->method_count; i++) {
        cg_clear_function(&output->methods[i]);
    }
    free(output->methods);
    for (size_t i = 0; i < output->macro_line_count; i++) {
        free(output->macro_lines[i].expr);
    }
    free(output->macro_lines);
    cg_clear_doc_attributes(&output->docs);
    for (size_t i = 0; i < output->param_count; i++) {
        free(output->param_docs[i]);
    }
    free(output->param_docs);
    free(output->initializer_init_c_name);
    cg_free_names(output->initializer_param_names, output->initializer_param_count);
    cg_free_names(output->initializer_param_c_types,
                  output->initializer_param_count);
    *output = (StructOutput) {0};
}

int cg_close_struct(StructOutput *output, char *error, size_t error_size)
{
    int result = 0;

    if (!output->active) {
        return 0;
    }
    if (!output->emit || cg_compile_analyze_only) {
        cg_clear_struct(output);
        return 0;
    }
    if (output->param_count == 0) {
        if (cg_module_body_printf(output->module, "typedef struct %s {\n",
                                  output->c_name) != 0) {
            result = -1;
            goto done;
        }
        for (size_t i = 0; i < output->field_count; i++) {
            if (output->fields[i].doc &&
                cg_emit_struct_member_doc_comment(output->module, "    ",
                                                  output->fields[i].doc) != 0) {
                result = -1;
                goto done;
            }
            if (cg_module_body_printf(
                    output->module, "    %s%s %s;\n",
                    output->fields[i].is_mutable ? "" : "const ",
                    output->fields[i].type, output->fields[i].name) != 0) {
                result = -1;
                goto done;
            }
        }
        for (size_t i = 0; i < output->macro_line_count; i++) {
            if (cg_module_body_printf(output->module, "    %s;\n",
                                      output->macro_lines[i].expr) != 0) {
                result = -1;
                goto done;
            }
        }
        if (cg_module_body_printf(output->module, "} %s_t;\n",
                                  output->c_name) != 0) {
            result = -1;
            goto done;
        }
        for (size_t i = 0; i < output->method_count; i++) {
            if (cg_emit_function(&output->methods[i], error, error_size) != 0) {
                result = -1;
                goto done;
            }
            cg_clear_function(&output->methods[i]);
        }
        if (output->initializer_init_c_name &&
            output->initializer_param_count > 0) {
            if (cg_module_body_printf(output->module,
                                      "static inline %s_t %s(",
                                      output->c_name, output->c_name) != 0) {
                result = -1;
                goto done;
            }
            for (size_t i = 0; i < output->initializer_param_count; i++) {
                if (cg_module_body_printf(
                        output->module, "%s%s %s",
                        i ? ", " : "", output->initializer_param_c_types[i],
                        output->initializer_param_names[i]) != 0) {
                    result = -1;
                    goto done;
                }
            }
            if (cg_module_body_printf(output->module, ")\n{\n") != 0 ||
                cg_module_body_printf(output->module, "    %s_t result;\n",
                                      output->c_name) != 0 ||
                cg_module_body_printf(output->module, "    %s(&result",
                                      output->initializer_init_c_name) != 0) {
                result = -1;
                goto done;
            }
            for (size_t i = 0; i < output->initializer_param_count; i++) {
                if (cg_module_body_printf(output->module, ", %s",
                                          output->initializer_param_names[i]) !=
                    0) {
                    result = -1;
                    goto done;
                }
            }
            if (cg_module_body_printf(output->module,
                                      ");\n    return result;\n}\n\n") != 0) {
                result = -1;
                goto done;
            }
            output->module->header_has_declaration = true;
        }
        goto done;
    }
    if (cg_emit_struct_macro_doc_comment(
            output->module, &output->docs, (const char **) output->params,
            (const char **) output->param_docs, output->param_variadic,
            output->param_count) != 0) {
        result = -1;
        goto done;
    }
    if (cg_module_body_printf(output->module, "#define %s(",
                           output->c_name) != 0) {
        result = -1;
        goto done;
    }
    for (size_t i = 0; i < output->param_count; i++) {
        if (output->param_variadic && output->param_variadic[i]) {
            if (cg_module_body_printf(output->module, "%s...",
                                   i ? ", " : "") != 0) {
                result = -1;
                goto done;
            }
        } else if (cg_module_body_printf(output->module, "%s%s",
                                      i ? ", " : "", output->params[i]) != 0) {
            result = -1;
            goto done;
        }
    }
    if (output->field_count == 0) {
        if (cg_module_body_printf(output->module, ")\n") != 0) {
            result = -1;
        }
        goto done;
    }
    if (cg_module_body_printf(output->module, ") \\\n") != 0) {
        result = -1;
        goto done;
    }
    for (size_t i = 0; i < output->field_count; i++) {
        if (cg_module_body_printf(
                output->module, "    %s%s %s",
                output->fields[i].is_mutable ? "" : "const ",
                output->fields[i].type, output->fields[i].name) != 0) {
            result = -1;
            goto done;
        }
        if (output->fields[i].doc &&
            cg_append_struct_member_suffix_doc(output->module,
                                               output->fields[i].doc) != 0) {
            result = -1;
            goto done;
        }
        if (cg_module_body_printf(
                output->module, "%s\n",
                i + 1 < output->field_count ? "; \\" : "") != 0) {
            result = -1;
            goto done;
        }
    }

done:
    cg_clear_struct(output);
    return result;
}

void cg_clear_function(FunctionOutput *output)
{
    free(output->c_name);
    free(output->dsl_name);
    free(output->return_type);
    free(output->return_expr);
    free(output->return_cast_type);
    free(output->call_block_method);
    cg_free_names(output->params, output->param_count);
    cg_free_names(output->param_types, output->param_count);
    free(output->param_variadic);
    free(output->param_requires);
    free(output->struct_dsl_name);
    free(output->self_struct_tag);
    for (size_t i = 0; i < output->statement_count; i++) {
        free(output->statements[i]);
    }
    free(output->statements);
    for (size_t i = 0; i < output->local_count; i++) {
        free(output->locals[i].name);
        free(output->locals[i].c_type);
        free(output->locals[i].value);
    }
    free(output->locals);
    for (size_t i = 0; i < output->arg_count; i++) {
        free(output->args[i].value);
    }
    free(output->args);
    cg_clear_doc_attributes(&output->docs);
    *output = (FunctionOutput) {0};
}

static int write_source_function(ModuleOutput *module, bool is_static,
                                 const char *return_type,
                                 const char *c_name,
                                 char **params, char **param_types,
                                 size_t param_count,
                                 const char *return_expr,
                                 const char *return_cast_type,
                                 const FunctionLocal *locals,
                                 size_t local_count,
                                 const FunctionArg *args,
                                 size_t arg_count, const char **statements,
                                 size_t statement_count, bool has_return,
                                 bool return_is_call, bool return_is_initializer,
                                 char *error,
                                 size_t error_size)
{
    FILE *out = NULL;

    if (cg_ensure_module_source(module, error, error_size) != 0) {
        return -1;
    }
    out = module->source_file;
    if (module->source_has_declaration) {
        while (module->pending_blank_lines > 0) {
            fputc('\n', out);
            module->pending_blank_lines--;
        }
    } else {
        module->pending_blank_lines = 0;
    }
    if (fprintf(out, "%s%s %s(",
                is_static ? "static " : "", return_type, c_name) < 0 ||
        (param_count == 0 && fprintf(out, "void") < 0)) {
        goto fail;
    }
    for (size_t i = 0; i < param_count; i++) {
        if (fprintf(out, "%s%s %s", i ? ", " : "",
                    param_types[i], params[i]) < 0) {
            goto fail;
        }
    }
    if (fprintf(out, ")\n{\n") < 0) {
        goto fail;
    }
    for (size_t i = 0; i < local_count; i++) {
        if (fprintf(out, "    %s%s %s = %s;\n",
                    locals[i].is_mutable ? "" : "const ", locals[i].c_type,
                    locals[i].name, locals[i].value) < 0) {
            goto fail;
        }
    }
    for (size_t i = 0; i < statement_count; i++) {
        if (fprintf(out, "    %s;\n", statements[i]) < 0) {
            goto fail;
        }
    }
    if (has_return) {
        if (return_expr) {
            if (return_is_initializer) {
                if (fprintf(out, "    return (%s){%s};\n",
                            return_cast_type ? return_cast_type : return_type,
                            return_expr) < 0) {
                    goto fail;
                }
            } else if (return_is_call) {
                if (fprintf(out, "    return ") < 0) {
                    goto fail;
                }
                if (return_cast_type) {
                    if (fprintf(out, "(%s)(", return_cast_type) < 0) {
                        goto fail;
                    }
                }
                if (fprintf(out, "%s(", return_expr) < 0) {
                    goto fail;
                }
                for (size_t i = 0; i < arg_count; i++) {
                    if (fprintf(out, "%s", i ? ", " : "") < 0) {
                        goto fail;
                    }
                    if (args[i].is_ref) {
                        if (fprintf(out, "&(%s)", args[i].value) < 0) {
                            goto fail;
                        }
                    } else {
                        if (fprintf(out, "%s", args[i].value) < 0) {
                            goto fail;
                        }
                    }
                }
                if (fprintf(out, ")") < 0) {
                    goto fail;
                }
                if (return_cast_type) {
                    if (fprintf(out, ")") < 0) {
                        goto fail;
                    }
                }
                if (fprintf(out, ";\n") < 0) {
                    goto fail;
                }
            } else if (return_cast_type) {
                if (fprintf(out, "    return (%s)(%s);\n",
                            return_cast_type, return_expr) < 0) {
                    goto fail;
                }
            } else {
                if (fprintf(out, "    return %s;\n", return_expr) < 0) {
                    goto fail;
                }
            }
        } else {
            if (fprintf(out, "    return;\n") < 0) {
                goto fail;
            }
        }
    }
    if (fprintf(out, "}\n") < 0) {
        goto fail;
    }
    module->source_has_declaration = true;
    return 0;

fail:
    cg_set_error(error, error_size, "write failed for module source");
    return -1;
}

static int capture_struct_initializer(StructOutput *struct_owner,
                                    FunctionOutput *output, char *error,
                                    size_t error_size)
{
    size_t param_count;

    if (!output->is_initializer) {
        return 0;
    }
    if (struct_owner->initializer_init_c_name) {
        cg_set_error(error, error_size,
                     "struct allows only one @initializer function");
        return -1;
    }
    if (output->param_count < 2) {
        cg_set_error(error, error_size,
                     "@initializer function requires parameters besides self");
        return -1;
    }
    struct_owner->initializer_init_c_name = strdup(output->c_name);
    if (!struct_owner->initializer_init_c_name) {
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    param_count = output->param_count - 1;
    struct_owner->initializer_param_names =
        calloc(param_count, sizeof(*struct_owner->initializer_param_names));
    struct_owner->initializer_param_c_types =
        calloc(param_count, sizeof(*struct_owner->initializer_param_c_types));
    if (!struct_owner->initializer_param_names ||
        !struct_owner->initializer_param_c_types) {
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    for (size_t i = 0; i < param_count; i++) {
        struct_owner->initializer_param_names[i] =
            strdup(output->params[i + 1]);
        struct_owner->initializer_param_c_types[i] =
            strdup(output->param_types[i + 1]);
        if (!struct_owner->initializer_param_names[i] ||
            !struct_owner->initializer_param_c_types[i]) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
    }
    struct_owner->initializer_param_count = param_count;
    return 0;
}

static int stash_struct_method(StructOutput *struct_owner,
                               FunctionOutput *output, char *error,
                               size_t error_size)
{
    FunctionOutput *grown;
    size_t capacity;

    if (struct_owner->method_count == struct_owner->method_capacity) {
        capacity = struct_owner->method_capacity
                       ? struct_owner->method_capacity * 2
                       : 4;
        grown = realloc(struct_owner->methods,
                        capacity * sizeof(*struct_owner->methods));
        if (!grown) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        struct_owner->methods = grown;
        struct_owner->method_capacity = capacity;
    }
    struct_owner->methods[struct_owner->method_count++] = *output;
    *output = (FunctionOutput) {0};
    return 0;
}

int cg_emit_function(FunctionOutput *output, char *error, size_t error_size)
{
    const char *return_type = output->return_type;

    if (!output->emit || cg_compile_analyze_only) {
        return 0;
    }
    if (!output->use_source) {
        if (cg_ensure_module_header(output->module, error, error_size) != 0) {
            return -1;
        }
        cg_flush_module_blank_lines(output->module);
        if (cg_emit_doc_comment(output->module, NULL, &output->docs) != 0) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        if (cg_module_body_printf(output->module, "%s %s(", return_type,
                                  output->c_name) != 0) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        if (output->param_count == 0 &&
            cg_module_body_printf(output->module, "void") != 0) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        for (size_t i = 0; i < output->param_count; i++) {
            if (cg_module_body_printf(
                    output->module, "%s%s %s", i ? ", " : "",
                    output->param_types[i], output->params[i]) != 0) {
                cg_set_error(error, error_size, "out of memory");
                return -1;
            }
        }
        if (cg_module_body_printf(output->module, ");\n") != 0) {
            cg_set_error(error, error_size, "out of memory");
            return -1;
        }
        output->module->header_has_declaration = true;
    }
    return write_source_function(
        output->module, output->use_source, return_type, output->c_name,
        output->params, output->param_types, output->param_count,
        output->return_expr, output->return_cast_type, output->locals,
        output->local_count, output->args, output->arg_count,
        (const char **) output->statements, output->statement_count,
        output->has_return, output->return_is_call,
        output->return_is_initializer, error, error_size);
}

int cg_close_function(FunctionOutput *output, StructOutput *struct_owner,
                      char *error, size_t error_size)
{
    const char *return_type;
    int result = 0;

    if (!output->active) {
        return 0;
    }
    if (output->has_meta_params) {
        if (!output->has_return || !output->return_is_initializer ||
            output->initializer_value_count == 0 || !output->return_expr ||
            output->return_is_call || output->return_cast_type ||
            output->local_count > 0 || output->arg_count > 0) {
            cg_set_error(error, error_size,
                         "parameterized fn requires a single inline "
                         "c.initializer(...) return");
            result = -1;
            goto done;
        }
        if (output->return_type) {
            cg_set_error(error, error_size,
                         "parameterized fn cannot declare a return type");
            result = -1;
            goto done;
        }
        if (output->use_source) {
            cg_set_error(error, error_size,
                         "parameterized fn cannot be private");
            result = -1;
            goto done;
        }
        if (!output->emit || cg_compile_analyze_only) {
            goto done;
        }
        if (cg_ensure_module_header(output->module, error, error_size) != 0) {
            result = -1;
            goto done;
        }
        cg_flush_module_blank_lines(output->module);
        if (cg_emit_doc_comment(output->module, NULL, &output->docs) != 0 ||
            cg_module_body_printf(output->module, "#define %s(",
                                  output->c_name) != 0) {
            cg_set_error(error, error_size, "out of memory");
            result = -1;
            goto done;
        }
        for (size_t i = 0; i < output->param_count; i++) {
            if (output->param_variadic &&
                output->param_variadic[i]) {
                if (cg_module_body_printf(output->module, "%s...",
                                          i ? ", " : "") != 0) {
                    cg_set_error(error, error_size, "out of memory");
                    result = -1;
                    goto done;
                }
            } else if (cg_module_body_printf(
                           output->module, "%s%s", i ? ", " : "",
                           output->params[i]) != 0) {
                cg_set_error(error, error_size, "out of memory");
                result = -1;
                goto done;
            }
        }
        if (cg_module_body_printf(output->module, ") {%s}\n",
                                  output->return_expr) != 0) {
            cg_set_error(error, error_size, "out of memory");
            result = -1;
            goto done;
        }
        output->module->header_has_declaration = true;
        goto done;
    }
    if (output->local_mutable || output->local_used ||
        output->local_pointer_pending) {
        cg_set_error(error, error_size,
                     "local attributes at end of function have no statement");
        result = -1;
        goto done;
    }
    if (output->param_mutable_pending) {
        cg_set_error(error, error_size,
                     "@mutable at end of function has no param");
        result = -1;
        goto done;
    }
    if (output->param_pointer_pending) {
        cg_set_error(error, error_size,
                     "@pointer at end of function has no param");
        result = -1;
        goto done;
    }
    if (!output->return_type) {
        if (output->has_return && output->return_expr) {
            cg_set_error(error, error_size,
                         "function with return value requires an explicit "
                         "return type");
            result = -1;
            goto done;
        }
        output->return_type = strdup("void");
        if (!output->return_type) {
            cg_set_error(error, error_size, "out of memory");
            result = -1;
            goto done;
        }
    }
    return_type = output->return_type;
    for (size_t i = 0; i < output->local_count; i++) {
        if (!output->locals[i].used && !output->locals[i].force_used) {
            cg_set_error(error, error_size,
                         "line %zu: unused local variable: %s",
                         output->locals[i].line_number,
                         output->locals[i].name);
            result = -1;
            goto done;
        }
    }
    if (!output->has_return && strcmp(return_type, "void") != 0) {
        cg_set_error(error, error_size,
                     "non-void function requires a return statement");
        result = -1;
        goto done;
    }
    if (output->is_method && output->has_return && output->statement_count > 0) {
        cg_set_error(error, error_size,
                     "struct method cannot mix return with body statements");
        result = -1;
        goto done;
    }
    if (!output->emit) {
        goto done;
    }
    if (output->is_method && struct_owner) {
        if (capture_struct_initializer(struct_owner, output, error,
                                       error_size) != 0) {
            result = -1;
            goto done;
        }
        if (stash_struct_method(struct_owner, output, error, error_size) != 0) {
            result = -1;
        }
        goto done;
    }
    if (!output->use_source) {
        if (cg_emit_function(output, error, error_size) != 0) {
            result = -1;
        }
        goto done;
    }
    if (write_source_function(output->module, output->use_source, return_type,
                              output->c_name, output->params,
                              output->param_types, output->param_count,
                              output->return_expr,
                              output->return_cast_type, output->locals,
                              output->local_count, output->args,
                              output->arg_count,
                              (const char **) output->statements,
                              output->statement_count, output->has_return,
                              output->return_is_call,
                              output->return_is_initializer,
                              error, error_size) != 0) {
        result = -1;
    }

done:
    cg_clear_function(output);
    return result;
}

static HeaderDeps *find_header_deps(HeaderDeps *deps, size_t count,
                                    const char *header)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(deps[i].header, header) == 0) {
            return &deps[i];
        }
    }
    return NULL;
}

static int record_header_include(HeaderDeps **deps, size_t *count,
                                 size_t *capacity, const char *header,
                                 const char *include)
{
    HeaderDeps *entry = find_header_deps(*deps, *count, header);

    if (!entry) {
        if (*count == *capacity) {
            size_t next = *capacity ? *capacity * 2 : 8;
            HeaderDeps *grown = realloc(*deps, next * sizeof(**deps));

            if (!grown) {
                return -1;
            }
            *deps = grown;
            *capacity = next;
        }
        entry = &(*deps)[(*count)++];
        entry->header = strdup(header);
        entry->includes = NULL;
        entry->include_count = 0;
        entry->include_capacity = 0;
        if (!entry->header) {
            return -1;
        }
    }
    for (size_t i = 0; i < entry->include_count; i++) {
        if (strcmp(entry->includes[i], include) == 0) {
            return 0;
        }
    }
    if (entry->include_count == entry->include_capacity) {
        size_t next = entry->include_capacity ? entry->include_capacity * 2 : 4;
        char **grown = realloc(entry->includes, next * sizeof(*entry->includes));

        if (!grown) {
            return -1;
        }
        entry->includes = grown;
        entry->include_capacity = next;
    }
    entry->includes[entry->include_count] = strdup(include);
    if (!entry->includes[entry->include_count]) {
        return -1;
    }
    entry->include_count++;
    return 0;
}

static bool header_transitively_includes(HeaderDeps *deps, size_t count,
                                         const char *from, const char *to)
{
    HeaderDeps *entry = find_header_deps(deps, count, from);

    if (!entry) {
        return false;
    }
    for (size_t i = 0; i < entry->include_count; i++) {
        if (strcmp(entry->includes[i], to) == 0) {
            return true;
        }
        if (header_transitively_includes(deps, count, entry->includes[i], to)) {
            return true;
        }
    }
    return false;
}

static bool include_covered_by_other(const char *candidate,
                                     char **includes, size_t include_count,
                                     HeaderDeps *deps, size_t deps_count)
{
    for (size_t i = 0; i < include_count; i++) {
        if (strcmp(includes[i], candidate) == 0) {
            continue;
        }
        if (header_transitively_includes(deps, deps_count,
                                         includes[i], candidate)) {
            return true;
        }
    }
    return false;
}

int cg_module_body_append(ModuleOutput *module, const char *text,
                              size_t length)
{
    if (cg_compile_analyze_only) {
        return 0;
    }
    if (module->body_length + length + 1 > module->body_capacity) {
        size_t needed = module->body_length + length + 1;
        size_t capacity = module->body_capacity ? module->body_capacity * 2 : 256;
        char *grown;

        while (capacity < needed) {
            capacity *= 2;
        }
        grown = realloc(module->body, capacity);
        if (!grown) {
            return -1;
        }
        module->body = grown;
        module->body_capacity = capacity;
    }
    memcpy(module->body + module->body_length, text, length);
    module->body_length += length;
    module->body[module->body_length] = '\0';
    return 0;
}

int cg_module_body_printf(ModuleOutput *module, const char *format, ...)
{
    va_list args;
    char stack[256];
    char *heap = NULL;
    int needed;
    int result = 0;

    va_start(args, format);
    needed = vsnprintf(stack, sizeof(stack), format, args);
    va_end(args);
    if (needed < 0) {
        return -1;
    }
    if ((size_t) needed < sizeof(stack)) {
        return cg_module_body_append(module, stack, (size_t) needed);
    }
    heap = malloc((size_t) needed + 1);
    if (!heap) {
        return -1;
    }
    va_start(args, format);
    vsnprintf(heap, (size_t) needed + 1, format, args);
    va_end(args);
    result = cg_module_body_append(module, heap, (size_t) needed);
    free(heap);
    return result;
}

void cg_free_doc_entry(DocEntry *entry)
{
    free(entry->text);
    *entry = (DocEntry) {0};
}

void cg_clear_doc_attributes(DocAttributes *attributes)
{
    for (size_t i = 0; i < attributes->entry_count; i++) {
        cg_free_doc_entry(&attributes->entries[i]);
    }
    free(attributes->entries);
    *attributes = (DocAttributes) {0};
}

int cg_add_doc_entry(DocAttributes *attributes, DocEntry *entry)
{
    if (attributes->entry_count == attributes->entry_capacity) {
        size_t capacity = attributes->entry_capacity
                              ? attributes->entry_capacity * 2 : 4;
        DocEntry *grown = realloc(attributes->entries,
                                  capacity * sizeof(*attributes->entries));

        if (!grown) {
            return -1;
        }
        attributes->entries = grown;
        attributes->entry_capacity = capacity;
    }
    attributes->entries[attributes->entry_count++] = *entry;
    *entry = (DocEntry) {0};
    return 0;
}

void cg_clear_include_attributes(IncludeAttributes *attributes)
{
    for (size_t i = 0; i < attributes->count; i++) {
        free(attributes->paths[i]);
    }
    free(attributes->paths);
    *attributes = (IncludeAttributes) {0};
}

int cg_add_include_attribute(IncludeAttributes *attributes, char *path)
{
    if (attributes->count == attributes->capacity) {
        size_t capacity = attributes->capacity ? attributes->capacity * 2 : 4;
        char **grown = realloc(attributes->paths, capacity * sizeof(*attributes->paths));

        if (!grown) {
            free(path);
            return -1;
        }
        attributes->paths = grown;
        attributes->capacity = capacity;
    }
    attributes->paths[attributes->count++] = path;
    return 0;
}

static void write_include_directive(FILE *file, const char *path)
{
    size_t length = strlen(path);

    if (length >= 2 && path[0] == '<' && path[length - 1] == '>') {
        fprintf(file, "#include %s\n", path);
        return;
    }
    fprintf(file, "#include \"%s\"\n", path);
}

static int module_has_source_include(ModuleOutput *module, const char *path)
{
    for (size_t i = 0; i < module->source_include_count; i++) {
        if (strcmp(module->source_includes[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}

static int cg_module_add_source_include(ModuleOutput *module, const char *path)
{
    char *copy;

    if (module_has_source_include(module, path)) {
        return 0;
    }
    copy = strdup(path);
    if (!copy) {
        return -1;
    }
    if (module->source_include_count == module->source_include_capacity) {
        size_t next = module->source_include_capacity
                          ? module->source_include_capacity * 2 : 4;
        char **grown = realloc(module->source_includes,
                               next * sizeof(*module->source_includes));

        if (!grown) {
            free(copy);
            return -1;
        }
        module->source_includes = grown;
        module->source_include_capacity = next;
    }
    module->source_includes[module->source_include_count++] = copy;
    if (module->source_file) {
        write_include_directive(module->source_file, path);
    }
    return 0;
}

static void flush_pending_source_includes(ModuleOutput *module)
{
    if (!module->source_file) {
        return;
    }
    for (size_t i = 0; i < module->source_include_count; i++) {
        write_include_directive(module->source_file, module->source_includes[i]);
    }
}

int cg_apply_include_attributes(ModuleOutput *module,
                                const IncludeAttributes *attributes,
                                HeaderDeps **deps, size_t *deps_count,
                                size_t *deps_capacity, bool to_source,
                                char *error, size_t error_size)
{
    for (size_t i = 0; i < attributes->count; i++) {
        const char *path = attributes->paths[i];

        if (to_source || !module->file) {
            if (cg_module_add_source_include(module, path) != 0) {
                snprintf(error, error_size, "out of memory");
                return -1;
            }
            continue;
        }
        if (cg_module_require_include(module, path, deps, deps_count,
                                      deps_capacity) != 0) {
            snprintf(error, error_size, "out of memory");
            return -1;
        }
    }
    return 0;
}

static int append_doc_text(ModuleOutput *output, const char *first_prefix,
                           const char *text)
{
    const char *line = text;
    const char *prefix = first_prefix;

    for (;;) {
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t) (end - line) : strlen(line);

        if (cg_module_body_printf(output, "%s", prefix) != 0) {
            return -1;
        }
        for (size_t i = 0; i < length; i++) {
            if (line[i] == '*' && i + 1 < length && line[i + 1] == '/') {
                if (cg_module_body_printf(output, "* /") != 0) {
                    return -1;
                }
                i++;
            } else if (cg_module_body_append(output, line + i, 1) != 0) {
                return -1;
            }
        }
        if (cg_module_body_printf(output, "\n") != 0) {
            return -1;
        }
        if (!end) {
            return 0;
        }
        line = end + 1;
        prefix = " * ";
    }
}

static int build_doc_comment(const DocAttributes *attributes,
                             char **comment, size_t *comment_length)
{
    ModuleOutput output = {0};

    *comment = NULL;
    *comment_length = 0;
    if (attributes->entry_count == 0) {
        return 0;
    }
    if (cg_module_body_printf(&output, "/**\n") != 0) {
        goto fail;
    }
    for (size_t i = 0; i < attributes->entry_count; i++) {
        const DocEntry *entry = &attributes->entries[i];

        if (i == 1 && cg_module_body_printf(&output, " *\n") != 0) {
            goto fail;
        }
        if (append_doc_text(&output, i == 0 ? " * @brief " : " * ",
                            entry->text) != 0) {
            goto fail;
        }
    }
    if (cg_module_body_printf(&output, " */\n") != 0) {
        goto fail;
    }
    *comment = output.body;
    *comment_length = output.body_length;
    return 0;

fail:
    free(output.body);
    return -1;
}

int cg_emit_struct_macro_doc_comment(ModuleOutput *module,
                                     const DocAttributes *brief_docs,
                                     const char **params,
                                     const char **param_docs,
                                     const bool *param_variadic,
                                     size_t param_count)
{
    ModuleOutput output = {0};
    bool has_brief = brief_docs && brief_docs->entry_count > 0;
    bool has_params = param_count > 0;

    if (!has_brief && !has_params) {
        return 0;
    }
    if (cg_module_body_printf(&output, "/**\n") != 0) {
        goto fail;
    }
    if (has_brief) {
        for (size_t i = 0; i < brief_docs->entry_count; i++) {
            const DocEntry *entry = &brief_docs->entries[i];

            if (i == 1 && cg_module_body_printf(&output, " *\n") != 0) {
                goto fail;
            }
            if (append_doc_text(&output, i == 0 ? " * @brief " : " * ",
                                entry->text) != 0) {
                goto fail;
            }
        }
    }
    if (has_brief && has_params && cg_module_body_printf(&output, " *\n") != 0) {
        goto fail;
    }
    for (size_t i = 0; i < param_count; i++) {
        if (param_variadic && param_variadic[i]) {
            continue;
        }
        if (cg_module_body_printf(&output, " * @param %s", params[i]) != 0) {
            goto fail;
        }
        if (param_docs && param_docs[i] && param_docs[i][0]) {
            const char *line = param_docs[i];
            bool first = true;

            for (;;) {
                const char *end = strchr(line, '\n');
                size_t length = end ? (size_t) (end - line) : strlen(line);

                if (first) {
                    if (length > 0 &&
                        cg_module_body_append(&output, " ", 1) != 0) {
                        goto fail;
                    }
                    first = false;
                } else if (cg_module_body_printf(&output, "\n *   ") != 0) {
                    goto fail;
                }
                for (size_t at = 0; at < length; at++) {
                    if (line[at] == '*' && at + 1 < length &&
                        line[at + 1] == '/') {
                        if (cg_module_body_append(&output, "* /", 3) != 0) {
                            goto fail;
                        }
                        at++;
                    } else if (cg_module_body_append(&output, line + at, 1) !=
                               0) {
                        goto fail;
                    }
                }
                if (!end) {
                    break;
                }
                line = end + 1;
            }
        }
        if (cg_module_body_printf(&output, "\n") != 0) {
            goto fail;
        }
    }
    if (cg_module_body_printf(&output, " */\n") != 0) {
        goto fail;
    }
    if (cg_module_body_append(module, output.body, output.body_length) != 0) {
        goto fail;
    }
    free(output.body);
    return 0;

fail:
    free(output.body);
    return -1;
}

int cg_emit_struct_member_doc_comment(ModuleOutput *module, const char *indent,
                                      const char *doc_text)
{
    ModuleOutput output = {0};

    if (!doc_text || !doc_text[0]) {
        return 0;
    }
    if (cg_module_body_printf(&output, "%s/**\n", indent) != 0) {
        goto fail;
    }
    if (append_doc_text(&output, " * ", doc_text) != 0) {
        goto fail;
    }
    if (cg_module_body_printf(&output, "%s */\n", indent) != 0) {
        goto fail;
    }
    if (cg_module_body_append(module, output.body, output.body_length) != 0) {
        goto fail;
    }
    free(output.body);
    return 0;

fail:
    free(output.body);
    return -1;
}

int cg_append_struct_member_suffix_doc(ModuleOutput *module,
                                       const char *doc_text)
{
    const char *line = doc_text;
    bool first = true;

    if (!doc_text || !doc_text[0]) {
        return 0;
    }
    if (cg_module_body_printf(module, " /**< ") != 0) {
        return -1;
    }
    for (;;) {
        const char *end = strchr(line, '\n');
        size_t length = end ? (size_t) (end - line) : strlen(line);

        if (!first && cg_module_body_append(module, " ", 1) != 0) {
            return -1;
        }
        first = false;
        for (size_t i = 0; i < length; i++) {
            if (line[i] == '*' && i + 1 < length && line[i + 1] == '/') {
                if (cg_module_body_append(module, "* /", 3) != 0) {
                    return -1;
                }
                i++;
            } else if (cg_module_body_append(module, line + i, 1) != 0) {
                return -1;
            }
        }
        if (!end) {
            break;
        }
        line = end + 1;
    }
    if (cg_module_body_printf(module, " */") != 0) {
        return -1;
    }
    return 0;
}

int cg_emit_doc_comment(ModuleOutput *module, FILE *file,
                            const DocAttributes *attributes)
{
    char *comment;
    size_t length;

    if (build_doc_comment(attributes, &comment, &length) != 0) {
        return -1;
    }
    if (!comment) {
        return 0;
    }
    if (file) {
        fwrite(comment, 1, length, file);
    } else if (cg_module_body_append(module, comment, length) != 0) {
        free(comment);
        return -1;
    }
    free(comment);
    return 0;
}

int cg_write_package_readme(const Block *block, char *error,
                                size_t error_size)
{
    FILE *file;
    char *directory;
    const char *slash;

    if (cg_compile_analyze_only) {
        return 0;
    }
    if (block->docs.entry_count == 0 || !block->readme_path ||
        block->internal) {
        return 0;
    }
    slash = strrchr(block->readme_path, '/');
    if (!slash) {
        cg_set_error(error, error_size, "invalid package readme path");
        return -1;
    }
    directory = cg_copy_text(block->readme_path,
                        (size_t) (slash - block->readme_path));
    if (!directory) {
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    if (platform_mkdir_p(directory, error, error_size) != 0) {
        free(directory);
        return -1;
    }
    free(directory);
    file = fopen(block->readme_path, "w");
    if (!file) {
        cg_set_error(error, error_size, "%s: %s",
                  block->readme_path, strerror(errno));
        return -1;
    }
    for (size_t i = 0; i < block->docs.entry_count; i++) {
        const char *line = block->docs.entries[i].text;

        if (i > 0) {
            fputc('\n', file);
        }
        for (;;) {
            const char *end = strchr(line, '\n');
            size_t length = end ? (size_t) (end - line) : strlen(line);

            fwrite(line, 1, length, file);
            fputc('\n', file);
            if (!end) {
                break;
            }
            line = end + 1;
        }
    }
    if (fclose(file) != 0) {
        cg_set_error(error, error_size, "%s: %s",
                  block->readme_path, strerror(errno));
        return -1;
    }
    return 0;
}

void cg_free_block(Block *block)
{
    cg_clear_doc_attributes(&block->docs);
    free(block->readme_path);
    free(block->name);
}

int cg_pop_block(Block *blocks, size_t *block_count, ModuleOutput *module,
                     HeaderDeps *deps, size_t deps_count,
                     const char *format_style, char *error,
                     size_t error_size)
{
    Block *block;

    if (*block_count == 0) {
        return 0;
    }
    block = &blocks[*block_count - 1];
    if (block->kind == BLOCK_MODULE) {
        if (cg_close_module(module, deps, deps_count, format_style, error,
                         error_size) != 0) {
            return -1;
        }
    } else if (block->kind == BLOCK_PACKAGE) {
        if (cg_write_package_readme(block, error, error_size) != 0) {
            return -1;
        }
    }
    cg_free_block(block);
    (*block_count)--;
    return 0;
}

int cg_module_require_include(ModuleOutput *module, const char *header,
                                  HeaderDeps **deps, size_t *deps_count,
                                  size_t *deps_capacity)
{
    if (!module->relative_header ||
        strcmp(header, module->relative_header) == 0) {
        return 0;
    }
    for (size_t i = 0; i < module->include_count; i++) {
        if (strcmp(module->includes[i], header) == 0) {
            return 0;
        }
    }
    if (module->include_count == module->include_capacity) {
        size_t next = module->include_capacity ? module->include_capacity * 2 : 4;
        char **grown = realloc(module->includes, next * sizeof(*module->includes));

        if (!grown) {
            return -1;
        }
        module->includes = grown;
        module->include_capacity = next;
    }
    module->includes[module->include_count] = strdup(header);
    if (!module->includes[module->include_count]) {
        return -1;
    }
    module->include_count++;
    return record_header_include(deps, deps_count, deps_capacity,
                                 module->relative_header, header);
}

void cg_flush_module_blank_lines(ModuleOutput *module)
{
    while (module->pending_blank_lines > 0) {
        if (cg_module_body_printf(module, "\n") != 0) {
            return;
        }
        module->pending_blank_lines--;
    }
}

int cg_emit_preprocessor(ModuleOutput *module, const char *directive,
                         char *error, size_t error_size)
{
    if (cg_ensure_module_header(module, error, error_size) != 0) {
        return -1;
    }
    cg_flush_module_blank_lines(module);
    if (cg_module_body_printf(module, "#%s\n", directive) != 0) {
        cg_set_error(error, error_size, "out of memory");
        return -1;
    }
    module->header_has_declaration = true;
    return 0;
}

int cg_close_if_frames(IfFrame **frames, size_t *depth, size_t until_indent,
                       ModuleOutput *module, char *error, size_t error_size)
{
    while (*depth > 0 && (*frames)[*depth - 1].indent >= until_indent) {
        (*depth)--;
        if (cg_emit_preprocessor(module, "endif", error, error_size) != 0) {
            return -1;
        }
    }
    return 0;
}

static char *make_guard(Block *blocks, size_t count)
{
    size_t length = sizeof("_H");
    char *guard;
    size_t at = 0;

    for (size_t i = 0; i < count; i++) {
        length += strlen(blocks[i].name) + 1;
    }
    guard = malloc(length);
    if (!guard) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; blocks[i].name[j]; j++) {
            guard[at++] = (char) toupper((unsigned char) blocks[i].name[j]);
        }
        guard[at++] = '_';
    }
    guard[at++] = 'H';
    guard[at] = '\0';
    return guard;
}

int cg_close_module(ModuleOutput *module, HeaderDeps *deps,
                        size_t deps_count, const char *format_style,
                        char *error, size_t error_size)
{
    bool header_created = module->file != NULL;
    bool source_created = module->source_file != NULL;
    int result = 0;

    if (module->file) {
        char *comment;
        size_t comment_length;

        if (build_doc_comment(&module->module_docs, &comment,
                              &comment_length) != 0) {
            result = -1;
            goto cleanup;
        }
        if (comment) {
            fwrite(comment, 1, comment_length, module->file);
            free(comment);
        }
        fprintf(module->file, "#ifndef %s\n#define %s\n\n",
                module->guard, module->guard);
        for (size_t i = 0; i < module->include_count; i++) {
            const char *header = module->includes[i];

            if (include_covered_by_other(header, module->includes,
                                         module->include_count,
                                         deps, deps_count)) {
                continue;
            }
            write_include_directive(module->file, header);
        }
        if (module->include_count > 0 && module->body_length > 0) {
            fputc('\n', module->file);
        }
        if (module->body_length > 0) {
            fwrite(module->body, 1, module->body_length, module->file);
        }
        fprintf(module->file, "\n#endif /* %s */\n", module->guard);
        if (fclose(module->file) != 0) {
            cg_set_error(error, error_size, "%s: %s",
                      module->path, strerror(errno));
            result = -1;
        }
    }
    if (module->source_file) {
        if (fclose(module->source_file) != 0 && result == 0) {
            cg_set_error(error, error_size, "%s: %s",
                      module->source_path, strerror(errno));
            result = -1;
        }
    }
    if (format_style && result == 0 && header_created &&
        platform_clang_format(module->path, format_style) != 0) {
        cg_set_error(error, error_size, "clang-format failed for %s",
                  module->path);
        result = -1;
    }
    if (format_style && result == 0 && source_created &&
        platform_clang_format(module->source_path, format_style) != 0) {
        cg_set_error(error, error_size, "clang-format failed for %s",
                  module->source_path);
        result = -1;
    }
cleanup:
    cg_clear_doc_attributes(&module->module_docs);
    for (size_t i = 0; i < module->include_count; i++) {
        free(module->includes[i]);
    }
    free(module->includes);
    for (size_t i = 0; i < module->source_include_count; i++) {
        free(module->source_includes[i]);
    }
    free(module->source_includes);
    free(module->body);
    free(module->path);
    free(module->source_path);
    free(module->source_directory);
    free(module->relative_header);
    free(module->guard);
    *module = (ModuleOutput) {0};
    return result;
}

void cg_free_header_deps(HeaderDeps *deps, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free(deps[i].header);
        for (size_t j = 0; j < deps[i].include_count; j++) {
            free(deps[i].includes[j]);
        }
        free(deps[i].includes);
    }
    free(deps);
}

int cg_ensure_module_source(ModuleOutput *module, char *error,
                                size_t error_size)
{
    if (cg_compile_analyze_only) {
        return 0;
    }
    if (module->source_file) {
        return 0;
    }
    if (platform_mkdir_p(module->source_directory, error, error_size) != 0) {
        return -1;
    }
    module->source_file = fopen(module->source_path, "w");
    if (!module->source_file) {
        cg_set_error(error, error_size, "%s: %s",
                  module->source_path, strerror(errno));
        return -1;
    }
    fprintf(module->source_file, "#include \"%s\"\n\n",
            module->relative_header);
    flush_pending_source_includes(module);
    return 0;
}

int cg_ensure_module_header(ModuleOutput *module, char *error,
                                size_t error_size)
{
    if (module->file) {
        return 0;
    }
    module->file = fopen(module->path, "w");
    if (!module->file) {
        cg_set_error(error, error_size, "%s: %s",
                  module->path, strerror(errno));
        return -1;
    }
    return 0;
}

int cg_open_module(ModuleOutput *module, const char *include_path,
                       const char *source_path, Block *blocks, size_t count,
                       char *error, size_t error_size)
{
    char *directory = cg_build_package_path(include_path, blocks, count - 1);
    char *source_directory = cg_build_package_path(source_path, blocks, count - 1);
    char *relative_directory = cg_build_package_path("", blocks, count - 1);
    char *filename = NULL;
    char *source_filename = NULL;

    if (!directory || !source_directory || !relative_directory) {
        cg_set_error(error, error_size, "out of memory");
        goto fail;
    }
    if (!cg_compile_analyze_only &&
        platform_mkdir_p(directory, error, error_size) != 0) {
        goto fail;
    }
    size_t name_length = strlen(blocks[count - 1].name);
    filename = malloc(name_length + 3);
    if (!filename) {
        cg_set_error(error, error_size, "out of memory");
        goto fail;
    }
    snprintf(filename, name_length + 3, "%s.h", blocks[count - 1].name);
    source_filename = malloc(name_length + 3);
    if (!source_filename) {
        cg_set_error(error, error_size, "out of memory");
        goto fail;
    }
    snprintf(source_filename, name_length + 3, "%s.c",
             blocks[count - 1].name);
    module->path = cg_join_path(directory, filename);
    module->source_path = cg_join_path(source_directory, source_filename);
    module->source_directory = source_directory;
    source_directory = NULL;
    module->relative_header = cg_join_path(relative_directory, filename);
    module->guard = make_guard(blocks, count);
    if (!module->path || !module->source_path ||
        !module->relative_header || !module->guard) {
        cg_set_error(error, error_size, "out of memory");
        goto fail;
    }
    if (!cg_compile_analyze_only) {
        if (remove(module->source_path) != 0 && errno != ENOENT) {
            cg_set_error(error, error_size, "%s: %s",
                      module->source_path, strerror(errno));
            goto fail;
        }
        if (!blocks[count - 1].internal) {
            module->file = fopen(module->path, "w");
            if (!module->file) {
                cg_set_error(error, error_size, "%s: %s",
                          module->path, strerror(errno));
                goto fail;
            }
        }
    }
    free(directory);
    free(source_directory);
    free(relative_directory);
    free(filename);
    free(source_filename);
    return 0;

fail:
    free(directory);
    free(source_directory);
    free(relative_directory);
    free(filename);
    free(source_filename);
    cg_close_module(module, NULL, 0, NULL, NULL, 0);
    return -1;
}
