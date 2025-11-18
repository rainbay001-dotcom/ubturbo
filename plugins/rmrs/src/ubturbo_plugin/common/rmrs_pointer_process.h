/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */


#ifndef RMRS_MANAGER_POINTER_PROCESS_H
#define RMRS_MANAGER_POINTER_PROCESS_H

#include "rmrs_config.h"
#include "rmrs_error.h"
#include "turbo_logger.h"

namespace rmrs {
using namespace turbo::log;


template <typename T> void SafeDeleteArray(T *&ptr)
{
    if (ptr) {
        delete[] ptr;
        ptr = nullptr;
    }
}

template <typename T>
bool SafeAdd(T a, T b, T& result)
{
    const auto ret = __builtin_add_overflow(a, b, &result);
    if (ret) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Add would overflow.";
    }
    return ret;
}

template <typename T>
bool SafeSub(T a, T b, T& result)
{
    const auto ret = __builtin_sub_overflow(a, b, &result);
    if (ret) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Sub would overflow.";
    }
    return ret;
}

template <typename T>
bool SafeMul(T a, T b, T& result)
{
    const auto ret = __builtin_mul_overflow(a, b, &result);
    if (ret) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Multiplication would overflow.";
    }
    return ret;
}
template <typename T> void SafeDeleteArray(T *&ptr, size_t ptrLen)
{
    if (ptr && ptrLen != 0) {
        delete[] ptr;
        ptr = nullptr;
    }
}
}
#endif // RMRS_MANAGER_POINTER_PROCESS_H
