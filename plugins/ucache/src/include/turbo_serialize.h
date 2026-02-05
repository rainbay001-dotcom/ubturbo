/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef TURBO_SERIALIZE_UTIL_H
#define TURBO_SERIALIZE_UTIL_H
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "securec.h"

namespace turbo::serialize {
constexpr size_t MAX_SAFE_SIZE = 1 * 1024 * 1024;

struct AnyType {
    template <typename T>
    operator T() const;
};
template <typename T, typename = void, typename... Args>
struct CountMember {
    constexpr static size_t value = sizeof...(Args) - 1;
};
template <typename T, typename... Args>
struct CountMember<T, std::void_t<decltype(T{{Args{}}...})>, Args...> {
    constexpr static size_t value = CountMember<T, void, Args..., AnyType>::value;
};

template <class T, size_t N>
struct MemberTupleHelper {
    template <class U>
    inline constexpr static auto GetTuple(U &u)
    {
        static_assert(sizeof(U) < 0);
        return std::tie();
    }
};

#define UCACHE_EXPAND(n) UCACHE_EXPAND##n
#define UCACHE_EXPAND1 a1
#define UCACHE_EXPAND2 UCACHE_EXPAND1, a2
#define UCACHE_EXPAND3 UCACHE_EXPAND2, a3
#define UCACHE_EXPAND4 UCACHE_EXPAND3, a4
#define UCACHE_EXPAND5 UCACHE_EXPAND4, a5
#define UCACHE_EXPAND6 UCACHE_EXPAND5, a6
#define UCACHE_EXPAND7 UCACHE_EXPAND6, a7
#define UCACHE_EXPAND8 UCACHE_EXPAND7, a8
#define UCACHE_EXPAND9 UCACHE_EXPAND8, a9
#define UCACHE_EXPAND10 UCACHE_EXPAND9, a10
#define UCACHE_EXPAND11 UCACHE_EXPAND10, a11
#define UCACHE_EXPAND12 UCACHE_EXPAND11, a12
#define UCACHE_EXPAND13 UCACHE_EXPAND12, a13
#define UCACHE_EXPAND14 UCACHE_EXPAND13, a14
#define UCACHE_EXPAND15 UCACHE_EXPAND14, a15
#define UCACHE_EXPAND16 UCACHE_EXPAND15, a16

#define UCACHE_MEMBER_TUPLE_HELPER(n)               \
    template <class T>                              \
    struct MemberTupleHelper<T, n> {                \
        template <class U>                          \
        inline constexpr static auto GetTuple(U &u) \
        {                                           \
            auto &&[UCACHE_EXPAND(n)] = u;          \
            return std::tie(UCACHE_EXPAND(n));      \
        }                                           \
    }

UCACHE_MEMBER_TUPLE_HELPER(1);
UCACHE_MEMBER_TUPLE_HELPER(2);
UCACHE_MEMBER_TUPLE_HELPER(3);
UCACHE_MEMBER_TUPLE_HELPER(4);
UCACHE_MEMBER_TUPLE_HELPER(5);
UCACHE_MEMBER_TUPLE_HELPER(6);
UCACHE_MEMBER_TUPLE_HELPER(7);
UCACHE_MEMBER_TUPLE_HELPER(8);
UCACHE_MEMBER_TUPLE_HELPER(9);
UCACHE_MEMBER_TUPLE_HELPER(10);
UCACHE_MEMBER_TUPLE_HELPER(11);
UCACHE_MEMBER_TUPLE_HELPER(12);
UCACHE_MEMBER_TUPLE_HELPER(13);
UCACHE_MEMBER_TUPLE_HELPER(14);
UCACHE_MEMBER_TUPLE_HELPER(15);
UCACHE_MEMBER_TUPLE_HELPER(16);

// 调用者必须负责调用 delete[] GetBufferPointer() 返回的指针
class UCacheOutStream {
public:
    template <typename T>
    UCacheOutStream &operator<<(const T &data);
    template <typename T>
    UCacheOutStream &operator<<(const std::vector<T> &data);
    template <typename T>
    UCacheOutStream &operator<<(const std::set<T> &data);
    template <typename T>
    UCacheOutStream &operator<<(const std::unordered_set<T> &data);
    template <typename K, typename V>
    UCacheOutStream &operator<<(const std::map<K, V> &data);
    template <typename K, typename V>
    UCacheOutStream &operator<<(const std::unordered_map<K, V> &data);
    void Write(const char *data, size_t len)
    {
        outStream.append(data, len);
    }

    std::string Str()
    {
        return outStream;
    }

    uint8_t *GetBufferPointer()
    {
        size_t dataLen = outStream.length();
        if (dataLen > MAX_SAFE_SIZE || dataLen == 0) {
            mFlag = false;
            return nullptr;
        }
        uint8_t *ptr = new (std::nothrow) uint8_t[dataLen];
        if (!ptr) {
            mFlag = false;
            return nullptr;
        }
        if (memcpy_s(ptr, dataLen, outStream.data(), dataLen) != 0) {
            delete[] ptr;
            mFlag = false;
            return nullptr;
        }
        return ptr;
    }

    size_t GetSize()
    {
        return outStream.length();
    }

public:
    std::string outStream;
    bool mFlag{true};
};

class UCacheInStream {
public:
    UCacheInStream(const std::string &s) : inStream(s) {}
    UCacheInStream(const uint8_t *s, size_t len)
    {
        inStream = std::string(reinterpret_cast<const char *>(s), len);
    }
    template <typename T>
    UCacheInStream &operator>>(T &data);
    template <typename T>
    UCacheInStream &operator>>(std::vector<T> &data);
    template <typename T>
    UCacheInStream &operator>>(std::set<T> &data);
    template <typename T>
    UCacheInStream &operator>>(std::unordered_set<T> &data);
    template <typename K, typename V>
    UCacheInStream &operator>>(std::map<K, V> &data);
    template <typename K, typename V>
    UCacheInStream &operator>>(std::unordered_map<K, V> &data);
    void Read(char *dest, size_t dest_size, size_t count)
    {
        if (count == 0) {
            return;
        }
        if (count > dest_size) {
            mFlag = false;
            return;
        }
        if (!mFlag || offset > inStream.length() || count > (inStream.length() - offset)) {
            mFlag = false;
            return;
        }
        if (memcpy_s(dest, dest_size, inStream.data() + offset, count) != 0) {
            mFlag = false;
        }
        offset += count;
    }

    std::string Str()
    {
        return inStream;
    }

    bool Check()
    {
        return mFlag && offset == inStream.length();
    }

private:
    size_t ReadAndCheckLength()
    {
        if (!mFlag) {
            return 0;
        }
        size_t len = 0;
        this->Read(reinterpret_cast<char *>(&len), sizeof(len), sizeof(len));
        if (!mFlag || len > MAX_SAFE_SIZE || offset > inStream.length() || len > inStream.length() - offset) {
            mFlag = false;
            return 0;
        }
        return len;
    }

    std::string inStream;
    size_t offset{};
    bool mFlag{true};
};

template <typename T>
UCacheOutStream &UCacheOutStream::operator<<(const T &data)
{
    if constexpr (std::is_trivially_copyable_v<T>) {
        this->Write(reinterpret_cast<const char *>(&data), sizeof(T));
    } else {
        constexpr auto n = CountMember<T>::value;
        auto members = MemberTupleHelper<T, n>::GetTuple(data);
        [&]<std::size_t... index>(std::index_sequence<index...>)
        {
            ((*this << std::get<index>(members)), ...);
        }
        (std::make_index_sequence<n>{});
    }
    return *this;
}

template <>
inline UCacheOutStream &UCacheOutStream::operator<< <std::string>(const std::string &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    this->Write(data.c_str(), len);
    return *this;
}

template <typename K, typename V>
UCacheOutStream &UCacheOutStream::operator<<(const std::map<K, V> &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    // 遍历 map，逐个序列化 key 和 value
    for (const auto &pair : data) {
        *this << pair.first;  // 序列化 key
        *this << pair.second; // 序列化 value
    }
    return *this;
}

template <typename K, typename V>
UCacheOutStream &UCacheOutStream::operator<<(const std::unordered_map<K, V> &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    // 遍历 map，逐个序列化 key 和 value
    for (const auto &pair : data) {
        *this << pair.first;  // 序列化 key
        *this << pair.second; // 序列化 value
    }
    return *this;
}

template <typename T>
UCacheOutStream &UCacheOutStream::operator<<(const std::vector<T> &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    for (auto &i : data) {
        *this << i;
    }
    return *this;
}

template <typename T>
UCacheOutStream &UCacheOutStream::operator<<(const std::set<T> &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    for (auto &i : data) {
        *this << i;
    }
    return *this;
}

template <typename T>
UCacheOutStream &UCacheOutStream::operator<<(const std::unordered_set<T> &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    for (auto &i : data) {
        *this << i;
    }
    return *this;
}

template <typename T>
UCacheInStream &UCacheInStream::operator>>(T &data)
{
    if (!mFlag) {
        return *this;
    }
    if constexpr (std::is_trivially_copyable_v<T>) {
        this->Read(reinterpret_cast<char *>(&data), sizeof(T), sizeof(T));
    } else {
        constexpr auto n = CountMember<T>::value;
        auto members = MemberTupleHelper<T, n>::GetTuple(data);
        [&]<std::size_t... index>(std::index_sequence<index...>)
        {
            ((*this >> std::get<index>(members)), ...);
        }
        (std::make_index_sequence<n>{});
    }
    return *this;
}

template <>
inline UCacheInStream &UCacheInStream::operator>><std::string>(std::string &data)
{
    size_t len = this->ReadAndCheckLength();
    if (!mFlag) {
        return *this;
    }
    try {
        data.resize(len);
    } catch (const std::bad_alloc &) {
        mFlag = false;
        return *this;
    }
    if (len > 0) {
        this->Read(&data[0], len, len);
    }
    return *this;
}

template <typename K, typename V>
UCacheInStream &UCacheInStream::operator>>(std::map<K, V> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = this->ReadAndCheckLength();
    if (!mFlag) {
        return *this;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!mFlag) {
            return *this;
        }
        K key;
        V value;
        *this >> key;
        *this >> value;
        data.emplace(key, value);
    }
    return *this;
}

template <typename K, typename V>
UCacheInStream &UCacheInStream::operator>>(std::unordered_map<K, V> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = this->ReadAndCheckLength();
    if (!mFlag) {
        return *this;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!mFlag) {
            return *this;
        }
        K key;
        V value;
        *this >> key;
        *this >> value;
        data.emplace(key, value);
    }
    return *this;
}

template <typename T>
UCacheInStream &UCacheInStream::operator>>(std::vector<T> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = this->ReadAndCheckLength();
    if (!mFlag) {
        return *this;
    }
    data.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        if (!mFlag) {
            return *this;
        }
        T temp;
        *this >> temp;
        data.emplace_back(std::move(temp));
    }
    return *this;
}

template <typename T>
UCacheInStream &UCacheInStream::operator>>(std::set<T> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = this->ReadAndCheckLength();
    if (!mFlag) {
        return *this;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!mFlag) {
            return *this;
        }
        T temp;
        *this >> temp;
        data.emplace(temp);
    }
    return *this;
}

template <typename T>
UCacheInStream &UCacheInStream::operator>>(std::unordered_set<T> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = this->ReadAndCheckLength();
    if (!mFlag) {
        return *this;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!mFlag) {
            return *this;
        }
        T temp;
        *this >> temp;
        data.emplace(temp);
    }
    return *this;
}
} // namespace turbo::serialize

#endif