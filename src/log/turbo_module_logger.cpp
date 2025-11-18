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
#include "turbo_module_logger.h"

#include <new>                     // for nothrow
#include <string>                  // for basic_string
#include "rack_logger_filesink.h"  // for RackLoggerFilesink
#include "rack_logger_manager.h"   // for RackLoggerManager
#include "rack_logger_writer.h"    // for LoggerOptions, RackLoggerWriter

#include "turbo_conf.h"
#include "turbo_error.h"

namespace turbo::log {
static std::unordered_map<std::string, uint32_t> logLevelMap = {
    {"DEBUG", 0}, {"INFO", 1}, {"WARN", 2}, {"ERROR", 3}, {"CRIT", 4}};

RetCode TurboModuleLogger::Init()
{
    LoggerOptions options;
    std::string level;
    turbo::config::UBTurboGetStr("ubturbo", "log.level", level);
    auto iter = logLevelMap.find(level);
    if (iter == logLevelMap.end()) {
        return TURBO_ERROR;
    }
    options.minLogLevel = static_cast<TurboLogLevel>(logLevelMap[level]);
    RackLoggerWriter *writer = new (std::nothrow)
        RackLoggerFilesink(options.logPath, options.maxFileSizeInMB * NO_1024 * NO_1024, options.rotationFileCount);
    if (writer == nullptr) {
        return TURBO_ERROR;
    }
    auto ret = RackLoggerManager::Instance()->Init(options, writer);
    if (ret != TURBO_OK) {
        if (writer != nullptr) {
            delete writer;
            writer = nullptr;
        }
        return ret;
    }
    return TURBO_OK;
}

RetCode TurboModuleLogger::Start()
{
    return TURBO_OK;
}

void TurboModuleLogger::Stop() {}

void TurboModuleLogger::UnInit()
{
    // 日志需要最后停
    if (RackLoggerManager::gInstance != nullptr) {
        turbo::log::RackLoggerManager::Destroy();
    }
}

std::string TurboModuleLogger::Name()
{
    return "logger";
}
} // namespace turbo::log