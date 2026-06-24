#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgem/common.h"
#include "cgem/compiler.h"
#include "cgem/format.h"
#ifndef CGEM_NO_IDE
#include "cgem/ide.h"
#endif
#include "cgem/platform.h"

typedef struct {
    const char **input_paths;
    size_t input_count;
    const char *include_path;
    const char *source_path;
    const char *compiler;
    bool generate_mode;
    bool format_mode;
    bool ide_mode;
    bool clean_output;
} CliOptions;

static bool parse_bool_argument(const char *value, bool *result)
{
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
        strcmp(value, "yes") == 0) {
        *result = true;
        return true;
    }
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0 ||
        strcmp(value, "no") == 0) {
        *result = false;
        return true;
    }
    return false;
}

static bool has_cgem_extension(const char *path)
{
    size_t length = strlen(path);

    return length >= 5 && strcmp(path + length - 5, ".cgem") == 0;
}

typedef struct {
    FILE *output;
    char *error;
    size_t error_size;
} AppendContext;

static bool append_input_path(const char *path, void *context);

static bool append_directory_entry(const char *path, void *context)
{
    AppendContext *ctx = context;

    if (platform_path_is_directory(path) ||
        has_cgem_extension(path)) {
        return append_input_path(path, ctx);
    }
    return true;
}

static bool append_input_path(const char *path, void *context)
{
    AppendContext *ctx = context;

    if (!platform_path_exists(path)) {
        snprintf(ctx->error, ctx->error_size, "%s: %s", path, strerror(errno));
        return false;
    }
    if (platform_path_is_directory(path)) {
        return platform_scan_directory(path, append_directory_entry, ctx);
    }
    if (!platform_path_is_regular_file(path)) {
        snprintf(ctx->error, ctx->error_size, "%s: not a regular file", path);
        return false;
    }

    FILE *input = fopen(path, "r");
    char buffer[4096];
    size_t length;
    int last = EOF;

    if (!input) {
        snprintf(ctx->error, ctx->error_size, "%s: %s", path, strerror(errno));
        return false;
    }
    while ((length = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, length, ctx->output) != length) {
            snprintf(ctx->error, ctx->error_size, "cannot prepare input");
            fclose(input);
            return false;
        }
        last = (unsigned char) buffer[length - 1];
    }
    if (ferror(input)) {
        snprintf(ctx->error, ctx->error_size, "%s: read failed", path);
        fclose(input);
        return false;
    }
    fclose(input);
    if (last != EOF && last != '\n' && fputc('\n', ctx->output) == EOF) {
        snprintf(ctx->error, ctx->error_size, "cannot prepare input");
        return false;
    }
    return true;
}

static bool append_input_path_to_file(FILE *output, const char *path,
                                      char *error, size_t error_size)
{
    AppendContext ctx = {
        .output = output,
        .error = error,
        .error_size = error_size
    };

    return append_input_path(path, &ctx);
}

static bool format_directory_entry(const char *path, void *context);

static bool format_input_path(const char *path, char *error, size_t error_size)
{
    typedef struct {
        char *error;
        size_t error_size;
    } FormatContext;
    FormatContext ctx;

    if (!platform_path_exists(path)) {
        snprintf(error, error_size, "%s: %s", path, strerror(errno));
        return false;
    }
    if (platform_path_is_directory(path)) {
        ctx.error = error;
        ctx.error_size = error_size;
        return platform_scan_directory(path, format_directory_entry, &ctx);
    }
    if (!platform_path_is_regular_file(path)) {
        snprintf(error, error_size, "%s: not a regular file", path);
        return false;
    }
    if (!has_cgem_extension(path)) {
        snprintf(error, error_size, "%s: expected a .cgem file", path);
        return false;
    }
    if (cg_format_path(path, error, error_size) != 0) {
        return false;
    }
    return true;
}

static bool format_directory_entry(const char *path, void *context)
{
    typedef struct {
        char *error;
        size_t error_size;
    } FormatContext;
    FormatContext *ctx = context;

    if (platform_path_is_directory(path) || has_cgem_extension(path)) {
        return format_input_path(path, ctx->error, ctx->error_size);
    }
    return true;
}

static int format_input_paths(const CliOptions *options)
{
    char error[512];

    for (size_t i = 0; i < options->input_count; i++) {
        if (!format_input_path(options->input_paths[i], error, sizeof(error))) {
            fprintf(stderr, "cgem: %s\n", error);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

static int compile_input_paths(const CliOptions *options)
{
    FILE *input = tmpfile();
    char error[512];
    char warning[1024];

    if (options->format_mode) {
        int format_result = format_input_paths(options);

        if (format_result != EXIT_SUCCESS) {
            return format_result;
        }
    }

    if (!input) {
        fprintf(stderr, "cgem: cannot prepare input: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < options->input_count; i++) {
        if (!append_input_path_to_file(input, options->input_paths[i],
                                       error, sizeof(error))) {
            fprintf(stderr, "cgem: %s\n", error);
            fclose(input);
            return EXIT_FAILURE;
        }
    }
    if (fflush(input) != 0 || fseek(input, 0, SEEK_SET) != 0) {
        fprintf(stderr, "cgem: cannot prepare input\n");
        fclose(input);
        return EXIT_FAILURE;
    }
    if (cgem_compile(input, options->include_path, options->source_path,
                     cgem_generated_output_style(options->include_path,
                                                 options->source_path),
                     options->compiler, options->clean_output,
                     warning, sizeof(warning),
                     error, sizeof(error),
                     NULL) != 0) {
        fprintf(stderr, "cgem: %s\n", error);
        fclose(input);
        return EXIT_FAILURE;
    }
    fclose(input);
    if (warning[0]) {
        const char *line = warning;

        while (*line) {
            const char *end = strchr(line, '\n');
            size_t length = end ? (size_t) (end - line) : strlen(line);

            fprintf(stderr, "cgem: warning: %.*s\n", (int) length, line);
            if (!end) {
                break;
            }
            line = end + 1;
        }
    }
    return EXIT_SUCCESS;
}

static void print_usage(const char *program)
{
#ifndef CGEM_NO_IDE
    fprintf(stderr,
            "usage: %s -g -i <input> [-i <input> ...] "
            "-I <include-path> -s <source-path> -c <compiler> [--ide]\n"
            "       [--clean-output true|false]\n"
            "       %s -f -i <input> [-i <input> ...]\n"
            "       %s --generate --input <input> "
            "--include <include-path> --source <source-path> "
            "--compiler <compiler> [--ide]\n"
            "       [--clean-output true|false]\n"
            "       %s --format --input <input>\n",
            program, program, program, program);
#else
    fprintf(stderr,
            "usage: %s -g -i <input> [-i <input> ...] "
            "-I <include-path> -s <source-path> -c <compiler>\n"
            "       [--clean-output true|false]\n"
            "       %s -f -i <input> [-i <input> ...]\n"
            "       %s --generate --input <input> "
            "--include <include-path> --source <source-path> "
            "--compiler <compiler>\n"
            "       [--clean-output true|false]\n"
            "       %s --format --input <input>\n",
            program, program, program, program);
#endif
}

static bool parse_arguments(int argc, char **argv, CliOptions *options)
{
    options->input_paths = calloc((size_t) argc, sizeof(*options->input_paths));
    if (!options->input_paths) {
        fprintf(stderr, "cgem: out of memory\n");
        return false;
    }

    for (int at = 1; at < argc; at++) {
        const char *argument = argv[at];
        const char **destination = NULL;
        const char *name = NULL;

        if (strcmp(argument, "-g") == 0 ||
            strcmp(argument, "--generate") == 0) {
            if (options->generate_mode) {
                fprintf(stderr, "cgem: --generate specified more than once\n");
                return false;
            }
            options->generate_mode = true;
            continue;
        } else if (strcmp(argument, "-f") == 0 ||
                   strcmp(argument, "--format") == 0) {
            if (options->format_mode) {
                fprintf(stderr, "cgem: --format specified more than once\n");
                return false;
            }
            options->format_mode = true;
            continue;
        } else if (strcmp(argument, "-i") == 0 ||
                   strcmp(argument, "--input") == 0) {
            if (++at >= argc) {
                fprintf(stderr, "cgem: --input requires a path\n");
                return false;
            }
            options->input_paths[options->input_count++] = argv[at];
            continue;
        } else if (strcmp(argument, "-I") == 0 ||
                   strcmp(argument, "--include") == 0) {
            destination = &options->include_path;
            name = "--include";
        } else if (strcmp(argument, "-s") == 0 ||
                   strcmp(argument, "--source") == 0) {
            destination = &options->source_path;
            name = "--source";
        } else if (strcmp(argument, "-c") == 0 ||
                   strcmp(argument, "--compiler") == 0) {
            destination = &options->compiler;
            name = "--compiler";
        } else if (strcmp(argument, "--ide") == 0) {
#ifdef CGEM_NO_IDE
            fprintf(stderr,
                    "cgem: this build was compiled without IDE support\n");
            return false;
#else
            if (options->ide_mode) {
                fprintf(stderr, "cgem: --ide specified more than once\n");
                return false;
            }
            options->ide_mode = true;
            continue;
#endif
        } else if (strcmp(argument, "--clean-output") == 0) {
            if (at + 1 < argc &&
                parse_bool_argument(argv[at + 1], &options->clean_output)) {
                at++;
            } else {
                options->clean_output = true;
            }
            continue;
        } else {
            fprintf(stderr, "cgem: unknown argument: %s\n", argument);
            return false;
        }

        if (*destination) {
            fprintf(stderr, "cgem: %s specified more than once\n", name);
            return false;
        }
        if (++at >= argc) {
            fprintf(stderr, "cgem: %s requires a path\n", name);
            return false;
        }
        *destination = argv[at];
    }

    if (!options->generate_mode && !options->format_mode) {
        fprintf(stderr, "cgem: --format or --generate is required\n");
        return false;
    }
    if (options->generate_mode &&
        (!options->include_path || !options->source_path ||
         !options->compiler)) {
        fprintf(stderr,
                "cgem: --generate requires --include, --source, and --compiler\n");
        return false;
    }
    if (options->ide_mode && !options->compiler) {
        fprintf(stderr, "cgem: --ide requires --compiler\n");
        return false;
    }
    if (!options->ide_mode && !options->input_count) {
        fprintf(stderr, "cgem: at least one --input path is required\n");
        return false;
    }
    if (options->ide_mode && options->input_count > 1) {
        fprintf(stderr,
                "cgem: --ide accepts at most one --input path\n");
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    CliOptions options = {.clean_output = true};
    int result;

    setlocale(LC_CTYPE, "");

    if (!parse_arguments(argc, argv, &options)) {
        print_usage(argv[0]);
        free(options.input_paths);
        return EXIT_FAILURE;
    }
    if (!options.ide_mode) {
        if (options.generate_mode) {
            result = compile_input_paths(&options);
        } else {
            result = format_input_paths(&options);
        }
        free(options.input_paths);
        return result;
    }

#ifdef CGEM_NO_IDE
    fprintf(stderr, "cgem: this build was compiled without IDE support\n");
    free(options.input_paths);
    return EXIT_FAILURE;
#else
    result = ide_run(options.input_count ? options.input_paths[0] : NULL,
                     options.include_path, options.source_path,
                     options.compiler);
    free(options.input_paths);
    return result;
#endif
}
