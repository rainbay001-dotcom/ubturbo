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

#ifndef RESOURCE_EXPORT_H
#define RESOURCE_EXPORT_H

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbo::ucache {
struct CgroupIoInfo {
    uint64_t ioReadBytes{};
    uint64_t ioWriteBytes{};
    uint64_t ioReadTimes{};
    uint64_t ioWriteTimes{};
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "ioReadBytes:" << ioReadBytes << ",";
        oss << "ioWriteBytes:" << ioWriteBytes << ",";
        oss << "ioReadTimes:" << ioReadTimes << ",";
        oss << "ioWriteTimes:" << ioWriteTimes;
        oss << "}";
        return oss.str();
    }
};

struct CgroupPageCacheInfo {
    uint64_t totalActiveFileBytes{};
    uint64_t totalInactiveFileBytes{};
    uint64_t pageCacheIn{};
    uint64_t pageCacheOut{};
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "totalActiveFileBytes:" << totalActiveFileBytes << ",";
        oss << "totalInactiveFileBytes:" << totalInactiveFileBytes << ",";
        oss << "pageCacheIn:" << pageCacheIn << ",";
        oss << "pageCacheOut:" << pageCacheOut;
        oss << "}";
        return oss.str();
    }
};

struct CgroupInfos {
    struct CgroupIoInfo ioInfo {};
    struct CgroupPageCacheInfo pageCacheInfo {};
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "CgroupIoInfo:" << ioInfo.ToString() << ",";
        oss << "CgroupPageCacheInfo:" << pageCacheInfo.ToString() << ",";
        oss << "}";
        return oss.str();
    }
};

struct NodeInfo {
    uint64_t memTotalBytes{};
    uint64_t memFreeBytes{};
    uint64_t memUsedBytes{};
    uint64_t activeFileBytes{};
    uint64_t inactiveFileBytes{};
    bool isRemote{};
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "memTotalBytes:" << memTotalBytes << ",";
        oss << "memFreeBytes:" << memFreeBytes << ",";
        oss << "memUsedBytes:" << memUsedBytes << ",";
        oss << "activeFileBytes:" << activeFileBytes << ",";
        oss << "inactiveFileBytes:" << inactiveFileBytes << ",";
        oss << "isRemote:" << (isRemote ? "true" : "false");
        oss << "}";
        return oss.str();
    }
};

struct MemWatermarkInfo {
    uint64_t minFreeKBytes{};
};

uint32_t GetCgroupsInfo(std::unordered_map<std::string, CgroupInfos> &cgroupInfos);
uint32_t GetNumaInfo(std::unordered_map<std::string, NodeInfo> &numaInfos);
uint32_t GetMemWatermarkInfo(MemWatermarkInfo &memWatermark);
bool IfPsiOpen();

} // namespace turbo::ucache

#endif // RESOURCE_EXPORT_H
