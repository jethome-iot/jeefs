// DO NOT EDIT — maintained by tools/sync_version.py from version.json
// SPDX-License-Identifier: (GPL-2.0+ or MIT)

#ifndef JEEFS_VERSION_H
#define JEEFS_VERSION_H

#define JEEFS_VERSION "0.1.2"
#define JEEFS_VERSION_MAJOR 0
#define JEEFS_VERSION_MINOR 1
#define JEEFS_VERSION_PATCH 2

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *jeefs_version(void) { return JEEFS_VERSION; }

#ifdef __cplusplus
}
#endif

#endif // JEEFS_VERSION_H
