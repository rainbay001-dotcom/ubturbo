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
#ifndef UCACHE_MESSAGE_H
#define UCACHE_MESSAGE_H

#include <sstream>
#include <string>
#include "rmrs_serialize.h"

namespace rmrs {

using namespace rmrs::serialize;

struct ResCode {
    uint32_t resCode{};
};

struct UCacheMigrationStrategyParam {
    int16_t localNumaId{};                 // 执行迁出的本地numa节点。若小于0，代表所有本地numa节点
    std::vector<uint16_t> remoteNumaIds{}; // 执行迁入的远端内存呈现numa节点列表
    std::vector<pid_t> pids{};             // 需要迁移的进程列表
    float ucacheUsageRatio{};              // 给Pagecache分配使用的内存比例

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"localNumaId":)" << localNumaId << R"(,)";
        oss << R"("remoteNumaIds": [)";

        bool firstRemote = true;
        for (uint16_t id : remoteNumaIds) {
            if (!firstRemote)
                oss << ",";
            oss << id;
            firstRemote = false;
        }

        oss << R"(],)";
        oss << R"("ucacheUsageRatio":)" << ucacheUsageRatio << R"(,)";
        oss << R"("pids": [)";

        bool firstPid = true;
        for (pid_t pid : pids) {
            if (!firstPid)
                oss << ",";
            oss << pid;
            firstPid = false;
        }

        oss << R"(]})";
        return oss.str();
    }
};

struct MigrationInfoParam {
    uint64_t borrowMemKB{};    // 借用内存总大小，单位KB
    std::vector<pid_t> pids{}; // 需要迁移的进程列表
 
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"({"borrowMemKB":)" << borrowMemKB << R"(,)";
 
        bool firstPid = true;
        oss << R"("pids": [)";
        for (pid_t pid : pids) {
            if (!firstPid)
                oss << ",";
            oss << pid;
            firstPid = false;
        }
 
        oss << R"(]})";
        return oss.str();
    }
};
 
struct UCacheRatioRes {
    float ucacheUsageRatio{}; // ucache借用比例
    uint32_t resCode{};       // 是否执行成功
};

} // namespace rmrs
#endif