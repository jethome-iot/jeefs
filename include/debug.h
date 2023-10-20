// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_DEBUG_H
#define JEEFS_DEBUG_H

#include <stdio.h>

#if DEBUG==1
#define debug(fmt, ...) printf("[D!] " fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...) printf("[D] %s:%i: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
//#define debug(...)
#endif


#endif //JEEFS_DEBUG_H
