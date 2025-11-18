/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "securec.h"
#define private public
#include "turbo_logger.h"
#include "rack_logger_manager.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI(#api, reinterpret_cast<TT>(api))
namespace turbo::log {
class TestRackLogger : public testing::Test {
public:
    void SetUp(void) override
    {
        Test::SetUp();
    }
    void TearDown(void) override
    {
        Test::TearDown();
        GlobalMockObject::verify();
    }
};

std::string DeleteOther(std::string str)
{
    std::regex pattern("\\[[^\\[\\]]*\\]");
    // 删除匹配的方括号
    int count = 0;
    while (count < 5 && std::regex_search(str, pattern)) { // 删除5对“[]"
        str = std::regex_replace(str, pattern, "", std::regex_constants::format_first_only);
        count++;
    }
    if (!str.empty() && str[0] == ' ') {
        str.erase(0, 1); // 删除第一个字符
    }
    return str;
}

/*
 * 用例描述：
 * 测试运算符重载
 */
TEST_F(TestRackLogger, TestoperatorChar_success)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    char data = 'a'; // 设置要写入的char型数据为a
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "a\n");
}
TEST_F(TestRackLogger, TestoperatorInt_success)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::DEBUG, "Test.log", "TestFunction", 1);
    int32_t data = -12345; // 设置要写入的int32_t型数为-12345
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "-12345\n");
}
TEST_F(TestRackLogger, TestoperatorUint32_success)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::WARN, "Test.log", "TestFunction", 1);
    uint32_t data = 12345; // 设置要写入的uint32_t型数为12345
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "12345\n");
}
TEST_F(TestRackLogger, TestoperatorInt64_success)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::CRIT, "Test.log", "TestFunction", 1);
    int64_t data = -123456789012345; // 设置要写入的in642_t型数为-123456789012345
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "-123456789012345\n");
}
TEST_F(TestRackLogger, TestoperatorUint64_success)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::ERROR, "Test.log", "TestFunction", 1);
    uint64_t data = 123456789012345; // 设置要写入的uin642_t型数为123456789012345
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "123456789012345\n");

    EXPECT_NO_THROW(turboLoggerEntry << data);
}
TEST_F(TestRackLogger, TestoperatorDouble_success)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    double data = 3.14; // 设置要写入的duble型数为3.14
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "3.14\n");
}
TEST_F(TestRackLogger, TestoperatorString_success)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    // 长日志输入测试，设置大于512字节的日志消息，测试开发代码因输入长日志导致的偶现问题
    std::string data =
        "This is a long string that exceeds 512 bytes. It is designed to serve as a placeholder for various testing ";
    int count = 10; // 设置循环次数为10，增加日志长度
    while (count--) {
        data += data;
    }
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.OutPutLog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, data + "\n");
}
/*
 * 用例描述：
 * 测试ResizeBuffer方法
 */
TEST_F(TestRackLogger, TestResizeBuffer_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    turboLoggerEntry.currentSize = 1; // 设置currentSize为1
    turboLoggerEntry.maxSize = 2;     // 设置maxSize为2
    size_t addSize = 1;               // 设置addSize为1
    EXPECT_NO_THROW(turboLoggerEntry.ResizeBuffer(addSize));
}
TEST_F(TestRackLogger, TestResizeBuffer_maxSize_2_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    turboLoggerEntry.heapBuffer = std::make_unique<char[]>(10); // 设置10为数组大小
    turboLoggerEntry.currentSize = 1;                           // 设置currentSize为1
    turboLoggerEntry.maxSize = 1;                               // 设置maxSize为1
    size_t addSize = 1;                                         // 设置addSize为1
    turboLoggerEntry.ResizeBuffer(addSize);
    EXPECT_EQ(turboLoggerEntry.maxSize, 2); // 设置newSize为2
}
TEST_F(TestRackLogger, TestResizeBuffer_size_should_lower_2)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    turboLoggerEntry.heapBuffer = std::make_unique<char[]>(10); // 设置10为数组大小
    turboLoggerEntry.currentSize = 1;                           // 设置currentSize为1
    turboLoggerEntry.maxSize = 1;                               // 设置maxSize为1
    size_t addSize = 1;                                         // 设置addSize为1
    turboLoggerEntry.ResizeBuffer(addSize);
    size_t testSize = turboLoggerEntry.maxSize;
    EXPECT_EQ(turboLoggerEntry.maxSize, std::max(testSize, static_cast<size_t>(2))); // 设置newSize为2
}
/*
 * 用例描述：
 * 测试EncodeString方法
 */
TEST_F(TestRackLogger, TestEncodeString_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    char data[512];    // 设置data容量为512
    size_t length = 0; // 设置length为0
    EXPECT_NO_THROW(turboLoggerEntry.EncodeString(data, length));
}

TEST_F(TestRackLogger, TestEncodeString_failed)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    char data[512];    // 设置data容量为512
    size_t length = 1024; // 设置length为1
    MOCKER(memcpy_s).stubs().will(returnValue(1));
}
/*
 * 用例描述：
 * 测试Encodedata方法
 */
TEST_F(TestRackLogger, testEncodeData_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    char *data = "test";
    EXPECT_NO_THROW(turboLoggerEntry.EncodeData(data));
}
TEST_F(TestRackLogger, testEncodeData_RackLoggerstring_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    const char *data = "test";
    TurboLoggerString rackLoggerString(data);
    EXPECT_NO_THROW(turboLoggerEntry.EncodeData(data));
}
/*
 * 用例描述：
 * 测试DecodeChar方法
 */
TEST_F(TestRackLogger, TestDecodeChar_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    // 准备输入数据
    char value = 'A';                          // 设置要写入的char型为'A'
    char buffer[512];                          // 设置buffer的容量为512
    std::copy_n(&value, sizeof(char), buffer); // 将整数拷贝到 buffer
    std::ostringstream oss;                    // 用于捕获输出流

    // 调用被测试的函数
    const char *nextBuffer = turboLoggerEntry.DecodeChar(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(char)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeUint方法
 */
TEST_F(TestRackLogger, TestDecodeUint_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    uint32_t value = 123456; // 设置要写入的uint32_t型数为123456
    std::ostringstream oss;
    char buffer[512]; // 设置buffer的容量为512
    std::copy_n(&value, sizeof(uint32_t), buffer);
    const char *nextBuffer = turboLoggerEntry.DecodeUint(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint32_t)); // 验证返回的指针

    value = 0; // 设置value为0测试边界值
    std::copy_n(&value, sizeof(uint32_t), buffer);
    nextBuffer = turboLoggerEntry.DecodeUint(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint32_t)); // 验证返回的指针

    value = 4294967295; // 设置value为4294967295测试边界值
    std::copy_n(&value, sizeof(uint32_t), buffer);
    nextBuffer = turboLoggerEntry.DecodeUint(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint32_t)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeUlong方法
 */
TEST_F(TestRackLogger, TestDecodeUlong_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    uint64_t value = 123456789012345; // 设置要写入的uint64_t型数为123456789012345
    std::ostringstream oss;
    char buffer[512]; // 设置buffer的容量为512
    std::copy_n(&value, sizeof(uint64_t), buffer);
    const char *nextBuffer = turboLoggerEntry.DecodeUlong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint64_t)); // 验证返回的指针

    value = 0; // 设置value为0测试边界值
    std::copy_n(&value, sizeof(uint64_t), buffer);
    nextBuffer = turboLoggerEntry.DecodeUlong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint64_t)); // 验证返回的指针

    value = UINT64_MAX; // 测试边界值
    std::copy_n(&value, sizeof(uint64_t), buffer);
    nextBuffer = turboLoggerEntry.DecodeUlong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(uint64_t)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeInt方法
 */
TEST_F(TestRackLogger, TestDecodeInt_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    // 准备输入数据
    int32_t value = -123456; // 设置要写入的int32_t型数为-123456
    char buffer[512];        // 设置buffer的容量为512
    std::copy_n(&value, sizeof(int32_t), buffer);
    std::ostringstream oss; // 用于捕获输出流
    // 调用被测试的函数
    const char *nextBuffer = turboLoggerEntry.DecodeInt(oss, buffer);
    // 验证输出
    EXPECT_EQ(nextBuffer, buffer + sizeof(int32_t)); // 验证返回的指针

    value = INT32_MIN; // 测试INT32_MIN边界值
    std::copy_n(&value, sizeof(int32_t), buffer);
    nextBuffer = turboLoggerEntry.DecodeInt(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int32_t)); // 验证返回的指针

    value = INT32_MAX; // 测试INT32_MAX边界值
    std::copy_n(&value, sizeof(int32_t), buffer);
    nextBuffer = turboLoggerEntry.DecodeInt(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int32_t)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeLong方法
 */
TEST_F(TestRackLogger, TestDecodeLong_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    // 准备输入数据
    int64_t value = -123456789012345; // 假设要写入的in64_t型数为-123456789012345
    char buffer[512];                 // 设置buffer的容量为512
    std::copy_n(&value, sizeof(int64_t), buffer);
    std::ostringstream oss; // 用于捕获输出流
    // 调用DecodeString
    const char *nextBuffer = turboLoggerEntry.DecodeLong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int64_t)); // 验证返回的指针

    value = INT64_MIN; // 测试INT64_MIN边界值
    std::copy_n(&value, sizeof(int64_t), buffer);
    nextBuffer = turboLoggerEntry.DecodeLong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int64_t)); // 验证返回的指针

    value = INT64_MAX; // 测试INT64_MAX边界值
    std::copy_n(&value, sizeof(int64_t), buffer);
    nextBuffer = turboLoggerEntry.DecodeLong(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(int64_t)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeDouble方法
 */
TEST_F(TestRackLogger, TestDecodeDouble_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    // 准备输入数据
    double value = 3.14; // 假设要写入的数为3.14
    char buffer[512];    // 设置buffer的容量为512
    std::copy_n(&value, sizeof(double), buffer);

    std::ostringstream oss; // 用于捕获输出流

    // 调用被测试的函数
    const char *nextBuffer = turboLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = DBL_MAX; // 测试DBL_MAX边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = turboLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = DBL_MIN; // 测试DBL_MIN边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = turboLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = DBL_TRUE_MIN; // 测试DBL_TRUE_MIN边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = turboLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = -DBL_MAX; // 测试-DBL_MAX边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = turboLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = -DBL_MIN; // 测试-DBL_MIN边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = turboLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = -0.0; // 测试-0.0边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = turboLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针

    value = +0.0; // 测试+0.0边界值
    std::copy_n(&value, sizeof(double), buffer);
    // 调用被测试的函数
    nextBuffer = turboLoggerEntry.DecodeDouble(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(double)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeString方法
 */
TEST_F(TestRackLogger, TestDecodeString_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    // 准备输入数据
    const char *str = "test";
    TurboLoggerString value(str); // 假设要写入的数
    char buffer[512];             // 设置buffer的容量为512
    std::copy_n(reinterpret_cast<const char *>(&value), sizeof(TurboLoggerString), buffer);
    std::ostringstream oss; // 用于捕获输出流

    // 调用被测试的函数
    const char *nextBuffer = turboLoggerEntry.DecodeString(oss, buffer);
    EXPECT_EQ(nextBuffer, buffer + sizeof(TurboLoggerString)); // 验证返回的指针
}
/*
 * 用例描述：
 * 测试DecodeConstChar方法
 */
TEST_F(TestRackLogger, TestDecodeConstChar_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    std::ostringstream oss; // 使用 ostringstream 作为输出流
    char input[] = "test";
    char *buffer = input; // 设置 buffer 指向 input

    // 调用 DecodeConstChar 方法
    const char *nextBuffer = turboLoggerEntry.DecodeConstChar(oss, buffer);
    // 验证输出内容
    EXPECT_EQ(nextBuffer, buffer + sizeof(input));
}
/*
 * 用例描述：
 * 测试DecodeData方法
 */
TEST_F(TestRackLogger, TestDecodeData_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    std::ostringstream oss;
    oss << std::fixed
        << std::setprecision(2); // 设置为固定格式，小数点后保留2位,解决可能偶发性导致的ostringstream精度问题
    char buffer[512]; // 设置buffer的容量为512
    std::vector<char> vec(buffer, buffer + sizeof(buffer));
    uint32_t size = 0; // 初始化size为0
    char c = 'A';
    int32_t i = -123456;              // 设置要写入的int32_t型数为-123456
    int64_t v = -123456789012345;     // 设置要写入的int64_t型数为-123456789012345
    uint32_t s = 123456;              // 设置要写入的uint32_t型数为123456
    uint64_t value = 123456789012345; // 设置要写入的uint64_t型数为123456789012345
    double d = 3.14;                  // 设置要写入的double型数为3.14
    const char *str = "test1";
    const std::string data = "test2";

    *reinterpret_cast<TurboLoggerTypeId *>(&buffer[size]) = TurboLoggerTypeId::CHAR;
    size += sizeof(TurboLoggerTypeId);
    *reinterpret_cast<char *>(&buffer[size]) = c;
    size += sizeof(char);

    *reinterpret_cast<TurboLoggerTypeId *>(&buffer[size]) = TurboLoggerTypeId::INT32;
    size += sizeof(TurboLoggerTypeId);
    *reinterpret_cast<int32_t *>(&buffer[size]) = i;
    size += sizeof(int32_t);

    *reinterpret_cast<TurboLoggerTypeId *>(&buffer[size]) = TurboLoggerTypeId::INT64;
    size += sizeof(TurboLoggerTypeId);
    *reinterpret_cast<int64_t *>(&buffer[size]) = v;
    size += sizeof(int64_t);

    *reinterpret_cast<TurboLoggerTypeId *>(&buffer[size]) = TurboLoggerTypeId::UINT32;
    size += sizeof(TurboLoggerTypeId);
    *reinterpret_cast<uint32_t *>(&buffer[size]) = s;
    size += sizeof(uint32_t);

    *reinterpret_cast<TurboLoggerTypeId *>(&buffer[size]) = TurboLoggerTypeId::UINT64;
    size += sizeof(TurboLoggerTypeId);
    *reinterpret_cast<uint64_t *>(&buffer[size]) = value;
    size += sizeof(uint64_t);

    *reinterpret_cast<TurboLoggerTypeId *>(&buffer[size]) = TurboLoggerTypeId::DOUBLE;
    size += sizeof(TurboLoggerTypeId);
    *reinterpret_cast<double *>(&buffer[size]) = d;
    size += sizeof(double);
    turboLoggerEntry.DecodeData(oss, &buffer[0], (&buffer[0]) + size);
    EXPECT_EQ(oss.str(), "A-123456-1234567890123451234561234567890123453.14");
    // 和已设置的数"A-123456-1234567890123451234561234567890123453.14"进行比较
}
/*
 * 用例描述：
 * 测试TurboIsLog方法
 */
TEST_F(TestRackLogger, TestTurboIsLog_success)
{
    TurboLogLevel level = TurboLogLevel::INFO;
    LoggerOptions options{RackLoggerManager::StringToLogLevel("INFO"), 30, 20,
                          1024}; // 设置30为filesize，20为fileNums，1024为maxItem
    RackLoggerManager::gInstance = new (std::nothrow) RackLoggerManager();
    RackLoggerManager::gInstance->SetLogLevel(options.minLogLevel);
    EXPECT_TRUE(TurboIsLog(level));

    RackLoggerManager::gInstance = nullptr;
    EXPECT_FALSE(TurboIsLog(level));
}
/*
 * 用例描述：
 * 测试RackLog方法
 */
TEST_F(TestRackLogger, TestRackLog_success)
{
    TurboLoggerEntry turboLoggerEntry(nullptr, TurboLogLevel::INFO, nullptr, nullptr, 0);
    EXPECT_NO_THROW(TurboLog(turboLoggerEntry));
}

/*
 * 用例描述：
 * 测试拷贝构造函数
 */
TEST_F(TestRackLogger, TestCopyConstructor_success)
{
    TurboLoggerEntry turboLoggerEntry1(nullptr, TurboLogLevel::INFO, "test.cpp", "testFunc", 111); // 设置行号为111
    TurboLoggerEntry turboLoggerEntry2(turboLoggerEntry1);
    turboLoggerEntry2.GetEntryTimeStamp();
    EXPECT_EQ(turboLoggerEntry2.GetFile(), "test.cpp");
    EXPECT_EQ(turboLoggerEntry2.GetLine(), 111); // 查看turboLoggerEntry2行号是否为111

    TurboLoggerEntry turboLoggerEntry3(nullptr, TurboLogLevel::INFO, "test.cpp", "testFunc", 111); // 设置行号为111
    std::string str =
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    turboLoggerEntry3 << str << str << str << str << str << str;
    TurboLoggerEntry turboLoggerEntry4(turboLoggerEntry3);
    EXPECT_EQ(turboLoggerEntry4.GetFile(), "test.cpp");
    EXPECT_EQ(turboLoggerEntry4.GetLine(), 111); // 查看turboLoggerEntry4行号是否为111
}

/*
 * 用例描述：
 * 测试FormatSyslog
 * 测试步骤：
 * 1. 通过FormatSyslog获取流
 * 预期结果：
 * 1. 得到的字符串与预期一致
 */
TEST_F(TestRackLogger, TestFormatSyslog_success)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    int64_t data = -123456789012345; // 设置要写入的in642_t型数为-123456789012345
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.FormatSyslog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "-123456789012345\n");
}

TEST_F(TestRackLogger, TestGetSyslogAsString)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    char data = 'a'; // 设置要写入的char型数据为a
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.FormatSyslog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "a\n");
}

TEST_F(TestRackLogger, TestGetSyslogAsInt32_t)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    int32_t data = -12345;
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.FormatSyslog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "-12345\n");
}

TEST_F(TestRackLogger, TestGetSyslogAsUint32_t)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    uint32_t data = 12345;
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.FormatSyslog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "12345\n");
}

TEST_F(TestRackLogger, TestGetSyslogAsInt64_t)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    int64_t data = -123456789012345;
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.FormatSyslog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "-123456789012345\n");
}

TEST_F(TestRackLogger, TestGetSyslogAsUint64_t)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    uint64_t data = 123456789012345;
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.FormatSyslog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "123456789012345\n");
}

TEST_F(TestRackLogger, TestGetSyslogAsDouble)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    double data = 3.14; // 设置要写入的duble型数为3.14
    turboLoggerEntry << data;
    std::ostringstream os;
    turboLoggerEntry.FormatSyslog(os);
    std::string str = os.str();
    str = DeleteOther(str);
    EXPECT_EQ(str, "3.14\n");
}

TEST_F(TestRackLogger, TestRackDefaultLoggerWriter)
{
    TurboLoggerEntry turboLoggerEntry("test_log", TurboLogLevel::INFO, "Test.log", "TestFunction", 1);
    RackDefaultLoggerWriter writer;
    auto ret = writer.Write(turboLoggerEntry);
    EXPECT_EQ(ret, true);
}
} // namespace turbo::log