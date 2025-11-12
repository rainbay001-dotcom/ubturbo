/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: securec stub
 * Create: 2024-09-18
 */

#ifndef SECUREC_H_5D13A042_DC3F_4ED9_A8D1_882811274C27
#define SECUREC_H_5D13A042_DC3F_4ED9_A8D1_882811274C27

#include <stdarg.h>
#include <stddef.h>


#ifndef errno_t
typedef int errno_t;
#endif

#ifndef EOK
#define EOK 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

errno_t memset_s(void *dest, size_t destMax, int c, size_t count);

int snprintf_s(char *strDest, unsigned long destMax, unsigned long count, const char *format, ...);

int vsnprintf_s(char *strDest, size_t destMax, size_t count, const char *format, va_list argList);

int sscanf_s(const char *buffer, const char *format, ...);

errno_t memcpy_s(void *dest, size_t numberOfElements, const void *src, size_t count);

errno_t strncpy_s(char *strDest, size_t destMax, const char *strSrc, size_t count);

errno_t strcpy_s(char *strDest, size_t destMax, const char *strSrc);

#ifdef __cplusplus
}
#endif

#endif /* SECUREC_H_5D13A042_DC3F_4ED9_A8D1_882811274C27 */
