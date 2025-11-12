/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: memcpy_s stub function
 * Create: 2024-09-27
 */

#include <stdlib.h>
#include "securec.h"

errno_t strncpy_s(char *strDest, size_t destMax, const char *strSrc, size_t count)
{
    strncpy(strDest, strSrc, count);
    return 0;
}