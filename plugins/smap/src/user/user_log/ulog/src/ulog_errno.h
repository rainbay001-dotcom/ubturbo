/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef __ULOG_ERRNO_H__
#define __ULOG_ERRNO_H__

namespace utilities {
namespace log {
constexpr int UOK = 0;
constexpr int UERR_CREATE_FAILED = 1;
constexpr int UERR_INVALID_PARAM = 2;
constexpr int UERR_UNINITIALIZED = 3;
constexpr int UERR_SET_OPTION_FAILED = 4;
constexpr int UERR_INVALID_LEVEL = 5;
}
}

#endif
