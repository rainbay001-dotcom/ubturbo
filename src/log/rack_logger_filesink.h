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
#ifndef RACK_FILE_SINK_H
#define RACK_FILE_SINK_H

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_map>
#include "rack_logger_writer.h"
#include "turbo_logger.h"

namespace turbo::log {
namespace fs = std::filesystem;
struct RackLogFile {
    bool isInitialized = false;
    fs::path filePath;
    uint32_t fileIndex = 1;
    std::ofstream logFile;
    ino_t inode = 0;
};

class RackLoggerFilesink : public RackLoggerWriter {
public:
    /* *
     * @brief 构造函数，初始化日志文件路径和文件滚动配置
     * @param[in] basePath 日志文件的所在目录
     * @param[in] maxFileSize 单个日志文件的最大大小（M字节）
     * @param[in] maxFileCount 最大保留的压缩日志文件数量
     */
    RackLoggerFilesink(std::string basePath, uint32_t maxFileSize, uint32_t maxFileCount);

    /* *
     * @brief 析构函数，清理资源
     */
    ~RackLoggerFilesink() override;

    /* *
     * @brief 写入日志消息到文件
     * @param[in] loggerEntry 要写入的日志消息
     * @return 如果写入成功返回 true，否则返回 false
     */
    bool Write(TurboLoggerEntry &loggerEntry) override;

private:
    bool IsFileStatusChanged(const std::string &fileName);

    /* *
     * @brief 触发文件滚动操作
     * @param[in] fileName 日志文件名
     * @return 如果滚动成功返回 true，否则返回 false
     */
    bool RollFile(const std::string &fileName);

    /* *
     * @brief 生成压缩日志文件的文件名
     * @param[in] fileName 日志文件名
     * @param[in] index 压缩文件的序号
     * @param[in] timeStamp 时间戳，用于生成唯一文件名
     * @return 生成的压缩日志文件名
     */
    std::string GenerateCompressedFilename(const std::string &fileName, uint32_t index, time_t timeStamp);

    /* *
     * @brief 打开日志文件进行写入
     * @param[in] fileName 日志文件名
     * @return 如果文件打开成功返回 true，否则返回 false
     */
    bool OpenFile(const std::string &fileName);

    /* *
     * @brief 压缩日志文件
     * @param[in] fileName 日志文件名
     * @param[in] sourceFilename 源日志文件名
     * @param[in] destFilename 目标压缩文件名
     * @return 如果压缩成功返回 true，否则返回 false
     */
    bool CompressFile(const std::string &fileName, const std::string &sourceFilename, const std::string &destFilename);

    /* *
     * @brief 重命名压缩包名
     * @param[in] compressedFiles 压缩包名数组
     */
    uint32_t RenameCompressedFile(std::vector<std::string> &compressedFiles);

    /* *
     * @brief 管理日志文件的滚动和压缩
     * @param[in] fileName 日志文件名
     */
    void ManageFileRotation(const std::string &fileName);

private:
    const std::string filenameSuffixPattern = R"(_\d{8}_\d{6}_\d{2}\.tar\.gz)";
    std::unordered_map<std::string, RackLogFile> fileMap;
    std::string basePath;
    uint32_t maxFileSize;
    uint32_t maxFileCount;
};
}

#endif // RACK_FILE_SINK_H