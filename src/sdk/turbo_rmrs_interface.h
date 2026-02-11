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

#ifndef RMRS_INTERFACE_H
#define RMRS_INTERFACE_H

#include <sys/types.h>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <set>
#include <sstream>

namespace turbo::rmrs {

struct VMPresetParam {
    pid_t pid;      // vm对应pid
    uint16_t ratio; // 迁出最大比例
};

struct NumaData {
    std::string numaId{};
    bool operator==(const NumaData &numaData) const
    {
        return numaId == numaData.numaId;
    }
};

struct RemoteNumaSocketInfo {
    int16_t borrowRemoteNuma{-1}; // 借入numa, remote 借用时有效，否则为-1
    std::string lentNode{};       // 借出节点
    uint16_t lentSocketId{0};     // 借出内存socketId
};

struct MigrateStrategyParamRMRS {
    std::vector<VMPresetParam> vmInfoList;                    // 虚拟机列表及最大迁出比例
    std::uint64_t borrowSize;                                 // 需要匀出本地内存大小
    std::map<pid_t, std::vector<uint16_t>> pidRemoteNumaMap;  // pid对应的远端numa信息Map
    std::vector<uint16_t> timeOutNumas;                       // 归还超时的远端numa
};

struct VMMigrateOutParam {
    pid_t pid;
    uint64_t memSize;   // 迁出预设比例
    uint16_t desNumaId; // 迁移远端numa
};

struct MigrateStrategyResult {
    std::vector<VMMigrateOutParam> vmInfoList;
    uint64_t waitingTime; // 单位ms
};

class MigrateBackResult {
public:
    uint32_t result{};
    std::vector<uint16_t> numaIds{};
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

struct MetaNumaInfo {
    uint16_t numaId{};       // numaId
    uint64_t numaUsedMem{};  // 该numaId上使用的内存
    bool isLocalNuma{true};  // 是否本地numa
    int socketId{-1};        // numaId对应的socketId

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "numaId:" << numaId << ",";
        oss << "numaUsedMem:" << numaUsedMem << " BYTE,";
        oss << "isLocalNuma:" << isLocalNuma << ",";
        oss << "socketId:" << socketId;
        oss << "}";
        return oss.str();
    }
};

struct PidInfo {
    pid_t pid{};                                 // 进程id
    std::vector<uint16_t> localNumaIds{};        // 本地numaId集合
    uint64_t totalLocalUsedMem{};                // 本地numa上使用的总内存大小
    uint64_t totalRemoteUsedMem{};               // 远端numa使用的总内存大小
    std::vector<MetaNumaInfo> metaNumaInfos{};   // pid进程元信息集合
    
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "pid:" << pid;
        oss << ",localNumaIds:[";
        for (size_t i = 0; i < localNumaIds.size(); ++i) {
            if (i) oss << ", ";
            oss << localNumaIds[i];
        }
        oss << "]";
        oss << "totalLocalUsedMem:" << totalLocalUsedMem << "BYTE";
        oss << "totalRemoteUsedMem:" << totalRemoteUsedMem << "BYTE";
        oss << ",metaNumaInfos:[";
        for (size_t i = 0; i < metaNumaInfos.size(); ++i) {
            if (i) oss << ", ";
            oss << metaNumaInfos[i].ToString();
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

class PidNumaInfoCollectParam {
public:
    PidNumaInfoCollectParam() {}
    explicit PidNumaInfoCollectParam(std::vector<pid_t> pidList) : pidList(pidList) {}
    std::vector<pid_t> pidList{};
};

class PidNumaInfoCollectResult {
public:
    PidNumaInfoCollectResult() {}
    explicit PidNumaInfoCollectResult(std::vector<PidInfo> pidInfoList) : pidInfoList(pidInfoList) {}
    std::vector<PidInfo> pidInfoList{};
};

class NumaMemInfoCollectParam {
public:
    NumaMemInfoCollectParam() {}
    explicit NumaMemInfoCollectParam(int numaId) : numaId(numaId) {}
    int numaId{};
};

struct ResponseInfo {
    unsigned int code;
    std::string message;
};

class ResponseInfoSimpo {
public:
    ResponseInfoSimpo() = default;
    explicit ResponseInfoSimpo(ResponseInfo responseInfoInput) : responseInfo_(std::move(responseInfoInput)) {}

    inline ResponseInfo GetResponseInfo()
    {
        return responseInfo_;
    }

    inline void SetResponseInfo(const int code, const std::string &message)
    {
        responseInfo_.code = code;
        responseInfo_.message = message;
    }

    std::string ToString() const
    {
        return "code=" + std::to_string(responseInfo_.code) + ", message=" + responseInfo_.message;
    };

    ResponseInfo responseInfo_{};
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
            if (!firstRemote) {
                oss << ",";
            }
            oss << id;
            firstRemote = false;
        }

        oss << R"(],)";
        oss << R"("ucacheUsageRatio":)" << ucacheUsageRatio << R"(,)";
        oss << R"("pids": [)";

        bool firstPid = true;
        for (pid_t pid : pids) {
            if (!firstPid) {
                oss << ",";
            }
            oss << pid;
            firstPid = false;
        }

        oss << R"(]})";
        return oss.str();
    }
};

struct ResCode {
    uint32_t resCode{};
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
            if (!firstPid) {
                oss << ",";
            }
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
extern "C" {
/**
 * @brief 内存迁出执策略
 * @param migrateStrategyParamRMRS [IN] 迁出策略参数
 * @param migrateStrategyResult [OUT] 迁出策略参数
 * @return  0为成功, 非0为异常
 */
uint32_t UBTurboRMRSAgentMigrateStrategy(const MigrateStrategyParamRMRS &migrateStrategyParam,
                                         MigrateStrategyResult &migrateStrategyResult);

/**
 * @brief 内存迁出执行
 * @param migrateStrategyResult [IN] 迁出执行参数
 * @return  0为成功, 非0为异常
 */
uint32_t UBTurboRMRSAgentMigrateExecute(const MigrateStrategyResult &migrateStrategyResult);

/**
 * @brief 将节点内所有远端内存迁回本地
 * @param migrateBackResult [OUT] 迁回结果
 * @return  0为成功, 非0为异常
 */
uint32_t UBTurboRMRSAgentMigrateBack(MigrateBackResult &migrateBackResult);

/**
 * @brief 将上次作业中迁出的内存回滚
 * @param borrowIdsPidsMap [IN] 内存标识符与pid映射表
 * @return  0为成功, 非0为异常
 */
uint32_t UBTurboRMRSAgentBorrowRollBack(std::map<std::string, std::set<BorrowIdInfo>> &borrowIdsPidsMap);

/**
 * @brief 采集进程内存信息
 * @param pidNumaInfoCollectParam [IN] 进程内存信息采集参数
 * @param pidNumaInfoCollectResult [OUT] 进程内存信息采集结果
 * @return  0为成功, 非0为异常
 */
uint32_t UBTurboRMRSAgentPidNumaInfoCollect(const PidNumaInfoCollectParam &pidNumaInfoCollectParam,
                                            PidNumaInfoCollectResult &pidNumaInfoCollectResult);

/**
 * @brief 采集numa或节点内存信息
 * @param numaMemInfoCollectParam [IN] numa/节点内存信息采集参数
 * @param responseInfoSimpo [OUT] numa/节点内存信息采集结果
 * @return  0为成功, 非0为异常
 */
uint32_t UBTurboRMRSAgentNumaMemInfoCollect(const NumaMemInfoCollectParam &numaMemInfoCollectParam,
                                            ResponseInfoSimpo &responseInfoSimpo);

/**
 * @brief 下发ucache迁移执行指令
 * @param uCacheMigrationStrategyParam [IN] ucache迁移执行参数
 * @param resCode [OUT] ucache迁移执行结果
 * @return  0为成功, 非0为异常
 */
uint32_t UBTurboRMRSAgentUCacheMigrateStrategy(const UCacheMigrationStrategyParam &uCacheMigrationStrategyParam,
                                               ResCode &resCode);

/**
 * @brief 下发ucache迁移停止指令
 * @param resCode [OUT] ucache迁移停止结果
 * @return  0为成功, 非0为异常
 */
uint32_t UBTurboRMRSAgentUCacheMigrateStop(ResCode &resCode);

/**
 * @brief 更新pagecache使用比例
 * @param migrationInfoParam [IN] 迁移参数
 * @param uCacheRatioRes [OUT] 更新pagecache使用比例结果
 * @return  0为成功, 非0为异常
 */
uint32_t UBTurboRMRSAgentUpdateUCacheRatio(const MigrationInfoParam &migrationInfoParam,
                                           UCacheRatioRes &uCacheRatioRes);
}
} // namespace turbo::rmrs

#endif