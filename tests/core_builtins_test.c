/* A persistent, buildable proof that cgem's new core + the real "c"
 * generator plugin can actually describe cgem's own numeric types the way
 * the design has been aiming at all along:
 *
 *     package c:
 *       struct uchar:
 *         field value as c.char
 *         fn add(other as uchar) returns uchar:
 *           return self.value.add(other.value)
 *
 * The tree is built entirely by the real pipeline -- cgem_generator_load
 * on the actual cgem_generator_c.so, cgem_generator_import_types for the
 * raw primitives, cgem_generator_bootstrap_types for "uchar" -- so this
 * test only verifies the shape; it does not construct anything itself.
 * There is no DSL parser for the new core yet, so cgem_generator_c.so's
 * bootstrap_types hook is the stand-in for what will eventually come from
 * a real .cgem source file. */

#include "cgem/compiler_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);  \
            return 1;                                                        \
        }                                                                    \
    } while (0)

int main(void)
{
    cgem_attributes_t *config = cgem_attributes_new();
    cgem_char_t error[256];
    cgem_generator_t *generator;
    cgem_node_t *root;
    cgem_package_t *pkg_c;
    cgem_node_t *uchar;
    cgem_node_t *add_fn;

    CHECK(config != NULL, "config alloc");
    CHECK(cgem_attributes_add(
              config, cgem_attribute_new(
                          "c.compiler", cgem_attribute_value_new_string("cc"))),
         "config set compiler");

    generator = cgem_generator_load(CGEM_TEST_GENERATOR_C_PATH, config, error,
                                    sizeof(error));
    if (!generator) {
        fprintf(stderr, "FAIL: generator load: %s\n", error);
        return 1;
    }

    root = cgem_node_new(NULL);
    pkg_c = cgem_package_new("c", root);
    CHECK(cgem_node_add(root, pkg_c), "add package c");

    CHECK(cgem_generator_import_types(generator, pkg_c), "import raw types");
    {
        cgem_char_t bootstrap_error[256];

        if (!cgem_generator_bootstrap_types(generator, pkg_c, bootstrap_error,
                                            sizeof(bootstrap_error))) {
            fprintf(stderr, "FAIL: bootstrap_types: %s\n", bootstrap_error);
            return 1;
        }
    }

    /* raw primitives are real, target-queried, resolvable */
    CHECK(cgem_resolve(root, "c.int") != NULL, "resolve c.int");
    CHECK(cgem_type_get_size(cgem_resolve(root, "c.char")) == 1,
         "c.char size == 1");
    CHECK(cgem_type_get_size(cgem_resolve(root, "c.int")) == sizeof(int),
         "c.int size == sizeof(int)");

    /* the wrapper struct is real and resolvable too */
    uchar = cgem_resolve(root, "c.uchar");
    CHECK(uchar != NULL, "resolve c.uchar");
    CHECK(cgem_resolve(root, "c.uchar.value") != NULL,
         "resolve c.uchar.value");

    add_fn = cgem_resolve(root, "c.uchar.add");
    CHECK(add_fn != NULL, "resolve c.uchar.add");
    CHECK(cgem_fn_get_param_count(add_fn) == 1, "add() has 1 param");
    CHECK(cgem_fn_get_statement_count(add_fn) == 1, "add() has 1 statement");

    {
        const cgem_attribute_value_t *return_type =
            cgem_fn_get_return_type(add_fn);
        cgem_node_t *resolved =
            cgem_resolve(root, cgem_attribute_value_get_string(return_type));

        CHECK(resolved == uchar, "add() returns uchar");
    }

    {
        cgem_node_t *stmt = cgem_fn_get_statement(add_fn, 0);
        cgem_node_t *call_expr = cgem_return_get_value(stmt);

        CHECK(call_expr != NULL, "return has a value");
        CHECK(strcmp(cgem_call_get_callee(call_expr), "self.value.add") == 0,
             "call callee is self.value.add");
        CHECK(cgem_node_get_count(call_expr) == 1, "call has 1 arg");
        CHECK(strcmp(cgem_ref_get_path(cgem_node_get(call_expr, 0)),
                     "other.value") == 0,
             "call arg is other.value");
    }

    cgem_node_free(root);
    cgem_generator_free(generator);
    cgem_attributes_free(config);

    printf("core_builtins_test: OK\n");
    return 0;
}
