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
#ifndef TURBO_COMMON_H
#define TURBO_COMMON_H
#include <cstdint>
#include <functional>

namespace turbo::common {

using RetCode = uint32_t;

constexpr auto CONFIG_DEFAULT_DIR = "conf";
constexpr auto LIB_DEFAULT_DIR = "lib";

constexpr inline auto MODULE_NAME = "ubturbo";
constexpr inline auto MODULE_CODE = 1;
}


#endif //  TURBO_COMMON_H

