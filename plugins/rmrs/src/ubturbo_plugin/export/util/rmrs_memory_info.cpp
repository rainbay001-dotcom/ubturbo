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
#include "rmrs_memory_info.h"
#include "turbo_logger.h"
#include "rmrs_error.h"
#include "rmrs_string_util.h"

namespace rmrs {
using namespace turbo::log;

uint64_t StringToKB(std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    size_t unitPos = str.find_first_not_of("0123456789."); // 找到单位起始位置
    if (unitPos == std::string::npos) {
        return 0; // 无单位，返回 0
    }
    double value = std::stod(str.substr(0, unitPos)); // 提取数值部分
    std::string unit = str.substr(unitPos);           // 提取单位部分
    if (unit == "KB") {
        return static_cast<uint64_t>(value);
    }
    if (unit == "MB") {
        return static_cast<uint64_t>(value * KB);
    }
    if (unit == "GB") {
        return static_cast<uint64_t>(value * MB);
    }
    if (unit == "TB") {
        return static_cast<uint64_t>(value * TB);
    }
    return 0; // 未知单位，返回 0
}

bool NodeMemoryInfoList::ParseNodeMemoryInfoMap(const JSON_VEC &nodeMemoryInfoListVec, const int &i,
                                                JSON_MAP &nodeMemoryInfoMap)
{
    for (const auto &key : {"timestamp", "nodeId", "totalMemory", "usedMemory", "freeMemory", "borrowedMemory",
                            "lentMemory", "numaMemInfo", "borrowedAndLentInfo"}) {
        nodeMemoryInfoMap.emplace(key, "");
    }
    if (!JsonUtil::RackMemConvertJsonStr2Map(nodeMemoryInfoListVec[i], nodeMemoryInfoMap)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return RMRS_ERROR;
    }
    uint64_t tmpTimeStamp;
    uint64_t tmpTotalMemory;
    uint64_t tmpUsedMemory;
    uint64_t tmpFreeMemory;
    uint64_t tmpBorrowedMemory;
    uint64_t tmpLentMemory;
    if (!RmrsStringUtil::StrToULong(nodeMemoryInfoMap["timestamp"], tmpTimeStamp) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(nodeMemoryInfoMap["totalMemory"])), tmpTotalMemory) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(nodeMemoryInfoMap["usedMemory"])), tmpUsedMemory) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(nodeMemoryInfoMap["freeMemory"])), tmpFreeMemory) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(nodeMemoryInfoMap["borrowedMemory"])),
                                    tmpBorrowedMemory) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(nodeMemoryInfoMap["lentMemory"])), tmpLentMemory)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Str to nodeMemoryInfoMap error.";
        return RMRS_ERROR;
    }
    this->nodeMemoryInfoList[i].timestamp = static_cast<time_t>(tmpTimeStamp);
    this->nodeMemoryInfoList[i].nodeId = nodeMemoryInfoMap["nodeId"];
    this->nodeMemoryInfoList[i].totalMemory = static_cast<uint64_t>(tmpTotalMemory);
    this->nodeMemoryInfoList[i].usedMemory = static_cast<uint64_t>(tmpUsedMemory);
    this->nodeMemoryInfoList[i].freeMemory = static_cast<uint64_t>(tmpFreeMemory);
    this->nodeMemoryInfoList[i].borrowedMemory = static_cast<uint64_t>(tmpBorrowedMemory);
    this->nodeMemoryInfoList[i].lentMemory = static_cast<uint64_t>(tmpLentMemory);
    return RMRS_OK;
}

bool NodeMemoryInfoList::ParseNumaMemInfoMap(const JSON_VEC &numaMemInfoVec, const int &i, const int &j,
                                             JSON_MAP &numaMemInfoMap)
{
    numaMemInfoMap.emplace("numaId", "");
    numaMemInfoMap.emplace("memTotal", "");
    numaMemInfoMap.emplace("memFree", "");
    numaMemInfoMap.emplace("memUsed", "");
    numaMemInfoMap.emplace("vmMemTotal", "");
    numaMemInfoMap.emplace("vmMemFree", "");
    numaMemInfoMap.emplace("reservedMem", "");
    numaMemInfoMap.emplace("lentMem", "");
    numaMemInfoMap.emplace("sharedMem", "");
    if (!JsonUtil::RackMemConvertJsonStr2Map(numaMemInfoVec[j], numaMemInfoMap)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return RMRS_ERROR;
    }
    uint32_t tmpNumaId;
    uint64_t tmpMemTotal;
    uint64_t tmpMemFree;
    uint64_t tmpMemUsed;
    uint64_t tmpVmMemTotal;
    uint64_t tmpVmMemFree;
    uint64_t tmpVmUsedMem;
    uint64_t tmpReservedMem;
    uint64_t tmpLentMem;
    uint64_t tmpSharedMem;
    if (!RmrsStringUtil::StrToUint(numaMemInfoMap["numaId"], tmpNumaId) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(numaMemInfoMap["memTotal"])), tmpMemTotal) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(numaMemInfoMap["memFree"])), tmpMemFree) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(numaMemInfoMap["memUsed"])), tmpMemUsed) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(numaMemInfoMap["vmMemTotal"])), tmpVmMemTotal) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(numaMemInfoMap["vmMemFree"])), tmpVmMemFree) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(numaMemInfoMap["vmMemUsed"])), tmpVmUsedMem) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(numaMemInfoMap["reservedMem"])), tmpReservedMem) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(numaMemInfoMap["lentMem"])), tmpLentMem) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(numaMemInfoMap["sharedMem"])), tmpSharedMem)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Str to numaMemInfoMap error.";
        return RMRS_ERROR;
    }
    this->nodeMemoryInfoList[i].numaMemInfo[j].numaId = tmpNumaId;
    this->nodeMemoryInfoList[i].numaMemInfo[j].memTotal = tmpMemTotal;
    this->nodeMemoryInfoList[i].numaMemInfo[j].memFree = tmpMemFree;
    this->nodeMemoryInfoList[i].numaMemInfo[j].memUsed = tmpMemUsed;
    this->nodeMemoryInfoList[i].numaMemInfo[j].vmMemTotal = tmpVmMemTotal;
    this->nodeMemoryInfoList[i].numaMemInfo[j].vmMemFree = tmpVmMemFree;
    this->nodeMemoryInfoList[i].numaMemInfo[j].vmUsedMem = tmpVmUsedMem;
    this->nodeMemoryInfoList[i].numaMemInfo[j].reservedMem = tmpReservedMem;
    this->nodeMemoryInfoList[i].numaMemInfo[j].lentMem = tmpLentMem;
    this->nodeMemoryInfoList[i].numaMemInfo[j].sharedMem = tmpSharedMem;
    return RMRS_OK;
}

bool NodeMemoryInfoList::ParseBorrowedItemMap(const JSON_VEC &borrowedItemVec, const int &i, const int &k,
                                              JSON_MAP &borrowedItemMap)
{
    borrowedItemMap.emplace("nodeId", "");
    borrowedItemMap.emplace("numaId", "");
    borrowedItemMap.emplace("memory", "");
    if (!JsonUtil::RackMemConvertJsonStr2Map(borrowedItemVec[k], borrowedItemMap)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return RMRS_ERROR;
    }
    uint32_t tmpNumaId;
    uint64_t tmpMemory;
    if (!RmrsStringUtil::StrToUint(borrowedItemMap["numaId"], tmpNumaId) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(borrowedItemMap["memory"])), tmpMemory)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Str to numaMemInfoMap error.";
        return RMRS_ERROR;
    }
    this->nodeMemoryInfoList[i].borrowedAndLentInfo.borrowedItem[k].nodeId = borrowedItemMap["nodeId"];
    this->nodeMemoryInfoList[i].borrowedAndLentInfo.borrowedItem[k].numaId = tmpNumaId;
    this->nodeMemoryInfoList[i].borrowedAndLentInfo.borrowedItem[k].memorySize = tmpMemory;
    return RMRS_OK;
}

bool NodeMemoryInfoList::ParseLentItemMap(const JSON_VEC &lentItemVec, const int &i, const int &v,
                                          JSON_MAP &lentItemMap)
{
    lentItemMap.emplace("nodeId", "");
    lentItemMap.emplace("numaId", "");
    lentItemMap.emplace("memory", "");
    if (!JsonUtil::RackMemConvertJsonStr2Map(lentItemVec[v], lentItemMap)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return RMRS_ERROR;
    }
    uint32_t tmpNumaId;
    uint64_t tmpMemory;
    if (!RmrsStringUtil::StrToUint(lentItemMap["numaId"], tmpNumaId) ||
        !RmrsStringUtil::StrToULong(std::to_string(StringToKB(lentItemMap["memory"])), tmpMemory)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Str to numaMemInfoMap error.";
        return RMRS_ERROR;
    }
    this->nodeMemoryInfoList[i].borrowedAndLentInfo.lentItem[v].nodeId = lentItemMap["nodeId"];
    this->nodeMemoryInfoList[i].borrowedAndLentInfo.lentItem[v].numaId = tmpNumaId;
    this->nodeMemoryInfoList[i].borrowedAndLentInfo.lentItem[v].memorySize = tmpMemory;
    return RMRS_OK;
}

bool NodeMemoryInfoList::CreateNodeMemoryInfoListVec(const std::string &jsonString, JSON_MAP &nodeMemoryInfoListMAP,
                                                     JSON_VEC &nodeMemoryInfoListVec)
{
    nodeMemoryInfoListMAP.emplace("nodeMemoryInfoList", "");
    if (!JsonUtil::RackMemConvertJsonStr2Map(jsonString, nodeMemoryInfoListMAP)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Map error.";
        return RMRS_ERROR;
    }
    if (!JsonUtil::RackMemConvertJsonStr2Vec(nodeMemoryInfoListMAP["nodeMemoryInfoList"], nodeMemoryInfoListVec)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return RMRS_ERROR;
    }
    this->nodeMemoryInfoList.resize(nodeMemoryInfoListVec.size());
    return RMRS_OK;
}

bool NodeMemoryInfoList::CreateNumaMemInfoVec(JSON_VEC &numaMemInfoVec, const int &i, JSON_MAP &nodeMemoryInfoMap)
{
    if (!JsonUtil::RackMemConvertJsonStr2Vec(nodeMemoryInfoMap["numaMemInfo"], numaMemInfoVec)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return RMRS_ERROR;
    }
    this->nodeMemoryInfoList[i].numaMemInfo.resize(numaMemInfoVec.size());
    for (size_t j = 0; j < numaMemInfoVec.size(); ++j) {
        JSON_MAP numaMemInfoMap;
        auto ret = ParseNumaMemInfoMap(numaMemInfoVec, i, j, numaMemInfoMap);
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "ParseNumaMemInfoMap error.";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}

bool NodeMemoryInfoList::CreateBorrowedItemVec(JSON_VEC &borrowedItemVec, const int &i,
                                               JSON_MAP &borrowedAndLentInfoMap)
{
    if (!JsonUtil::RackMemConvertJsonStr2Vec(borrowedAndLentInfoMap["borrowedItem"], borrowedItemVec)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return RMRS_ERROR;
    }
    this->nodeMemoryInfoList[i].borrowedAndLentInfo.borrowedItem.resize(borrowedItemVec.size());
    for (size_t k = 0; k < borrowedItemVec.size(); ++k) {
        JSON_MAP borrowedItemMap;
        auto ret = ParseBorrowedItemMap(borrowedItemVec, i, k, borrowedItemMap);
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "ParseBorrowedItemMap error.";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}

bool NodeMemoryInfoList::CreateLentItemVec(JSON_VEC &lentItemVec, const int &i, JSON_MAP &borrowedAndLentInfoMap)
{
    if (!JsonUtil::RackMemConvertJsonStr2Vec(borrowedAndLentInfoMap["lentItem"], lentItemVec)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return RMRS_ERROR;
    }
    this->nodeMemoryInfoList[i].borrowedAndLentInfo.lentItem.resize(lentItemVec.size());
    for (size_t v = 0; v < lentItemVec.size(); ++v) {
        JSON_MAP lentItemMap;
        auto ret = ParseLentItemMap(lentItemVec, i, v, lentItemMap);
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "ParseLentItemMap error.";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}

bool NodeMemoryInfoList::FromJson(const std::string &jsonString)
{
    JSON_MAP nodeMemoryInfoListMAP;
    JSON_VEC nodeMemoryInfoListVec;
    auto ret = CreateNodeMemoryInfoListVec(jsonString, nodeMemoryInfoListMAP, nodeMemoryInfoListVec);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "CreateNodeMemoryInfoListVec error.";
        return RMRS_ERROR;
    }
    for (size_t i = 0; i < nodeMemoryInfoListVec.size(); ++i) {
        JSON_MAP nodeMemoryInfoMap;
        auto ret = ParseNodeMemoryInfoMap(nodeMemoryInfoListVec, i, nodeMemoryInfoMap);
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "ParseNodeMemoryInfoMap error.";
            return RMRS_ERROR;
        }
        JSON_VEC numaMemInfoVec;
        ret = CreateNumaMemInfoVec(numaMemInfoVec, i, nodeMemoryInfoMap);
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "CreateNumaMemInfoVec error.";
            return RMRS_ERROR;
        }
        JSON_MAP borrowedAndLentInfoMap;
        borrowedAndLentInfoMap.emplace("borrowedItem", "");
        borrowedAndLentInfoMap.emplace("lentItem", "");

        if (!JsonUtil::RackMemConvertJsonStr2Map(nodeMemoryInfoMap["borrowedAndLentInfo"], borrowedAndLentInfoMap)) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "RackMemConvertJsonStr2Map error.";
            return RMRS_ERROR;
        }
        JSON_VEC borrowedItemVec;
        ret = CreateBorrowedItemVec(borrowedItemVec, i, borrowedAndLentInfoMap);
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "CreateBorrowedItemVec error.";
            return RMRS_ERROR;
        }
        JSON_VEC lentItemVec;
        ret = CreateLentItemVec(lentItemVec, i, borrowedAndLentInfoMap);
        if (ret != RMRS_OK) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "CreateLentItemVec error.";
            return RMRS_ERROR;
        }
    }
    return RMRS_OK;
}

std::string NodeMemoryInfoWithReservedMem::ToString()
{
    std::stringstream ss;
    struct tm tmStruct;
    char timeBuffer[20];
    localtime_r(&timestamp, &tmStruct);
    size_t result = strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tmStruct);
    if (result == 0) {
        return "";
    }
    ss << "timestamp: " << timeBuffer;
    ss << ", nodeId: " << nodeId;
    ss << ", totalMemory: " << totalMemory << "kB";
    ss << ", usedMemory: " << usedMemory << "kB";
    ss << ", freeMemory: " << freeMemory << "kB";
    ss << ", borrowedMemory: " << borrowedMemory << "kB";
    ss << ", lentMemory: " << lentMemory << "kB";
    ss << ", numaMemInfo: [";
    for (size_t i = 0; i < numaMemInfo.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        ss << numaMemInfo[i].ToString();
    }
    ss << "]";
    ss << ", borrowedAndLentInfo.borrowedItem: [";
    for (size_t j = 0; j < borrowedAndLentInfo.borrowedItem.size(); ++j) {
        if (j > 0) {
            ss << ", ";
        }
        ss << borrowedAndLentInfo.borrowedItem[j].ToString();
    }
    ss << "]";
    ss << ", borrowedAndLentInfo.lentItem: [";
    for (size_t k = 0; k < borrowedAndLentInfo.lentItem.size(); ++k) {
        if (k > 0) {
            ss << ", ";
        }
        ss << borrowedAndLentInfo.lentItem[k].ToString();
    }
    ss << "]";
    ss << ", reservedMem: " << reservedMem << "kB";
    ss << ", sharedMem: " << sharedMem << "kB";
    return ss.str();
}

} // namespace mempooling
