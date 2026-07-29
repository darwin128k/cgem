#ifndef CGEM_PROJECT_H
#define CGEM_PROJECT_H

#include "cgem/core/package.h"
#include "cgem/core/primitive.h"

typedef cgem_package_t cgem_project_t;

cgem_project_t *cgem_project_new(const cgem_char_t *name, cgem_node_t *owner);

#endif
