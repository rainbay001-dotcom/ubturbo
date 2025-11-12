/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: strcpy_s stub function
 * Create: 2025-09-22
 */

#include <stdlib.h>
#include "securec.h"

errno_t strcpy_s(char *strDest, size_t destMax, const char *strSrc)
{
    strcpy(strDest, strSrc);
    return 0;
}