#include "cgem/core/project.h"

#define CGEM_PROJECT_TYPE "project"

cgem_project_t *cgem_project_new(const cgem_char_t *name, cgem_node_t *owner)
{
    return cgem_package_new_with_type(name, CGEM_PROJECT_TYPE, owner);
}
