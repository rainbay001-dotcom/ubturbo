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
#ifndef OVER_COMMIT_DEF_H
#define OVER_COMMIT_DEF_H

#include <sstream>
#include "rmrs_serialize.h"

namespace mempooling::over_commit {
using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;
constexpr int BYTE2MB = 20;
constexpr int BYTE2KB = 10;
constexpr int KB2BYTE = 10;
constexpr int KB2MB = 10;
constexpr int VIRT_SIZE = 2048;
constexpr int CTR_SIZE = 4;
constexpr uint64_t KB22MB = static_cast<uint64_t>(2) << KB2MB;
constexpr uint64_t B22KB = static_cast<uint64_t>(1) << BYTE2KB;

struct MemBorrowInfo {
    uint16_t presentNumaId;
    uint64_t borrowSize;

    std::string ToString() const
    {
        return R"("presentNumaId":)" + std::to_string(presentNumaId) + R"(,"borrowSize":)" + std::to_string(borrowSize);
    }
};

struct MemBorrowInfoWithSrc {
    uint64_t srcNumaId;
    uint16_t presentNumaId;
    uint64_t borrowSize;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({presentNumaId=)" << presentNumaId << R"(,)";
        oss << R"(borrowSize=)" << borrowSize << R"(,)";
        oss << R"(srcNumaId=)" << srcNumaId << R"(})";
        return oss.str();
    }
};

struct MemMigrateResult {
    pid_t pid;
    uint16_t remoteNumaId;
    uint64_t size;
    uint16_t maxRatio;

    std::string ToString() const
    {
        return R"(pid=)" + std::to_string(pid) + R"(, remoteNumaId=)" + std::to_string(remoteNumaId) + R"(, size=)" +
               std::to_string(size) + R"(, maxRatio=)" + std::to_string(maxRatio);
    }
};

} // namespace mempooling::over_commit

#endif // OVER_COMMIT_DEF_H
