#ifndef LH_VERSION_FIELDS_H
#define LH_VERSION_FIELDS_H

/**
 * @param major_type Major version number.
 * @param minor_type Minor version number.
 * @param patch_type Patch version number.
 */
#define lh_version_fields(major_type, minor_type, patch_type)                  \
    major_type major;                                                          \
    minor_type minor;                                                          \
    patch_type patch

#endif /* LH_VERSION_FIELDS_H */
