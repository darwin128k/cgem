#ifndef CGEM_PROJECT_H
#define CGEM_PROJECT_H

#include "cgem/core/package.h"

typedef cgem_package_t cgem_project_t;

cgem_project_t *cgem_project_new(const char *name, cgem_node_t *owner);

#endif
