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

#ifndef RMRS_FILE_UTIL_H
#define RMRS_FILE_UTIL_H

#include <string>
#include <vector>

#include "rmrs_error.h"

namespace rmrs {
using std::string;
using std::vector;

class RmrsFileUtil {
public:
    static RMRS_RES GetFileInfo(const string &path, vector<string> &info);
    static RMRS_RES IsPathExist(const string &path);
    static RMRS_RES IsSpecifiedPath(const string &dirPath, const string &pattern);
};
}

#endif // RMRS_FILE_UTIL_H
