/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "turbo_logger.h"
namespace turbo::log {

void TurboLogOutput(const char *moduleName, TurboLogLevel level, const char *msg);

bool TurboIsLog(TurboLogLevel level)
{
    return true;
}

bool TurboLog::operator==(TurboLoggerEntry &loggerEntry)
{
    return false;
}

void TurboLoggerEntry::DecodeData(std::ostream &os, char *start, const char *end)
{
}

char *TurboLoggerEntry::DecodeConstChar(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *TurboLoggerEntry::DecodeString(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *TurboLoggerEntry::DecodeDouble(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *TurboLoggerEntry::DecodeLong(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *TurboLoggerEntry::DecodeInt(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *TurboLoggerEntry::DecodeUlong(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *TurboLoggerEntry::DecodeUint(std::ostream &os, char *buffer)
{
    return nullptr;
}

char *TurboLoggerEntry::DecodeChar(std::ostream &os, char *buffer)
{
    return nullptr;
}

void TurboLoggerEntry::EncodeData(TurboLoggerString data)
{
}

void TurboLoggerEntry::EncodeData(const char *data)
{
}

void TurboLoggerEntry::EncodeString(const char *data, size_t length)
{
}

char *TurboLoggerEntry::GetBuffer()
{
    return nullptr;
}

void TurboLoggerEntry::ResizeBuffer(size_t addSize)
{
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(const std::string &data)
{
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(uint64_t data)
{
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(double data)
{
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(int64_t data)
{
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(uint32_t data)
{
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(int32_t data)
{
    return *this;
}

TurboLoggerEntry &TurboLoggerEntry::operator<<(char data)
{
    return *this;
}

char *TurboLoggerEntry::GetMessage(size_t &length)
{
    return nullptr;
}

uint32_t TurboLoggerEntry::GetLine()
{
    return 0;
}

const char *TurboLoggerEntry::GetFile()
{
    return nullptr;
}

const char *TurboLoggerEntry::GetModuleName()
{
    return nullptr;
}

void TurboLoggerEntry::OutPutLog(std::ostream &os)
{
}

TurboLoggerEntry::TurboLoggerEntry(const char *gModuleName, TurboLogLevel level, const char *file, const char *func,
                                   uint32_t line)
{
}
}  // namespace rack::log