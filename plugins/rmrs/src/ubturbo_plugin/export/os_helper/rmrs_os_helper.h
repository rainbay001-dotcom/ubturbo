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
#ifndef RMRS_OS_HELPER_H
#define RMRS_OS_HELPER_H

#include <mutex>
#include <vector>

#include "rmrs_resource_export.h"
#include "rmrs_error.h"

namespace rmrs {
using namespace rmrs::exports;
using std::mutex;
using std::string;
using std::unordered_map;
using std::vector;

class OsHelper {
public:
    static RmrsResult GetSocketCpuRelation(unordered_map<uint16_t, uint16_t> &cpuSocketMap);

    static RmrsResult GetNumaCPUInfos(unordered_map<uint16_t, uint16_t> &cpuSocketMap,
                                      unordered_map<uint16_t, uint16_t> &cpuNumaMap,
                                      unordered_map<uint16_t, bool> &numaLocalMap, vector<NumaInfo> &numaInfos);

    static RmrsResult GetPidCreatTime(ResourceExport *resourceCollect);

    static RmrsResult GetProcessStartTime(pid_t pid, time_t &timestamp);

    static RmrsResult GetInfoFromNumaMaps(const string &uuid, const string &path, ResourceExport *resourceCollect);

    static RmrsResult GetVmPageSizeFromNumaMaps(const string &uuid, const string &path,
                                                ResourceExport *resourceCollect);

    static RmrsResult GetPidFromUUID(ResourceExport *resourceCollect);

    static RmrsResult GetVMUsedMemory(ResourceExport *resourceCollect);

    static RmrsResult GetVmsPidOnNuma(ResourceExport *resourceCollect);

    static RmrsResult UpdateNumaMemInfo(ResourceExport *resourceCollect);

    static RmrsResult GetRemoteAvailableFlag(vector<NumaInfo> &numaInfos);

    static std::string ExecCommand(const std::string &cmd);

    static RmrsResult ReadNumaMap(const std::string &pidStr, std::string &fileContent);

private:
    static string cpuSocketPathPrefix;
    static string cpuSocketPath;
    static string numaPathPrefix;
    static string procPathPrefix;
    static string qemuPathPrefix;
    static const unsigned int startTimeIndexInProc = 21;
    static const int numaIndex = 2;

    static vector<uint16_t> ParseCPUList(const string &line);

    static pid_t GetPidByVmName(const string &name);

    static int ReadCpuInfo(std::vector<std::string> &nodeInfo, NumaInfo &tempNumaInfo,
                           unordered_map<uint16_t, uint16_t> &cpuSocketMap, std::string folderName);
    static RmrsResult getMemInfo(const std::string &nodePath, NumaInfo &tempNumaInfo);
    static RmrsResult updateMemInfo(const std::string &nodePath, ResourceExport *resourceCollect,
                                    std::string folderName);
    static RmrsResult readMemInfo(std::vector<std::string> &memInfos, unordered_map<std::string, uint64_t> &memInfoMap);
    static RmrsResult getFileContent(const std::string &filePath, std::vector<std::string> &info,
                                     const std::string &fileName);
    static RmrsResult checkFileContent(std::vector<std::string> &info, const std::string &fileName, uint16_t vectorNum);
};
} // namespace rmrs

#endif // MP_OS_HELPER_H
