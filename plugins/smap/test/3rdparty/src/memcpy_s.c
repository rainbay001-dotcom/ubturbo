/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: memcpy_s stub function
 * Create: 2024-09-27
 */

#include <stdlib.h>
#include "securec.h"

errno_t memcpy_s(void *dest, size_t numberOfElements, const void *src, size_t count)
{
    memcpy(dest, src, count);
    return 0;
}
