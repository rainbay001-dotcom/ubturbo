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

#ifndef RMRS_JSON_UTIL_H
#define RMRS_JSON_UTIL_H

#include <map>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>


#include "turbo_logger.h"
#include "rmrs_config.h"

namespace rmrs {
using namespace turbo::log;
using namespace rapidjson;

using JSON_STR = std::string;
using JSON_VEC = std::vector<std::string>;
using JSON_MAP = std::map<std::string, std::string>;
using JSON_SET = std::set<std::string>;

class JsonUtil {
public:
    /* 将document对象打印成string字符串 */
    static JSON_STR PrintJsonString(const rapidjson::Document& doc);

    /* 将Value对象打印成string字符串 */
    static JSON_STR PrintJsonString(const rapidjson::Value& value);

    /* 返回pstJson中key对应的子节点的json字符串 */
    static bool RackMemGetJsonItemStr(const rapidjson::Document &doc, const char *key, JSON_STR &jsonStr);

    /* 将map转为json字符串，其中map的value也可以是json字符串 */
    static bool RackMemConvertMap2JsonStr(const JSON_MAP &strMap, JSON_STR &jsonStr);

    /* 将object类型的json字符串转为map，其中map的key须提前填充，value也可以是json字符串 */
    static bool RackMemConvertJsonStr2Map(const JSON_STR &jsonStr, JSON_MAP &strMap);

    /* 将vector转为json字符串，其中map的value也可以是json字符串 */
    static bool RackMemConvertVector2JsonStr(const JSON_VEC &strVec, JSON_STR &jsonStr);

    /* 将array类型的json字符串转为map，value也可以是json字符串 */
    static bool RackMemConvertJsonStr2Vec(const JSON_STR &jsonStr, JSON_VEC &strVec);
};
} // namespace rmrs

#endif // RMRS_JSON_UTIL_H