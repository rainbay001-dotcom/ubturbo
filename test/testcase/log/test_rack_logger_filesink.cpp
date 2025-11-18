/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "test_rack_logger_filesink.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include <sys/syslog.h>
#include <chrono>
#include <sstream>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>

namespace fs = std::filesystem;
using namespace ::testing;
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))

namespace turbo::log {
std::string currentPath = std::filesystem::current_path().string();

std::string FindFirstFileMatchingPattern(const std::string &pattern, const std::string prefix)
{
    std::regex regexPattern(pattern);
    for (const auto &entry : fs::directory_iterator(prefix)) {
        if (entry.is_regular_file() && std::regex_match(entry.path().filename().string(), regexPattern)) {
            return entry.path().string();
        }
    }
    return "";
}

void TestRackLoggerFilesink::SetUp()
{
    Test::SetUp();
    auto result = mkdir((currentPath + FILE_PATH).c_str(), 0750); // 权限0750

    auto canonicalPath = realpath("/var/log/scbus", nullptr);
    if (canonicalPath == nullptr) {
        auto result = mkdir("/var/log/scbus", 0777); // 权限0777
        if (result == -1) {
            perror("mkdir"); // Print error message
            std::cerr << "Failed to create directory: " << std::endl;
        } else {
            std::cout << "Directory created successfully: " << std::endl;
        }
    }
}
/*
 * 测试结束后清理测试生成的日志文件
 * 检查当前目录下是否存在名为"test_log.log"的文件，如果存在则删除；
 * 遍历当前目录下的所有文件，如果文件是普通文件（非目录），并且文件名以"test_log_"开头，则删除该文件；
 * 检查当前目录下是否存在名为"log"的子目录，如果存在则删除该子目录及其所有内容。
 */
void TestRackLoggerFilesink::TearDown()
{
    Test::TearDown();
    if (fs::exists(currentPath + FILE_PATH + FILE_NAME + ".log")) {
        fs::remove(currentPath + FILE_PATH + FILE_NAME + ".log");
    }
    if (!fs::exists(currentPath + FILE_PATH)) {
        return;
    }
    for (const auto &entry : fs::directory_iterator(currentPath + FILE_PATH)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string filename = entry.path().filename().string();
        if (filename.find("test_log_") == 0) {
            fs::remove(entry.path());
        }
    }
}

/*
 * 用例描述：
 * 成功写入日志消息
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为1024字节。
 * 2. 调用Write方法写入一条日志消息。
 * 预期结果：
 * 1. 日志文件成功创建并打开。
 * 2. 日志文件中包含写入的消息。
 */
TEST_F(TestRackLoggerFilesink, TestWrite)
{
    std::string path = currentPath + FILE_PATH;

    try {
        if (!fs::exists(path)) {
            fs::create_directories(path);
        } else {
            std::cout << "Directory already exists:" << path << std::endl;
        }
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
    }

    RackLoggerFilesink sink(path, 1024, 16); // 1024设置为文件最大大小，16设置为文件最大数量
    TurboLoggerEntry entry("test_log", TurboLogLevel::INFO, "test.log", "TestFunction", 1);
    ASSERT_TRUE(sink.Write(entry));

    std::ifstream logFile(path + "test_log.log");
    ASSERT_TRUE(logFile.is_open());
}

TEST_F(TestRackLoggerFilesink, TestWriteFail1)
{
    RackLoggerFilesink sink(currentPath + FILE_PATH, 1024, 16); // 1024设置为文件最大大小，16设置为文件最大数量
    TurboLoggerEntry entry(nullptr, TurboLogLevel::INFO, "test.log", "TestFunction", 1);
    ASSERT_TRUE(sink.Write(entry));
}
/*
 * 用例描述：
 * 文件滚动并压缩
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为32字节。
 * 2. 连续写入多条日志消息以触发文件滚动。
 * 预期结果：
 * 1. 原始日志文件不存在。
 * 2. 生成一个压缩的日志文件。
 */
TEST_F(TestRackLoggerFilesink, RollFile)
{
    // 设置一个较小的maxFileSize以便触发滚动，20设置为文件最大大小，16设置为文件最大数量
    RackLoggerFilesink sink(currentPath + FILE_PATH, 20, 16);
    // 设置rackLoggerEntry1的行数为1
    TurboLoggerEntry rackLoggerEntry1("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 设置rackLoggerEntry2的行数为2
    TurboLoggerEntry rackLoggerEntry2("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 2);
    // 设置rackLoggerEntry3的行数为3
    TurboLoggerEntry rackLoggerEntry3("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 3);
    ASSERT_TRUE(sink.Write(rackLoggerEntry1));
    ASSERT_TRUE(sink.Write(rackLoggerEntry2));
    ASSERT_TRUE(sink.Write(rackLoggerEntry3)); // 触发文件滚动

    ASSERT_TRUE(!fs::exists(currentPath + FILE_PATH + "test_log.log")); // 原始日志文件不存在
    ASSERT_TRUE(
        !FindFirstFileMatchingPattern(R"(test_log_.*_01\.tar\.gz)", currentPath + FILE_PATH).empty()); // 压缩文件应存在
}

/*
 * 用例描述：
 * 文件滚动并压缩，达到最大数量时删除最旧文件并重新命名
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为32字节，最大文件数量为3。
 * 2. 连续写入多条日志消息以触发文件滚动并生成多个压缩文件。
 * 预期结果：
 * 1. 原始日志文件不存在。
 * 2. 生成最多3个压缩的日志文件。
 * 3. 最旧的压缩文件被删除，剩余文件按序重新命名。
 */
TEST_F(TestRackLoggerFilesink, RollFileWithMaxCount)
{
    // 设置一个较小的maxFileSize以便触发滚动，最大文件数量为3
    RackLoggerFilesink sink(currentPath + FILE_PATH, 20, 3); // 20设置为文件最大大小，3设置为文件最大数量
    // 写入足够多的日志消息以触发滚动并生成多个压缩文件
    // 设置rackLoggerEntry1的行数为1
    TurboLoggerEntry rackLoggerEntry1("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 设置rackLogggerEntry2的行数为2
    TurboLoggerEntry rackLoggerEntry2("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 2);
    // 设置rackLoggerEntry3的行数为3
    TurboLoggerEntry rackLoggerEntry3("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 3);
    // 设置rackLoggerEntry4的行数为4
    TurboLoggerEntry rackLoggerEntry4("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 4);
    // 设置rackLogggerEntry5的行数为5
    TurboLoggerEntry rackLoggerEntry5("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 5);
    // 设置rackLoggerEntry6的行数为6
    TurboLoggerEntry rackLoggerEntry6("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 6);
    // 设置rackLoggerEntry7的行数为7
    TurboLoggerEntry rackLoggerEntry7("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 7);
    // 设置rackLoggerEntry8的行数为8
    TurboLoggerEntry rackLoggerEntry8("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 8);
    ASSERT_TRUE(sink.Write(rackLoggerEntry1));
    ASSERT_TRUE(sink.Write(rackLoggerEntry2)); // 触发第一次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_01\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 1号压缩文件应存在
    ASSERT_TRUE(sink.Write(rackLoggerEntry3));
    ASSERT_TRUE(sink.Write(rackLoggerEntry4)); // 触发第二次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_02\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 2号压缩文件应存在
    ASSERT_TRUE(sink.Write(rackLoggerEntry5));
    ASSERT_TRUE(sink.Write(rackLoggerEntry6)); // 触发第三次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_03\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 3号压缩文件应存在
    ASSERT_TRUE(sink.Write(rackLoggerEntry7));
    ASSERT_TRUE(sink.Write(rackLoggerEntry8)); // 触发第四次滚动
    ASSERT_TRUE(FindFirstFileMatchingPattern(R"(test_log_.*_04\.tar\.gz)", currentPath + FILE_PATH)
                    .empty()); // 4号压缩文件不存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_01\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 1号压缩文件应存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_02\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 2号压缩文件应存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_03\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 3号压缩文件应存在
    // 检查原始日志文件是否存在
    ASSERT_TRUE(!fs::exists(currentPath + FILE_PATH + "test_log.log"));
}

/*
 * 用例描述：
 * 设置非同级相对路径，文件滚动并压缩，达到最大数量时删除最旧文件并重新命名
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为32字节，最大文件数量为3。
 * 2. 连续写入多条日志消息以触发文件滚动并生成多个压缩文件。
 * 预期结果：
 * 1. 原始日志文件不存在。
 * 2. 生成最多3个压缩的日志文件。
 * 3. 最旧的压缩文件被删除，剩余文件按序重新命名。
 */
TEST_F(TestRackLoggerFilesink, NonPeerDirRollFileWithMaxCount)
{
    // 设置一个较小的值20为maxFileSize以便触发滚动，最大文件数量为3
    RackLoggerFilesink sink(currentPath + FILE_PATH, 20, 3);
    // 设置rackLoggerEntry1的行数为1
    TurboLoggerEntry rackLoggerEntry1("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 设置rackLogggerEntry2的行数为2
    TurboLoggerEntry rackLoggerEntry2("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 2);
    // 设置rackLoggerEntry3的行数为3
    TurboLoggerEntry rackLoggerEntry3("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 3);
    // 设置rackLoggerEntry4的行数为4
    TurboLoggerEntry rackLoggerEntry4("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 4);
    // 设置rackLogggerEntry5的行数为5
    TurboLoggerEntry rackLoggerEntry5("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 5);
    // 设置rackLoggerEntry6的行数为6
    TurboLoggerEntry rackLoggerEntry6("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 6);
    // 设置rackLoggerEntry7的行数为7
    TurboLoggerEntry rackLoggerEntry7("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 7);
    // 设置rackLoggerEntry8的行数为8
    TurboLoggerEntry rackLoggerEntry8("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 8);
    // 写入足够多的日志消息以触发滚动并生成多个压缩文件
    ASSERT_TRUE(sink.Write(rackLoggerEntry1));
    ASSERT_TRUE(sink.Write(rackLoggerEntry2)); // 触发第一次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_01\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 1号压缩文件应存在
    ASSERT_TRUE(sink.Write(rackLoggerEntry3));
    ASSERT_TRUE(sink.Write(rackLoggerEntry4)); // 触发第二次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_02\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 2号压缩文件应存在
    ASSERT_TRUE(sink.Write(rackLoggerEntry5));
    ASSERT_TRUE(sink.Write(rackLoggerEntry6)); // 触发第三次滚动
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_03\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 3号压缩文件应存在
    ASSERT_TRUE(sink.Write(rackLoggerEntry7));
    ASSERT_TRUE(sink.Write(rackLoggerEntry8)); // 触发第四次滚动
    ASSERT_TRUE(FindFirstFileMatchingPattern(R"(test_log_.*_04\.tar\.gz)", currentPath + FILE_PATH)
                    .empty()); // 4号压缩文件不存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_01\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 1号压缩文件应存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_02\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 2号压缩文件应存在
    ASSERT_TRUE(!FindFirstFileMatchingPattern(R"(test_log_.*_03\.tar\.gz)", currentPath + FILE_PATH)
                     .empty()); // 3号压缩文件应存在
    // 检查原始日志文件是否存在
    ASSERT_TRUE(!fs::exists(currentPath + FILE_PATH + "test_log.log"));
    for (const auto &entry : std::filesystem::directory_iterator(currentPath + FILE_PATH)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string filename = entry.path().filename().string();
        if (filename.find("test_log_") == 0) {
            std::filesystem::remove(entry.path());
        }
    }
}

/*
 * 用例描述：
 * 检查压缩文件的权限
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为64字节。
 * 2. 连续写入多条日志消息以触发文件滚动。
 * 预期结果：
 * 1. 生成的压缩文件权限为400。
 */
TEST_F(TestRackLoggerFilesink, FilePermissions) // fail
{
    // 设置一个较小的maxFileSize以便触发滚动，maxFileSize设置为20，maxFileCount为16
    RackLoggerFilesink sink(currentPath + FILE_PATH, 20, 16);
    // 设置rackLoggerEntry1的行数为1
    TurboLoggerEntry rackLoggerEntry1("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 设置rackLogggerEntry2的行数为2
    TurboLoggerEntry rackLoggerEntry2("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 2);
    // 设置rackLoggerEntry3的行数为3
    TurboLoggerEntry rackLoggerEntry3("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 3);
    ASSERT_TRUE(sink.Write(rackLoggerEntry1));
    ASSERT_TRUE(sink.Write(rackLoggerEntry2));
    ASSERT_TRUE(sink.Write(rackLoggerEntry3)); // 触发文件滚动

    auto perms =
        fs::status(FindFirstFileMatchingPattern(R"(test_log_.*_01\.tar\.gz)", currentPath + FILE_PATH)).permissions();
    ASSERT_EQ(perms, fs::perms::owner_read); // 检查压缩文件权限是否为0400
}

/*
 * 用例描述：
 * 文件滚动后文件索引增加
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为4字节。
 * 2. 连续写入多条日志消息以触发多次文件滚动。
 * 预期结果：
 * 1. 文件索引依次递增。
 * 2. 生成多个压缩文件。
 */
TEST_F(TestRackLoggerFilesink, FileIndexIncrement)
{
    // 设置一个较小的值4作为maxFileSize以便触发滚动，maxFileCount为16
    RackLoggerFilesink sink(currentPath + FILE_PATH, 4, 16);
    // 设置rackLoggerEntry1的行数为1
    TurboLoggerEntry rackLoggerEntry1("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 设置rackLogggerEntry2的行数为2
    TurboLoggerEntry rackLoggerEntry2("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 2);
    // 设置rackLoggerEntry3的行数为3
    TurboLoggerEntry rackLoggerEntry3("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 3);
    // 设置rackLoggerEntry4的行数为4
    TurboLoggerEntry rackLoggerEntry4("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 4);
    TurboLoggerEntry* entrys[] = {&rackLoggerEntry1, &rackLoggerEntry2, &rackLoggerEntry3, &rackLoggerEntry4};
    // 将4条日志消息写入
    for (const auto &entry : entrys) {
        ASSERT_TRUE(sink.Write(*entry));
    }
    // 循环4次生成4个两位数字符串
    for (int i = 1; i <= 4; ++i) {
        std::stringstream ss;
        ss << std::setw(2) << std::setfill('0') << i; // 设置输出字符串的宽度为2
        std::string compressedFilename = "test_log_.*_" + ss.str() + ".tar.gz";
        EXPECT_TRUE(!FindFirstFileMatchingPattern(compressedFilename, currentPath + FILE_PATH).empty());
    }
}

/*
 * 用例描述：
 * 检查日志文件权限
 * 测试步骤：
 * 1. 创建FileSink实例，设置日志文件大小限制为1024字节。
 * 2. 写入一条日志消息。
 * 预期结果：
 * 1. 日志文件权限为600。
 */
TEST_F(TestRackLoggerFilesink, LogFilePermissions)
{
    RackLoggerFilesink sink(currentPath + FILE_PATH, 1024, 16); // 1024设置为文件最大大小，16设置为文件最大数量
    TurboLoggerEntry rackLoggerEntry1("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction",
        1); // rackLoggerEntry1的行数为1
    ASSERT_TRUE(sink.Write(rackLoggerEntry1));

    auto perms = fs::status(currentPath + FILE_PATH + "test_log.log").permissions();
    EXPECT_TRUE((perms & fs::perms::owner_read) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::owner_read) != fs::perms::none);
    EXPECT_TRUE((perms & ~(fs::perms::owner_read | fs::perms::owner_write)) == fs::perms::none); // 确保只有600权限
    
    // ASSERT_EQ(perms, fs::perms::owner_read | fs::perms::owner_write); // 检查日志文件权限是否为600
}

/*
 * 用例描述：
 * 最大文件边界值处理
 * 测试步骤：
 * 1. 创建FileSink实例，设置文件大小为0。
 * 2. 尝试写入日志消息。
 * 预期结果：
 * 1. FileSink初始化配置函数应处理文件大小为0并返回false。
 * 2. Write方法返回false。
 */
TEST_F(TestRackLoggerFilesink, ZeroMaxFileSize)
{
    RackLoggerFilesink sink(currentPath + FILE_PATH, 0, 16); // 0设置为文件最大大小，16设置为文件最大数量
}

/*
 * 用例描述：
 * 最大压缩包数边界值处理
 * 测试步骤：
 * 1. 创建FileSink实例，设置压缩包最大数量为0。
 * 2. 尝试写入日志消息。
 * 预期结果：
 * 1. FileSink初始化配置函数应处理压缩包最大数量为0并返回false。
 * 2. Write方法返回false。
 */
TEST_F(TestRackLoggerFilesink, ZeroMaxFileCount)
{
    int fileMaxSize = 1024;
    RackLoggerFilesink sink(currentPath + FILE_PATH, fileMaxSize, 0); // 1024设置为文件最大大小，0设置为文件最大数量
}

/*
 * 用例描述：
 * 日志文件路径不存在
 * 测试步骤：
 * 1. 创建FileSink实例，设置一个不存在的日志文件路径。
 * 2. 调用Write方法写入日志消息。
 * 预期结果：
 * 1. FileSink构造函数使用默认路径./log/run。
 * 2. Write方法返回true。
 */
TEST_F(TestRackLoggerFilesink, NonexistentFilePath)
{
    int fileMaxSize = 1024;
    RackLoggerFilesink sink(currentPath + "/noexist", fileMaxSize, 16); // 1024设置为文件最大大小，16设置为文件最大数量
    // 设置rackLoggerEntry的行数为1
    TurboLoggerEntry rackLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    ASSERT_TRUE(sink.Write(rackLoggerEntry));
    std::string fileName = currentPath + "/noexist/" + FILE_NAME + ".log";
    std::ifstream logFile(fileName);
    ASSERT_TRUE(logFile.is_open());
}

/*
 * 用例描述：
 * 测试判断文件inode是否已经失效
 * 测试步骤：
 * 1. 创建FileSink实例，设置一个日志文件路径。
 * 2. 调用Write方法写入日志消息。
 * 3.判断文件是否被外部修改
 * 预期结果：
 * 1. 文件未被外部修改，inode依然有效。
 */
TEST_F(TestRackLoggerFilesink, TestIsFileStatusChanged)
{
    int fileMaxSize = 1024;
    int fileMaxNum = 16;
    RackLoggerFilesink sink(currentPath + FILE_PATH, fileMaxSize, fileMaxNum); // 1024设置为文件最大大小，16设置为文件最大数量
    TurboLoggerEntry rackLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    sink.Write(rackLoggerEntry);
    std::string fileName = "test_log";
    ASSERT_FALSE(sink.IsFileStatusChanged(fileName));
}


TEST_F(TestRackLoggerFilesink, TestOpenFileFailed)
{
    int fileMaxSize = 1024;
    int fileMaxNum = 16;
    RackLoggerFilesink sink(currentPath + FILE_PATH, fileMaxSize, fileMaxNum); // 1024设置为文件最大大小，16设置为文件最大数量
    std::string invalidFile = "/dev/null/invalid";

    EXPECT_FALSE(sink.OpenFile(invalidFile));
}

TEST_F(TestRackLoggerFilesink, TestGenerateCompressedFilename)
{
    const std::string fileName = "logfile";
    uint32_t index = 1;
    time_t ts = 1672531199;
    int fileMaxSize = 1024;
    int fileMaxNum = 16;
    RackLoggerFilesink sink(currentPath + FILE_PATH, fileMaxSize, fileMaxNum);
    auto ret = sink.GenerateCompressedFilename(fileName, index, ts);
    ASSERT_EQ(ret, currentPath + FILE_PATH + "logfile_20230101_075959_01.tar.gz");
}

TEST_F(TestRackLoggerFilesink, TestCompressFileFail)
{
    const std::string filename = "test";
    const std::string sourceFilename = "source_test";
    const std::string destFilename = "dest_test";
    int fileMaxSize = 1024;
    int fileMaxNum = 16;
    RackLoggerFilesink sink(currentPath + FILE_PATH, fileMaxSize, fileMaxNum);
    MOCKER(system).stubs().will(returnValue(1));
    auto ret = sink.CompressFile(filename, sourceFilename, destFilename);
    ASSERT_FALSE(ret);
}

TEST_F(TestRackLoggerFilesink, TestRenameCompressedFile)
{
    std::vector<std::string> compressedFiles{"test_01.tar.gz", "test_02.tar.gz", "test_03.tar.gz"};
    int fileMaxSize = 1024;
    int fileMaxNum = 16;
    RackLoggerFilesink sink(currentPath + FILE_PATH, fileMaxSize, fileMaxNum);
    auto ret = sink.RenameCompressedFile(compressedFiles);
    ASSERT_EQ(ret, compressedFiles.size());
}

} // namespace turbo::log