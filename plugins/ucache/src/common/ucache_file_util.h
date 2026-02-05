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

#ifndef UCACHE_FILE_UTIL_H
#define UCACHE_FILE_UTIL_H

#include <string>
#include <vector>

namespace turbo::ucache {
using std::string;
using std::vector;

class UCacheFileUtil {
public:
    static uint32_t GetFileInfo(const string &pathStr, vector<string> &info);
    static uint32_t ListDirectory(const string &path, vector<string> &outNames, bool sorted);
};
} // namespace turbo::ucache

#endif // UCACHE_FILE_UTIL_H