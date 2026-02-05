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

#include "resource_collector.h"

#include <dirent.h>
#include <cstring>

#include "ucache_file_util.h"
#include "ucache_string_util.h"
#include "ucache_turbo_config.h"
#include "ucache_turbo_error.h"

namespace turbo::ucache {
using namespace turbo::log;

namespace fs = std::filesystem;
const int KB_TO_BYTE = 1024;

uint32_t GetCgroupsIoBytesInfo(const std::string &dockerId, CgroupInfos &infos)
{
    std::string path = "/sys/fs/cgroup/blkio/docker/" + dockerId + "/blkio.throttle.io_service_bytes_recursive";

    std::vector<std::string> lines;
    if (UCacheFileUtil::GetFileInfo(path, lines) != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Read cgroup io bytes failed, dockerId: " << dockerId;
        return UCACHE_ERR;
    }

    if (lines.empty()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Cgroup info is empty, dockerId: " << dockerId;
        return UCACHE_ERR;
    }

    infos.ioInfo.ioReadBytes = 0;
    infos.ioInfo.ioWriteBytes = 0;

    static const char *kRead = "Read ";
    static const char *kWrite = "Write ";

    for (const auto &line : lines) {
        uint64_t val = 0;

        if (TryExtractValue(line, kRead, val)) {
            infos.ioInfo.ioReadBytes += val;
        } else if (TryExtractValue(line, kWrite, val)) {
            infos.ioInfo.ioWriteBytes += val;
        }
    }

    return UCACHE_OK;
}

uint32_t GetCgroupsIoTimesInfo(const std::string &dockerId, CgroupInfos &infos)
{
    std::string path = "/sys/fs/cgroup/blkio/docker/" + dockerId + "/blkio.throttle.io_serviced_recursive";
    std::vector<std::string> lines;
    if (UCacheFileUtil::GetFileInfo(path, lines) != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Read cgroup io times failed, dockerId: " << dockerId;
        return UCACHE_ERR;
    }

    if (lines.empty()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Cgroup info is empty, dockerId: " << dockerId;
        return UCACHE_ERR;
    }

    infos.ioInfo.ioReadTimes = 0;
    infos.ioInfo.ioWriteTimes = 0;

    static const char *kRead = "Read ";
    static const char *kWrite = "Write ";
    for (const auto &line : lines) {
        uint64_t val = 0;

        if (TryExtractValue(line, kRead, val)) {
            infos.ioInfo.ioReadTimes += val;
        } else if (TryExtractValue(line, kWrite, val)) {
            infos.ioInfo.ioWriteTimes += val;
        }
    }
    return UCACHE_OK;
}

uint32_t GetCgroupsPageCacheInfo(const std::string &dockerId, CgroupInfos &infos)
{
    std::string path = "/sys/fs/cgroup/memory/docker/" + dockerId + "/memory.stat";
    std::vector<std::string> lines;
    if (UCacheFileUtil::GetFileInfo(path, lines) != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Read cgroup pagecache info failed, dockerId: " << dockerId;
        return UCACHE_ERR;
    }

    if (lines.empty()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Cgroup info is empty, dockerId: " << dockerId;
        return UCACHE_ERR;
    }

    infos.pageCacheInfo = {0, 0, 0, 0};

    static const char *kPgin = "total_pgpgin ";
    static const char *kPgout = "total_pgpgout ";
    static const char *kInact = "total_inactive_file ";
    static const char *kAct = "total_active_file ";

    for (const auto &line : lines) {
        uint64_t val = 0;

        if (TryExtractValue(line, kPgin, val)) {
            infos.pageCacheInfo.pageCacheIn = val;
        } else if (TryExtractValue(line, kPgout, val)) {
            infos.pageCacheInfo.pageCacheOut = val;
        } else if (TryExtractValue(line, kInact, val)) {
            infos.pageCacheInfo.totalInactiveFileBytes = val;
        } else if (TryExtractValue(line, kAct, val)) {
            infos.pageCacheInfo.totalActiveFileBytes = val;
        }
    }

    return UCACHE_OK;
}

uint32_t GetNodeMemInfo(const std::string &nodeId, NodeInfo &infos)
{
    std::string path = "/sys/devices/system/node/" + nodeId + "/meminfo";

    std::vector<std::string> lines;
    if (UCacheFileUtil::GetFileInfo(path, lines) != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Read node meminfo failed, nodeId: " << nodeId;
        return UCACHE_ERR;
    }

    if (lines.empty()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node meminfo is empty, nodeId: " << nodeId;
        return UCACHE_ERR;
    }

    infos.memTotalBytes = 0;
    infos.memFreeBytes = 0;
    infos.memUsedBytes = 0;
    infos.activeFileBytes = 0;
    infos.inactiveFileBytes = 0;

    static const char *kTotal = "MemTotal:";
    static const char *kFree = "MemFree:";
    static const char *kUsed = "MemUsed:";  // 注意：标准内核通常没有这个字段，如果是定制内核则保留
    static const char *kActive = "Active(file):";
    static const char *kInactive = "Inactive(file):";

    for (const auto &line : lines) {
        uint64_t val = 0;

        if (TryExtractValue(line, kTotal, val)) {
            infos.memTotalBytes = val * KB_TO_BYTE;
        } else if (TryExtractValue(line, kFree, val)) {
            infos.memFreeBytes = val * KB_TO_BYTE;
        } else if (TryExtractValue(line, kUsed, val)) {
            infos.memUsedBytes = val * KB_TO_BYTE;
        } else if (TryExtractValue(line, kActive, val)) {
            infos.activeFileBytes = val * KB_TO_BYTE;
        } else if (TryExtractValue(line, kInactive, val)) {
            infos.inactiveFileBytes = val * KB_TO_BYTE;
        }
    }

    return UCACHE_OK;
}

void GetRemoteInfo(const std::string &nodeId, NodeInfo &infos)
{
    infos.isRemote = false;

    std::string path = "/sys/devices/system/node/" + nodeId + "/remote";
    std::vector<std::string> lines;

    if (UCacheFileUtil::GetFileInfo(path, lines) != UCACHE_OK || lines.empty()) {
        return;
    }

    std::string cleanStr = TrimString(lines[0]);

    uint32_t val = 0;
    if (SafeStoInt(cleanStr, val) == UCACHE_OK) {
        infos.isRemote = (val != 0);
    }
}

static bool IsDockerIdFormat(const std::string &s)
{
    const size_t dockerIdLength = 64;
    if (s.length() != dockerIdLength) {
        return false;
    }
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

uint32_t GetDockerIDs(std::vector<std::string> &dockerIDs)
{
    static const std::string kPath = "/sys/fs/cgroup/cpuset/docker/";
    namespace fs = std::filesystem;

    std::vector<std::string> allFiles;
    if (UCacheFileUtil::ListDirectory(kPath, allFiles, true) != UCACHE_OK) {
        UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to open or list dockerSysPath.";
        return UCACHE_OK;
    }

    for (const auto &name : allFiles) {
        if (!IsDockerIdFormat(name)) {
            continue;
        }

        std::string fullPath = kPath + name;
        std::error_code ec;
        if (!fs::is_directory(fullPath, ec) && !fs::is_symlink(fullPath, ec)) {
            continue;
        }

        dockerIDs.emplace_back(name);
    }

    if (dockerIDs.empty()) {
        UBTURBO_LOG_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "DockerIDs is empty.";
    } else {
        UBTURBO_LOG_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << dockerIDs.size() << " running docker containers found.";
    }

    return UCACHE_OK;
}

uint32_t CollectCgroupsInfo(std::unordered_map<std::string, CgroupInfos> &cgroupInfos)
{
    std::vector<std::string> dockerIDs{};
    uint32_t ret = GetDockerIDs(dockerIDs);
    if (ret != UCACHE_OK) {
        return ret;
    }
    for (const auto &id : dockerIDs) {
        CgroupInfos infos{};
        uint32_t ret = GetCgroupsIoBytesInfo(id, infos);
        if (ret == UCACHE_ERR) {
            return ret;
        }
        ret = GetCgroupsIoTimesInfo(id, infos);
        if (ret == UCACHE_ERR) {
            return ret;
        }
        ret = GetCgroupsPageCacheInfo(id, infos);
        if (ret == UCACHE_ERR) {
            return ret;
        }
        cgroupInfos.emplace(id, infos);
    }
    return UCACHE_OK;
}

uint32_t GetNodeIDs(std::vector<std::string> &nodeIDs)
{
    static const std::string kPath = "/sys/devices/system/node";
    namespace fs = std::filesystem;

    std::vector<std::string> allFiles{};
    if (UCacheFileUtil::ListDirectory(kPath, allFiles, true) != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Sys_node_path is not exist or invalid.";
        return UCACHE_ERR;
    }

    for (const auto &name : allFiles) {
        if (name.find("node") != 0) {
            continue;
        }

        std::string fullPath = kPath + "/" + name;
        std::error_code ec;
        if (!fs::is_directory(fullPath, ec)) {
            continue;
        }
        nodeIDs.emplace_back(name);
    }

    return nodeIDs.empty() ? UCACHE_ERR : UCACHE_OK;
}

uint32_t CollectNodeInfo(std::unordered_map<std::string, NodeInfo> &numaInfos)
{
    std::vector<std::string> nodeIDs{};
    uint32_t ret = GetNodeIDs(nodeIDs);
    if (ret == UCACHE_ERR) {
        return ret;
    }
    for (const auto &id : nodeIDs) {
        NodeInfo info{};
        ret = GetNodeMemInfo(id, info);
        if (ret == UCACHE_ERR) {
            return ret;
        }
        GetRemoteInfo(id, info);
        numaInfos.emplace(id, info);
    }
    return UCACHE_OK;
}

uint32_t GetMemMinFreeKbytes(MemWatermarkInfo &memWatermark)
{
    std::string path = "/proc/sys/vm/min_free_kbytes";

    std::vector<std::string> lines;
    if (UCacheFileUtil::GetFileInfo(path, lines) != UCACHE_OK) {
        return UCACHE_ERR;
    }

    if (lines.empty()) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << path << " has no content.";
        return UCACHE_ERR;
    }

    std::string cleanVal = TrimString(lines[0]);
    uint64_t val = 0;
    if (SafeStoInt(cleanVal, val) != UCACHE_OK) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid min_free_kbytes content: " << lines[0];
        return UCACHE_ERR;
    }

    memWatermark.minFreeKBytes = val;
    return UCACHE_OK;
}

uint32_t CollectMemWatermarkInfo(MemWatermarkInfo &memWatermark)
{
    uint32_t ret = GetMemMinFreeKbytes(memWatermark);
    if (ret == UCACHE_ERR) {
        return ret;
    }
    return UCACHE_OK;
}

uint32_t GetMemWatermarkInfo(MemWatermarkInfo &memWatermark)
{
    uint32_t ret = CollectMemWatermarkInfo(memWatermark);
    if (ret == UCACHE_ERR) {
        return ret;
    }
    return UCACHE_OK;
}

uint32_t GetCgroupsInfo(std::unordered_map<std::string, CgroupInfos> &cgroupInfos)
{
    uint32_t ret = CollectCgroupsInfo(cgroupInfos);
    if (ret == UCACHE_ERR) {
        return ret;
    }
    return UCACHE_OK;
}

uint32_t GetNumaInfo(std::unordered_map<std::string, NodeInfo> &numaInfos)
{
    uint32_t ret = CollectNodeInfo(numaInfos);
    if (ret == UCACHE_ERR) {
        UBTURBO_LOG_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Collect node info failed!";
        return ret;
    }
    return UCACHE_OK;
}
} // namespace turbo::ucache