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
#ifndef TURBO_ERROR_H
#define TURBO_ERROR_H
#include <cstdint>

/* 各个子系统，MID分段起始ID定义，各个模块定义时选择相应的起始ID */
#define TURBO_MID_BEGIN0 0x0000 /* common        */
#define TURBO_MID_BEGIN1 0x1000
#define TURBO_MID_BEGIN2 0x2000

/* 宏定义各个子模块MID计算方法 */
constexpr uint32_t TurboCommonError(uint32_t n)
{
    return TURBO_MID_BEGIN0 + n;
}

/* ********************************************* */
/* common错误码定义，全局唯一，记录系统的标准错误返回 */
/* ********************************************* */
#define TURBO_OK TurboCommonError(0)                         /* 正确 */
#define TURBO_ERROR TurboCommonError(1)                      /* 错误 */

#endif //  TURBO_ERROR_H