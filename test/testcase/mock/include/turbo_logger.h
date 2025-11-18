/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
#ifndef TURBO_LOGGER_H
#define TURBO_LOGGER_H
#include <sys/types.h>  // for pid_t
#include <cstdint>      // for uint32_t, uint64_t, int32_t, int64_t, uint8_t
#include <cstring>      // for size_t, strrchr
#include <iomanip>
#include <iosfwd>       // for ostream
#include <memory>       // for unique_ptr
#include <string>       // for string
#include <thread>       // for thread
#include <type_traits>  // for enable_if, is_same
#include <iostream>

namespace turbo::log {
/**
 * @brief 定义Module名，为调试日志写入的文件名
 * @param mn [in] Module名，如
 */
#ifndef RACK_DEFINE_THIS_MODULE
#define RACK_DEFINE_THIS_MODULE(mn, mid)                                                           \
    static const char *gModuleName = (mn);                                                         \
    _Pragma("GCC diagnostic push")                                                                 \
        _Pragma("GCC diagnostic ignored \"-Wunused-variable\"") static uint32_t gModuleId = (mid); \
    _Pragma("GCC diagnostic pop")
#endif

/**
 * @brief 文件名的宏
 */
#ifndef FILENAME
#define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

/**
 * @brief 创建单条日志，写到终端或写入文件
 * @param gModuleName [in] Module名，为调试日志写入的文件名
 * @param LEVEL [in] 单条日志级别
 */
#define TURBO_LOG_INTERNAL(gModuleName, LEVEL) \
    turbo::log::TurboLog() == turbo::log::TurboLoggerEntry(gModuleName, LEVEL, FILENAME, __func__, __LINE__)


#define UBTURBO_LOG_CRIT(gModuleName, gModuleId) if (true) {} else std::cout
#define UBTURBO_LOG_ERROR(gModuleName, gModuleId) if (true) {} else std::cout
#define UBTURBO_LOG_WARN(gModuleName, gModuleId) if (true) {} else std::cout
#define UBTURBO_LOG_INFO(gModuleName, gModuleId) if (true) {} else std::cout
#define UBTURBO_LOG_DEBUG(gModuleName, gModuleId) if (true) {} else std::cout

enum class TurboLogLevel : uint32_t { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, CRIT = 4 };

/**
 * @brief 三方库注册日志函数接口
 * @param moduleName [in] Module名，为调试日志写入的文件名
 * @param level [in] 日志级别
 * @param msg [in] 日志内容
 */
void TurboLogOutput(const char *moduleName, TurboLogLevel level, const char *msg);

/**
 * @brief 格式化错误码
 * @param retCode [in] 错误码
 * @return 格式化后的错误码
 */
std::string FormatRetCode(uint32_t retCode);

enum class TurboLoggerTypeId : uint8_t { CHAR = 0, UINT32, UINT64, INT32, INT64, DOUBLE, STRING, CHARPTR };

struct TurboLoggerString {
    explicit TurboLoggerString(const char *buffer) : data(buffer) {}
    const char *data;
};

class TurboLoggerEntry {
public:
    TurboLoggerEntry(const char *gModuleName, TurboLogLevel level, const char *file, const char *func, uint32_t line);
    ~TurboLoggerEntry() = default;

    TurboLoggerEntry();

    TurboLoggerEntry(const TurboLoggerEntry &other);

    TurboLoggerEntry &operator=(const TurboLoggerEntry &);

    void OutPutLog(std::ostream &os);
    void FormatSyslog(std::ostream &os);

    TurboLogLevel GetLogLevel();
    const char *GetSyslogAsString();
    const char *GetModuleName();
    const char *GetFile();
    uint32_t GetLine();
    char *GetMessage(size_t &length);
    uint64_t GetEntryTimeStamp();

    TurboLoggerEntry &operator<<(char data);
    TurboLoggerEntry &operator<<(int32_t data);
    TurboLoggerEntry &operator<<(uint32_t data);
    TurboLoggerEntry &operator<<(int64_t data);
    TurboLoggerEntry &operator<<(uint64_t data);
    TurboLoggerEntry &operator<<(double data);
    TurboLoggerEntry &operator<<(const std::string &data);

    template <size_t count>
    TurboLoggerEntry &operator<<(const char (&data)[count])
    {
        EncodeData(TurboLoggerString(data));
        return *this;
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, const char *>::value, TurboLoggerEntry &>::type operator<<(T const &data)
    {
        EncodeData(data);
        return *this;
    }

private:
    void ResizeBuffer(size_t addSize);

    char *GetBuffer();

    void EncodeString(const char *data, size_t length);

    template <typename T>
    void EncodeData(T data)
    {
        *reinterpret_cast<T *>(GetBuffer()) = data;
        currentSize += sizeof(T);
    }

    template <typename T>
    void EncodeData(TurboLoggerTypeId id, T data)
    {
        ResizeBuffer(sizeof(TurboLoggerTypeId) + sizeof(T));
        EncodeData<TurboLoggerTypeId>(id);
        EncodeData<T>(data);
    }

    void EncodeData(const char *data);
    void EncodeData(TurboLoggerString data);
    char *DecodeChar(std::ostream &os, char *buffer);
    char *DecodeUint(std::ostream &os, char *buffer);
    char *DecodeUlong(std::ostream &os, char *buffer);
    char *DecodeInt(std::ostream &os, char *buffer);
    char *DecodeLong(std::ostream &os, char *buffer);
    char *DecodeDouble(std::ostream &os, char *buffer);
    char *DecodeString(std::ostream &os, char *buffer);
    char *DecodeConstChar(std::ostream &os, char *buffer);
    void DecodeData(std::ostream &os, char *start, const char *end);

    std::string moduleName;
    uint64_t timeStamp;
    pid_t pid;
    std::thread::id tid;
    TurboLogLevel level;
    const char *file;
    const char *func;
    uint32_t line;

    size_t maxSize;
    char logEntryBuffer[512] = {0};  // 缓冲区大小为512
    std::unique_ptr<char[]> heapBuffer;
    size_t currentSize;
};

bool TurboIsLog(TurboLogLevel level);

struct TurboLog {
    bool operator==(TurboLoggerEntry &loggerEntry);
};
}  // namespace turbo::log
#endif  // TURBO_LOGGER_H