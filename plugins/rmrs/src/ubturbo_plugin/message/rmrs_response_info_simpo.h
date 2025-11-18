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

#ifndef RESPONSE_INFO_SIMPO_H
#define RESPONSE_INFO_SIMPO_H

#include "rmrs_error.h"
#include "rmrs_serialize.h"

namespace rmrs {
using serialize::RmrsInStream;
using serialize::RmrsOutStream;

struct RMRSResponseInfo {
    unsigned int code;
    std::string message;
};

inline void DefaultFreeFunc(const uint8_t *data)
{
    delete[] data;
}

class RMRSResponseInfoSimpo {
public:
    RMRSResponseInfoSimpo() = default;

    inline RMRSResponseInfo GetResponseInfo()
    {
        return responseInfo_;
    }

    inline void SetResponseInfo(const int code, const std::string &message)
    {
        responseInfo_.code = code;
        responseInfo_.message = message;
    }

    std::string ToString() const
    {
        return "code: " + std::to_string(responseInfo_.code) + "; message: " + responseInfo_.message;
    };

    RMRSResponseInfo responseInfo_{};
};
} // namespace rmrs

#endif // RESPONSE_INFO_SIMPO_H
