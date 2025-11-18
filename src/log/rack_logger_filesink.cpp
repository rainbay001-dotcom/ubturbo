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
#include "rack_logger_filesink.h"
#include <sys/stat.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include "rack_common_def.h"

namespace turbo::log {
using namespace rack::common::def;
RackLoggerFilesink::RackLoggerFilesink(std::string basePath, uint32_t maxFileSize, uint32_t maxFileCount)
    : basePath(std::move(basePath)),
      maxFileSize(maxFileSize),
      maxFileCount(maxFileCount)
{
    auto canonicalPath = realpath(this->basePath.c_str(), nullptr);
    if (canonicalPath == nullptr) {
        std::cerr << "FilePath does not exist: " << this->basePath << std::endl;
        auto result = mkdir(this->basePath.c_str(), 0700); // 权限0700
        if (result == -1) {
            perror("mkdir"); // Print error message
            std::cerr << "Failed to create directory: " << std::endl;
        }
    } else {
        this->basePath = canonicalPath;
    }
    free(canonicalPath);
}

RackLoggerFilesink::~RackLoggerFilesink()
{
    for (auto &it : fileMap) {
        if (it.second.logFile.is_open()) {
            it.second.logFile.close();
        }
    }
}

bool RackLoggerFilesink::Write(TurboLoggerEntry &loggerEntry)
{
    const char* moduleName = loggerEntry.GetModuleName();
    if (moduleName == nullptr) {
        std::cerr << "LoggerEntry moduleName is nullptr." << std::endl;
        return false;
    }
    
    std::string fileName = moduleName;
    if (!fileMap[fileName].isInitialized) {
        fileMap[fileName].filePath = basePath + "/" + fileName + ".log";
        fileMap[fileName].isInitialized = true;
    }

    bool needOpen = !fileMap[fileName].logFile.is_open() || IsFileStatusChanged(fileName) ||
                    !fileMap[fileName].logFile.good();
    if (needOpen) {
        fileMap[fileName].logFile.close();
        if (!OpenFile(fileName)) {
            std::cerr << "Open file failed" << std::endl;
            return false;
        }
    }

    loggerEntry.OutPutLog(fileMap[fileName].logFile);

    uint32_t currentFileSize = 0;
    try {
        currentFileSize = fs::file_size(fileMap[fileName].filePath);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    if (currentFileSize > maxFileSize) {
        if (!RollFile(fileName)) {
            std::cerr << "Failed rolling file" << std::endl;
            return false;
        }
    }
    return true;
}

bool RackLoggerFilesink::IsFileStatusChanged(const std::string &fileName)
{
    struct stat fileStat {};
    if (stat(fileMap[fileName].filePath.c_str(), &fileStat) != 0) {
        return true; // 文件不存在或无法访问
    }
    bool isChanged = (fileMap[fileName].inode != fileStat.st_ino);
    return isChanged;
}

bool RackLoggerFilesink::RollFile(const std::string &fileName)
{
    ManageFileRotation(fileName);
    time_t currentTime = std::time(nullptr);
    std::string compressedFilename = GenerateCompressedFilename(fileName, fileMap[fileName].fileIndex, currentTime);
    if (!CompressFile(fileName, fileMap[fileName].filePath, compressedFilename)) {
        std::cerr << "Failed compressing file" << std::endl;
        return false;
    }
    return true;
}

std::string RackLoggerFilesink::GenerateCompressedFilename(const std::string &fileName, uint32_t index,
                                                           time_t timeStamp)
{
    std::ostringstream oss;

    // 获取当前时间信息
    struct tm *ptimeinfo = localtime(&timeStamp);
    if (ptimeinfo == nullptr) {
        std::cerr << "Error: localtime failed for timestamp " << timeStamp << std::endl;
    }

    // 生成文件名
    oss << basePath << "/" << fileName << "_" << std::setw(4) << std::setfill('0') << // 年份格式占4位
        (ptimeinfo->tm_year + 1900) << std::setw(2) << std::setfill('0') << // 月份格式占2位 起始年份1900
        (ptimeinfo->tm_mon + 1) << std::setw(2) << std::setfill('0') << ptimeinfo->tm_mday // 日期格式占2位
        << "_" << std::setw(2) << std::setfill('0') << ptimeinfo->tm_hour                  // 小时格式占2位
        << std::setw(2) << std::setfill('0') << ptimeinfo->tm_min                          // 分钟格式占2位
        << std::setw(2) << std::setfill('0') << ptimeinfo->tm_sec                          // 秒格式占2位
        << "_" << std::setw(2) << std::setfill('0') << index                               // 序号格式占2位
        << ".tar.gz";

    return oss.str();
}

bool RackLoggerFilesink::OpenFile(const std::string &fileName)
{
    fs::path filePath = fileMap[fileName].filePath;
    try {
        fileMap[fileName].logFile.open(filePath, std::ios::out | std::ios::app);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }

    if (fileMap[fileName].logFile.is_open()) {
        struct stat fileStat {};
        stat(fileMap[fileName].filePath.c_str(), &fileStat);
        fileMap[fileName].inode = fileStat.st_ino;
        try {
            fs::permissions(filePath, fs::perms::owner_read | fs::perms::owner_write); // 设置权限为600
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }
        return true;
    } else {
        std::cerr << "Failed to open log file: " << filePath << std::endl;
        return false;
    }
}

bool RackLoggerFilesink::CompressFile(const std::string &fileName, const std::string &sourceFilename,
                                      const std::string &destFilename)
{
    std::string command = "tar -czf " + destFilename + " -C " + basePath + " " + fileName + ".log";
    int result = system(command.c_str());
    if (result != 0) {
        std::cerr << "Failed to compress file using system command" << std::endl;
        return false;
    }
    try {
        fs::permissions(destFilename, fs::perms::owner_read); // 设置权限为400
        fs::remove(sourceFilename);
        fileMap[fileName].logFile.close();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return true;
}


uint32_t RackLoggerFilesink::RenameCompressedFile(std::vector<std::string> &compressedFiles)
{
    uint32_t currentFilecount = compressedFiles.size();
    for (uint32_t i = 0; i < currentFilecount; i++) {
        std::smatch match;
        if (std::regex_search(compressedFiles[i], match, std::regex(R"(_(\d{2})\.tar\.gz)"))) {
            uint32_t newIndex = i + 1;
            std::ostringstream oss;
            oss << std::setw(2) << std::setfill('0') << newIndex; // 匹配压缩包名2位序号

            std::string newFilename = compressedFiles[i];
            newFilename.replace(match.position(1), match.length(1), oss.str());

            try {
                fs::rename(compressedFiles[i], newFilename);
                compressedFiles[i] = std::move(newFilename);
            } catch (const std::exception &e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
    }
    return currentFilecount;
}

void RackLoggerFilesink::ManageFileRotation(const std::string &fileName)
{
    // 存储匹配到的文件
    std::vector<std::string> compressedFiles;
    fs::path filePath = fs::path(basePath + "/" + fileName);
    fs::path dir = filePath.parent_path();

    std::regex filenamePattern(filePath.string() + filenameSuffixPattern);

    for (const auto &entry : fs::directory_iterator(dir)) {
        if (fs::is_regular_file(entry) && std::regex_match(entry.path().string(), filenamePattern)) {
            compressedFiles.push_back(entry.path().string());
        }
    }

    // 按照文件名中的序号进行排序
    std::sort(compressedFiles.begin(), compressedFiles.end());
    fileMap[fileName].fileIndex = RenameCompressedFile(compressedFiles) + 1;

    if (compressedFiles.size() < maxFileCount) {
        return;
    }
    uint32_t currentFilecount = compressedFiles.size();
    for (uint32_t i = 0; i < currentFilecount - maxFileCount + 1; i++) {
        std::string oldestFile = compressedFiles.front();
        fs::remove(oldestFile);
        compressedFiles.erase(compressedFiles.begin());
    }

    // 重新命名剩余的文件
    RenameCompressedFile(compressedFiles);

    // 将新文件的索引设置为最大
    fileMap[fileName].fileIndex = maxFileCount;
}
} // namespace turbo::log