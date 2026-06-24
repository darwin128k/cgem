#ifndef LH_VERSION_H
#define LH_VERSION_H

#include "lh/version/fields.h"
#include "lh/version/major.h"
#include "lh/version/minor.h"
#include "lh/version/patch.h"

typedef struct lh_version
{
    lh_version_fields(lh_version_major_t,
                      lh_version_minor_t,
                      lh_version_patch_t);
} lh_version_t;

lh_version_major_t lh_version_get_major(const lh_version_t *self);
lh_version_minor_t lh_version_get_minor(const lh_version_t *self);
lh_version_patch_t lh_version_get_patch(const lh_version_t *self);
void lh_version_pack(lh_version_t *self,
                     const lh_version_major_t *major,
                     const lh_version_minor_t *minor,
                     const lh_version_patch_t *patch);
void lh_version_unpack(const lh_version_t *self,
                       lh_version_major_t *major,
                       lh_version_minor_t *minor,
                       lh_version_patch_t *patch);
void lh_version_set_major(lh_version_t *self, const lh_version_major_t value);
void lh_version_set_minor(lh_version_t *self, const lh_version_minor_t value);
void lh_version_set_patch(lh_version_t *self, const lh_version_patch_t value);
void lh_version_set(lh_version_t *self,
                    const lh_version_major_t major,
                    const lh_version_minor_t minor,
                    const lh_version_patch_t patch);
void lh_version_init(lh_version_t *self,
                     const lh_version_major_t major,
                     const lh_version_minor_t minor,
                     const lh_version_patch_t patch);
static inline lh_version_t lh_version(const lh_version_major_t major,
                                      const lh_version_minor_t minor,
                                      const lh_version_patch_t patch)
{
    lh_version_t result;
    lh_version_init(&result, major, minor, patch);
    return result;
}

#endif /* LH_VERSION_H */
