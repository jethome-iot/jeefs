// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 */

#ifndef JEEFS_DEBUG_H
#define JEEFS_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * JEEFS_LOG(fmt, ...) is the overridable log sink (#24): define it before
 * including this header (or on the compiler command line) to route
 * diagnostics into the environment's logger — printk, U-Boot printf, a
 * HAL trace macro. The default is hosted printf, pulled in only when
 * logging is actually enabled, so freestanding builds without DEBUG never
 * see <stdio.h>.
 */
#ifdef DEBUG
#ifndef JEEFS_LOG
#include <stdio.h>
#define JEEFS_LOG(fmt, ...)                                                                                            \
    do {                                                                                                               \
        printf(fmt, ##__VA_ARGS__);                                                                                    \
        fflush(stdout);                                                                                                \
    } while (0)
#endif
#define debug(fmt, ...) JEEFS_LOG("[D!] %s:%i: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define debug(...)                                                                                                     \
    do {                                                                                                               \
    } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif // JEEFS_DEBUG_H
