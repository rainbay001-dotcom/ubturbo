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
#include "rmrs_json_util.h"

#include <map>
#include <string>
#include <vector>

#include "turbo_logger.h"

namespace rmrs {

/* 将document对象打印成string字符串 */
JSON_STR JsonUtil::PrintJsonString(const rapidjson::Document& doc)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    JSON_STR jsonStr = buffer.GetString();
    return jsonStr;
}

/* 将Value对象打印成string字符串 */
JSON_STR JsonUtil::PrintJsonString(const rapidjson::Value& value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    JSON_STR jsonStr = buffer.GetString();
    return jsonStr;
}

/* 返回pstJson中key对应的子节点的json字符串 */
bool JsonUtil::RackMemGetJsonItemStr(const rapidjson::Document &doc, const char *key, JSON_STR &jsonStr)
{
    if (key == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Input key is null.";
        return false;
    }
    if (!doc.HasMember(key)) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Json item get error, dont have member " << key << ".";
        return false;
    }

    const rapidjson::Value& child = doc[key];
    jsonStr = PrintJsonString(child);
    return true;
}

/* 将map转为json字符串，其中map的value可以是json字符串,也可以是不加引号的string字符串 */
bool JsonUtil::RackMemConvertMap2JsonStr(const JSON_MAP &strMap, JSON_STR &jsonStr)
{
    rapidjson::Document doc;
    doc.SetObject();
    for (const auto &item : strMap) {
        const JSON_STR &key = item.first;
        const JSON_STR &value = item.second;
        // 此处保证临时对象与doc使用相同的allocator
        rapidjson::Document tmpDoc(&doc.GetAllocator());
        tmpDoc.Parse(value.c_str());
        if (!tmpDoc.HasParseError()) {
            // value是合法的json字符串,为doc添加一个子对象
            doc.AddMember(rapidjson::StringRef(key.c_str()), tmpDoc, doc.GetAllocator());
        } else {
            // value是不加引号的string,为doc添加一个字符串成员
            doc.AddMember(rapidjson::StringRef(key.c_str()), rapidjson::Value(value.c_str(), doc.GetAllocator()),
                          doc.GetAllocator());
        }
    }
    jsonStr = PrintJsonString(doc);
    return true;
}

/* 将object类型的json字符串转为map，其中map的key须提前填充，value也可以是json字符串 */
bool JsonUtil::RackMemConvertJsonStr2Map(const JSON_STR &jsonStr, JSON_MAP &strMap)
{
    Document pJson;
    pJson.Parse(jsonStr.c_str());
    if (pJson.HasParseError()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Parse json error.";
        return false;
    }
    if (!pJson.IsObject()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Json string is not object.";
        return false;
    }
    for (auto &[fst, snd] : strMap) {
        if (!pJson.HasMember(fst.c_str())) {
            return false;
        }
        if (pJson[fst.c_str()].IsString()) {
            snd = pJson[fst.c_str()].GetString();
        } else {
            const auto &pcVal = pJson[fst.c_str()];
            StringBuffer buffer;
            PrettyWriter<StringBuffer> writer(buffer);
            pcVal.Accept(writer);
            snd = buffer.GetString();
        }
    }
    return true;
}

/* 将vector转为json字符串，其中vector的value也可以是json字符串 */
bool JsonUtil::RackMemConvertVector2JsonStr(const JSON_VEC &strVec, JSON_STR &jsonStr)
{
    rapidjson::Document doc;
    doc.SetArray();
    for (const auto &item : strVec) {
        // 此处保证临时对象与doc使用相同的allocator
        rapidjson::Document tmpDoc(&doc.GetAllocator());
        tmpDoc.Parse(item.c_str());
        if (!tmpDoc.HasParseError()) {
            // value是合法的json字符串,为doc添加一个子对象
            doc.PushBack(tmpDoc, doc.GetAllocator());
        } else {
            // value是string,为doc添加一个字符串成员
            doc.PushBack(rapidjson::Value(item.c_str(), doc.GetAllocator()), doc.GetAllocator());
        }
    }
    jsonStr = PrintJsonString(doc);
    return true;
}

/* 将array类型的json字符串转为vec，value也可以是json字符串 */
bool JsonUtil::RackMemConvertJsonStr2Vec(const JSON_STR &jsonStr, JSON_VEC &strVec)
{
    rapidjson::Document pJson;
    auto &allocator = pJson.GetAllocator();
    pJson.Parse(jsonStr.c_str());
    if (pJson.HasParseError() || !pJson.IsArray()) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "Json Parse error.";
        return false;
    }
    const rapidjson::Value &array = pJson.GetArray();
    uint32_t length = array.Size();
    // 遍历Json数组将所有字符串加入Vector
    for (size_t i = 0; i < length; ++i) {
        if (array[i].IsString()) {
            strVec.emplace_back(array[i].GetString());
        } else {
            std::string element = PrintJsonString(array[i]);
            UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "Json element is not a string, element=" << element << ".";
            strVec.emplace_back(element);
        }
    }
    return true;
}

} // namespace rmrs