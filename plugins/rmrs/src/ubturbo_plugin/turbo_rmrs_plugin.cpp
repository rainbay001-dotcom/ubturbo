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
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>

#include "callback_manager.h"
#include "iostream"
#include "rmrs_config.h"
#include "rmrs_error.h"
#include "rmrs_migrate_module.h"
#include "rmrs_resource_export.h"
#include "rmrs_smap_helper.h"
#include "turbo_conf.h"
#include "turbo_logger.h"
#include "rmrs_smap_module.h"
#include "rmrs_serialize.h"
#include "bottleneck_detector.h"

using namespace rmrs::exports;
using namespace turbo::config;
using namespace turbo::log;
using namespace rmrs;
using namespace std;
using namespace rmrs::smap;
using namespace rmrs::migrate;
using namespace rmrs::serialize;

extern "C" {
uint32_t TurboPluginInit(const uint16_t modCode);
void TurboPluginDeInit();
void RmrsReboot();
}

static const std::string MIGREATE_RECORD_PATH = "/opt/ubturbo/";
static const std::string MIGREATE_RECORD_FILE = "/opt/ubturbo/migrate_record";

void RmrsReboot()
{
    if (!std::filesystem::exists(MIGREATE_RECORD_FILE)) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit][RmrsReboot] No Record file.";
        return ;
    }
    std::ifstream fin;
    fin.open(MIGREATE_RECORD_FILE, std::ios::in | std::ios::binary);
    if (!fin.is_open()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit][RmrsReboot] Record file open failed.";
        return ;
    }
    std::stringstream temp;
    temp << fin.rdbuf();
    std::string text = temp.str();
    fin.close();
    if (text.length() == 0) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit][RmrsReboot] Record file is empty.";
        return ;
    }

    rmrs::serialization::MigrateStrategyResult migrateStrategyResult;

    RmrsInStream builder(text);
    builder >> migrateStrategyResult;
    std::vector<uint16_t> remoteNumaIdsIn;
    for (auto &item : migrateStrategyResult.vmInfoList) {
        remoteNumaIdsIn.push_back(item.desNumaId);
    }
    std::vector<pid_t> pidsIn;
    std::vector<uint64_t> memSizeList;
    for (auto &item : migrateStrategyResult.vmInfoList) {
        pidsIn.push_back(item.pid);
        memSizeList.push_back(0);
    }
    
    auto ret = RmrsSmapHelper::MigrateColdDataToRemoteNumaSync(remoteNumaIdsIn, pidsIn, memSizeList,
        migrateStrategyResult.waitingTime);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsInit][RmrsReboot] MigrateColdDataToRemoteNumaSync failed.";
    }
    std::ofstream fout;
    fout.open(MIGREATE_RECORD_FILE, std::ios::out | std::ios::binary);
    fout.close();
    return ;
}

uint32_t TurboPluginInit(const uint16_t modCode)
{
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit] Start to init rmrs plugin.";
    // 配置文件初始化
    RmrsResult ret = RmrsConfig::Instance().Init(modCode);
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit] Rmrs config init failed." << ret;
        return RMRS_ERROR;
    }

    // 回调函数初始化
    ret = CallbackManager::Init();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit] Rmrs config init failed." << ret;
        return RMRS_ERROR;
    }

    auto resourceExport = rmrs::exports::ResourceExport();
    ret = resourceExport.Init();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit] Resource export init failed." << ret;
        return RMRS_ERROR;
    }

    ret = RmrsSmapHelper::Init();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit] Rmrs SmapModule init failed." << ret;
        return RMRS_ERROR;
    }

    if (RmrsConfig::Instance().GetRmrsUcacheEnable()) {
        UBTURBO_LOG_DEBUG(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit] Rmrs UCache enabled.";
        ucache::bottleneck_detector::BottleneckDetector::GetInstance().Init();
    }

    RmrsReboot();

    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit] RMRS plugin initialization succeed.";

    return RMRS_OK;
}

void TurboPluginDeInit()
{
    RmrsSmapHelper::Deinit();
    if (RmrsConfig::Instance().GetRmrsUcacheEnable()) {
        ucache::bottleneck_detector::BottleneckDetector::GetInstance().Deinit();
    }
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsInit] Plugin rmrs deinit.";
}