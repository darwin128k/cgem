#include "lh/version.h"

lh_version_major_t lh_version_get_major(const lh_version_t *self)
{
    return self->major;
}
lh_version_minor_t lh_version_get_minor(const lh_version_t *self)
{
    return self->minor;
}
lh_version_patch_t lh_version_get_patch(const lh_version_t *self)
{
    return self->patch;
}
void lh_version_pack(lh_version_t *self,
                     const lh_version_major_t *major,
                     const lh_version_minor_t *minor,
                     const lh_version_patch_t *patch)
{
    if (major)
        self->major = *major;
    if (minor)
        self->minor = *minor;
    if (patch)
        self->patch = *patch;
}
void lh_version_unpack(const lh_version_t *self,
                       lh_version_major_t *major,
                       lh_version_minor_t *minor,
                       lh_version_patch_t *patch)
{
    if (major)
        *major = self->major;
    if (minor)
        *minor = self->minor;
    if (patch)
        *patch = self->patch;
}
void lh_version_set_major(lh_version_t *self, const lh_version_major_t value)
{
    self->major = value;
}
void lh_version_set_minor(lh_version_t *self, const lh_version_minor_t value)
{
    self->minor = value;
}
void lh_version_set_patch(lh_version_t *self, const lh_version_patch_t value)
{
    self->patch = value;
}
void lh_version_set(lh_version_t *self,
                    const lh_version_major_t major,
                    const lh_version_minor_t minor,
                    const lh_version_patch_t patch)
{
    lh_version_set_major(self, major);
    lh_version_set_minor(self, minor);
    lh_version_set_patch(self, patch);
}
void lh_version_init(lh_version_t *self,
                     const lh_version_major_t major,
                     const lh_version_minor_t minor,
                     const lh_version_patch_t patch)
{
    lh_version_set(self, major, minor, patch);
}
