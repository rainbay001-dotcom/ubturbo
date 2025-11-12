/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <iostream>
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include "ulog_errno.h"
#include "ulog.h"

namespace utilities {
namespace log {
ULog *ULog::gSingleLogger = nullptr;
thread_local std::string ULog::gLastErrorMessage("");
const int STDOUT_TYPE = 0;
const int FILE_TYPE = 1;
const int STDERR_TYPE = 2;

constexpr int MAX_LOG_LEVEL = 5;
constexpr int FILE_SIZE_MAX = 1000 * 1024 * 1024; // 1GB
constexpr int FILE_SIZE_MIN = 2 * 1024 * 1024;   // 2MB
constexpr int FILE_COUNT_MAX = 50;

int ULog::VerifyUlogParamters(int type, int minLevel, const char *path, int fileSize, int fileCount)
{
    if (type != STDOUT_TYPE && type != FILE_TYPE && type != STDERR_TYPE) {
        gLastErrorMessage = "Invalid log type";
        return UERR_INVALID_PARAM;
    } else if (minLevel < 0 || minLevel > MAX_LOG_LEVEL) {
        gLastErrorMessage = "Invalid min log level";
        return UERR_INVALID_PARAM;
    }

    // for stdout
    if (type == 0) {
        return UOK;
    }

    // for file
    if (path == nullptr) {
        gLastErrorMessage = "Invalid path";
        return UERR_INVALID_PARAM;
    } else if (fileSize > FILE_SIZE_MAX || fileSize < FILE_SIZE_MIN) {
        gLastErrorMessage = "Invalid max file size";
        return UERR_INVALID_PARAM;
    } else if (fileCount > FILE_COUNT_MAX || fileCount < 1) {
        gLastErrorMessage = "Invalid max file count";
        return UERR_INVALID_PARAM;
    }
    return UOK;
}

int ULog::CreateSingleUlog(int type, int minLevel, const char *path, int fileSize, int fileCount)
{
    int ret;
    std::string realPath;
    ULog *logger;

    if (gSingleLogger) {
        return UOK;
    }

    ret = VerifyUlogParamters(type, minLevel, path, fileSize, fileCount);
    if (ret != 0) {
        return ret;
    }

    realPath = std::string(path);

    try {
        logger = new ULog(type, minLevel, realPath, fileSize, fileCount);
    } catch (const std::bad_alloc& e) {
        return UERR_CREATE_FAILED;
    }

    ret = logger->Initialize();
    if (ret != 0) {
        return ret;
    }

    gSingleLogger = logger;
    return UOK;
}

int ULog::LogMessage(int level, const char *prefix, const char *message)
{
    if (!prefix || !message) {
        gLastErrorMessage = "Invalid param";
        return UERR_INVALID_PARAM;
    }
    if (!gSingleLogger) {
        gLastErrorMessage = "Logger uninitialized";
        return UERR_UNINITIALIZED;
    }

    return gSingleLogger->Log(level, prefix, message);
}

int ULog::Initialize()
{
    try {
        if (this->logType == STDOUT_TYPE) {
            this->mSPDLogger = spdlog::stdout_logger_mt("console");
            this->mSPDLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f %t %l %v");
        } else if (this->logType == FILE_TYPE) {
            std::string logName = "log:" + this->filePath;
            this->mSPDLogger = spdlog::rotating_logger_mt(logName.c_str(), this->filePath, this->rotationFileSize,
                                                          this->rotationFileCount);
            this->mSPDLogger->set_pattern("%v");
            this->mSPDLogger->info("", "");
            this->mSPDLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f %t %v");
            this->mSPDLogger->info("Log started at [{}] level",
                                   spdlog::level::to_string_view(static_cast<spdlog::level::level_enum>
                                   (this->minLogLevel)).data());
            this->mSPDLogger->info("Log default format: yyyy-mm-dd hh:mm:ss.uuuuuu threadid loglevel msg");
            this->mSPDLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f %t %l %v");
            spdlog::flush_every(std::chrono::seconds(1));
        } else if (this->logType == STDERR_TYPE) {
            this->mSPDLogger = spdlog::stderr_logger_mt("console");
            this->mSPDLogger->set_pattern("%C/%m/%d %H:%M:%S.%f %t %l %v");
        }
        this->mSPDLogger->set_level(static_cast<spdlog::level::level_enum>(this->minLogLevel));
        this->mSPDLogger->flush_on(spdlog::level::err);

        if (this->minLogLevel < static_cast<int>(spdlog::level::info)) {
            this->debugEnabled = true;
        }
    } catch (const spdlog::spdlog_ex &ex) {
        gLastErrorMessage = "Failed to create log: ";
        gLastErrorMessage += ex.what();
        return UERR_CREATE_FAILED;
    }
    return UOK;
}

int ULog::Log(int level, const char *prefix, const char *message) const
{
    if (!this->mSPDLogger) {
        return UERR_UNINITIALIZED;
    }
    if (level < 0 || level > MAX_LOG_LEVEL) {
        gLastErrorMessage = "Invalid log level";
        return UERR_INVALID_LEVEL;
    }
    this->mSPDLogger->log(static_cast<spdlog::level::level_enum>(level), "{}] {}", prefix, message);
    return UOK;
}

void ULog::Exit()
{
    if (gSingleLogger == nullptr) {
        return;
    }
    if (gSingleLogger->mSPDLogger != nullptr) {
        gSingleLogger->mSPDLogger->flush();
        gSingleLogger->mSPDLogger = nullptr;
    }
    delete gSingleLogger;
    gSingleLogger = nullptr;
}
}
}
