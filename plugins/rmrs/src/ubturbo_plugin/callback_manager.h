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
#ifndef RMRS_CALLBACK_MANAGER_H
#define RMRS_CALLBACK_MANAGER_H

#include "rmrs_error.h"
#include "turbo_ipc_server.h"
#include "rmrs_response_info_simpo.h"
namespace rmrs {

class CallbackManager {
public:
    static RmrsResult Init();

    static RmrsResult InitSmap();

    static RmrsResult InitExport();

    static RmrsResult DeInitExport();

    static RmrsResult SetResponse(RMRSResponseInfoSimpo &response, const RmrsResult &retCode, const std::string &msg,
                                                   TurboByteBuffer &resBuffer);
    static RmrsResult MigrateBackRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);

    static RmrsResult BorrowRollbackRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);

    static RmrsResult MigrateExecuteRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);

    static RmrsResult MigrateStrategyRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);

    static RmrsResult HeartBeatRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);

    static RmrsResult PidNumaInfoCollectRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);

    static RmrsResult NumaMemInfoCollectRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);

    static RmrsResult UCacheMigrateStrategyRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);

    static RmrsResult UCacheMigrateStopRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);

    static RmrsResult UpdateUCacheRatioRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp);
private:
    static bool isSetSmapMode;

    static const int memFragmentationMode; // smapRunmode设置为碎片场景--参数为1
};

}  // namespace rmrs

#endif