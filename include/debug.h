// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_DEBUG_H
#define JEEFS_DEBUG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#define debug(fmt, ...)                                                        \
  printf("[D!] %s:%i: " fmt, __FILE__, __LINE__, ##__VA_ARGS__);               \
  fflush(stdout);
#else
// #define debug(fmt, ...) printf("[D] %s:%i: " fmt, __FILE__, __LINE__,
// ##__VA_ARGS__)
#define debug(...)
#endif

#ifdef __cplusplus
}
#endif

#endif // JEEFS_DEBUG_H
