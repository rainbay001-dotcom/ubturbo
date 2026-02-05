/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * ucache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UCACHE_TURBO_ERROR_H
#define UCACHE_TURBO_ERROR_H

#include <cstdint>
namespace turbo::ucache {
    const uint32_t UCACHE_OK = 0;
    const uint32_t UCACHE_ERR = 1;
    const uint32_t UCACHE_ERR_MSG_LEN = 2;

    // IPC执行错误码
    const uint32_t EXECUTE_DEFAULT_ERROR = 0x3001;
    const uint32_t EXECUTE_INVALID_PARAM = 0x3002;
    const uint32_t TURBO_EXECUTE_ERROR = 0x3003;

    // 任务执行错误码
    const uint32_t INVALID_TASK_TYPE = 0x4002;
    const uint32_t INVALID_BUFFER = 0x4006;
    const uint32_t EMPTY_BUFFER = 0x4007;

    // 字符串转换错误码
    const uint32_t EMPTY_STRING = 0x5001;
    const uint32_t INVALID_ARGUMENT = 0x5002;
    const uint32_t OUT_OF_RANGE = 0x5003;
} // namespace turbo::ucache

#endif // UCACHE_TURBO_ERROR_H