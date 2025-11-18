/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */#ifndef RMRS_JSON_HELPER_H
#define RMRS_JSON_HELPER_H

#include <string>
#include <map>
#include <set>

namespace rmrs::serialization {

class RmrsJsonHelper {
    static std::string ToJson()
    {
        return "";
    }
    static bool FromJson()
    {
        return true;
    }
};


struct BorrowIdInfo {
    pid_t pid;
    uint64_t oriSize;

    friend bool operator<(const BorrowIdInfo &x, const BorrowIdInfo &y)
    {
        if (x.pid != y.pid) {
            return x.pid < y.pid;
        }
        return x.oriSize < y.oriSize;
    }
    friend bool operator==(const BorrowIdInfo &x, const BorrowIdInfo &y)
    {
        return x.pid == y.pid && x.oriSize == y.oriSize;
    }
};

} // namespace rmrs::serialization

#endif