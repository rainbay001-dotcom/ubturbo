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

#ifndef TURBO_SERIALIZE_UTIL_H
#define TURBO_SERIALIZE_UTIL_H
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include "securec.h"

namespace turbo::serialize {

struct AnyType {
    template <typename T>
    operator T() const;
};
template <typename T, typename = void, typename ...Args>
struct CountMember {
    constexpr static size_t value = sizeof...(Args) - 1;
};
template <typename T, typename ...Args>
struct CountMember<T, std::void_t<decltype(T{{Args{}}...})>, Args...>
{
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

#define RMRS_EXPAND(n) RMRS_EXPAND##n
#define RMRS_EXPAND1 a1
#define RMRS_EXPAND2 RMRS_EXPAND1, a2
#define RMRS_EXPAND3 RMRS_EXPAND2, a3
#define RMRS_EXPAND4 RMRS_EXPAND3, a4
#define RMRS_EXPAND5 RMRS_EXPAND4, a5
#define RMRS_EXPAND6 RMRS_EXPAND5, a6
#define RMRS_EXPAND7 RMRS_EXPAND6, a7
#define RMRS_EXPAND8 RMRS_EXPAND7, a8
#define RMRS_EXPAND9 RMRS_EXPAND8, a9
#define RMRS_EXPAND10 RMRS_EXPAND9, a10
#define RMRS_EXPAND11 RMRS_EXPAND10, a11
#define RMRS_EXPAND12 RMRS_EXPAND11, a12
#define RMRS_EXPAND13 RMRS_EXPAND12, a13
#define RMRS_EXPAND14 RMRS_EXPAND13, a14
#define RMRS_EXPAND15 RMRS_EXPAND14, a15
#define RMRS_EXPAND16 RMRS_EXPAND15, a16

#define RMRS_MEMBER_TUPLE_HELPER(n) \
template <class T> \
struct MemberTupleHelper<T, n> { \
    template <class U> \
    inline constexpr static auto GetTuple(U &u) \
    { \
        auto &&[RMRS_EXPAND(n)] = u; \
        return std::tie(RMRS_EXPAND(n)); \
    } \
}

RMRS_MEMBER_TUPLE_HELPER(1);
RMRS_MEMBER_TUPLE_HELPER(2);
RMRS_MEMBER_TUPLE_HELPER(3);
RMRS_MEMBER_TUPLE_HELPER(4);
RMRS_MEMBER_TUPLE_HELPER(5);
RMRS_MEMBER_TUPLE_HELPER(6);
RMRS_MEMBER_TUPLE_HELPER(7);
RMRS_MEMBER_TUPLE_HELPER(8);
RMRS_MEMBER_TUPLE_HELPER(9);
RMRS_MEMBER_TUPLE_HELPER(10);
RMRS_MEMBER_TUPLE_HELPER(11);
RMRS_MEMBER_TUPLE_HELPER(12);
RMRS_MEMBER_TUPLE_HELPER(13);
RMRS_MEMBER_TUPLE_HELPER(14);
RMRS_MEMBER_TUPLE_HELPER(15);
RMRS_MEMBER_TUPLE_HELPER(16);

class RmrsOutStream {
public:
    template <typename T> RmrsOutStream &operator << (const T &data);
    template <typename T> RmrsOutStream &operator << (const std::vector<T> &data);
    template <typename T> RmrsOutStream &operator << (const std::set<T> &data);
    template <typename T> RmrsOutStream &operator << (const std::unordered_set<T> &data);
    template <typename K, typename V> RmrsOutStream &operator << (const std::map<K, V> &data);
    template <typename K, typename V> RmrsOutStream &operator << (const std::unordered_map<K, V> &data);
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
        if (ptr != nullptr) {
            return ptr;
        }
        ptr = new uint8_t[outStream.length()];
        if (memcpy_s(ptr, outStream.length(), outStream.c_str(), outStream.length()) != 0) {
            mFlag = false;
            return ptr;
        }
        return ptr;
    }

    size_t GetSize()
    {
        return outStream.length();
    }

public:
    std::string outStream;
    uint8_t *ptr{};
    bool mFlag{true};
};

class RmrsInStream {
public:
    RmrsInStream(const std::string &s) : inStream(s) {}
    RmrsInStream(const uint8_t *s, size_t len)
    {
        inStream = std::string(reinterpret_cast<const char *>(s), len);
    }
    template <typename T> RmrsInStream &operator >> (T &data);
    template <typename T> RmrsInStream &operator >> (std::vector<T> &data);
    template <typename T> RmrsInStream &operator >> (std::set<T> &data);
    template <typename T> RmrsInStream &operator >> (std::unordered_set<T> &data);
    template <typename K, typename V> RmrsInStream &operator >> (std::map<K, V> &data);
    template <typename K, typename V> RmrsInStream &operator >> (std::unordered_map<K, V> &data);
    void Read(char *data, size_t len)
    {
        if (!mFlag || inStream.length() - offset < len) {
            mFlag = false;
            return ;
        }
        if (memcpy_s(data, inStream.length() - offset, &inStream[offset], len) != 0) {
            mFlag = false;
        }
        offset += len;
        return ;
    }

    std::string Str()
    {
        return inStream;
    }

    bool Check()
    {
        return mFlag && offset == inStream.length() ;
    }

private:
    std::string inStream;
    size_t offset{};
    bool mFlag{true};
};

#pragma GCC diagnostic push


template <typename T> RmrsOutStream &RmrsOutStream::operator << (const T &data)
{
    if constexpr (std::is_trivially_copyable_v<T>) {
        this->Write(reinterpret_cast<const char *>(&data), sizeof(T));
    } else {
        constexpr auto n = CountMember<T>::value;
        auto members = MemberTupleHelper<T, n>::GetTuple(data);
        [&]<std::size_t... index>(std::index_sequence<index...>) {
            ((*this << std::get<index>(members)), ...);
        } (std::make_index_sequence<n>{});
    }
    return *this;
}

template <>
inline RmrsOutStream &RmrsOutStream::operator << <std::string> (const std::string &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    this->Write(data.c_str(), len);
    return *this;
}

template <typename K, typename V> RmrsOutStream &RmrsOutStream::operator << (const std::map<K, V> &data)
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

template <typename K, typename V> RmrsOutStream &RmrsOutStream::operator << (const std::unordered_map<K, V> &data)
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

template <typename T> RmrsOutStream &RmrsOutStream::operator << (const std::vector<T> &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    for (auto &i : data) {
        *this << i;
    }
    return *this;
}

template <typename T> RmrsOutStream &RmrsOutStream::operator << (const std::set<T> &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    for (auto &i : data) {
        *this << i;
    }
    return *this;
}

template <typename T> RmrsOutStream &RmrsOutStream::operator << (const std::unordered_set<T> &data)
{
    size_t len = data.size();
    this->Write(reinterpret_cast<const char *>(&len), sizeof(len));
    for (auto &i : data) {
        *this << i;
    }
    return *this;
}

template <typename T> RmrsInStream &RmrsInStream::operator >> (T &data)
{
    if (!mFlag) {
        return *this;
    }
    if constexpr (std::is_trivially_copyable_v<T>) {
        this->Read(reinterpret_cast<char *>(&data), sizeof(T));
    } else {
        constexpr auto n = CountMember<T>::value;
        auto members = MemberTupleHelper<T, n>::GetTuple(data);
        [&]<std::size_t... index>(std::index_sequence<index...>) {
            ((*this >> std::get<index>(members)), ...);
        } (std::make_index_sequence<n>{});
    }
    return *this;
}

template <>
inline RmrsInStream &RmrsInStream::operator >> <std::string> (std::string &data)
{
    size_t len = 0;
    this->Read(reinterpret_cast<char *>(&len), sizeof(len));
    if (!mFlag || inStream.length() - offset < len) {
        mFlag = false;
        return *this;
    }
    data.resize(len);
    this->Read(&data[0], len);
    return *this;
}

template <typename K, typename V> RmrsInStream &RmrsInStream::operator >> (std::map<K, V> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = 0;
    this->Read(reinterpret_cast<char *>(&len), sizeof(len));
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

template <typename K, typename V> RmrsInStream &RmrsInStream::operator >> (std::unordered_map<K, V> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = 0;
    this->Read(reinterpret_cast<char *>(&len), sizeof(len));
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

template <typename T> RmrsInStream &RmrsInStream::operator >> (std::vector<T> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = 0;
    this->Read(reinterpret_cast<char *>(&len), sizeof(len));
    for (size_t i = 0; i < len; ++i) {
        if (!mFlag) {
            return *this;
        }
        T temp;
        *this >> temp;
        data.emplace_back(temp);
    }
    return *this;
}

template <typename T> RmrsInStream &RmrsInStream::operator >> (std::set<T> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = 0;
    this->Read(reinterpret_cast<char *>(&len), sizeof(len));
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

template <typename T> RmrsInStream &RmrsInStream::operator >> (std::unordered_set<T> &data)
{
    if (!mFlag) {
        return *this;
    }
    data.clear();
    size_t len = 0;
    this->Read(reinterpret_cast<char *>(&len), sizeof(len));
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

#pragma GCC diagnostic pop
} // namespace turbo::serialize

#endif