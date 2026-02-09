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
#include <cerrno>

#include "turbo_ipc_client.h"
#include "smap_interface.h"
#include "smap_handler_msg.h"
#include "ulog.h"

using namespace turbo::smap::codec;
using namespace turbo::ipc::client;
using namespace turbo::smap::ulog;

int ubturbo_smap_migrate_out(struct MigrateOutMsg *msg, int pidType)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapMigrateOutCodec handler;
    if (!msg) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Migrate out msg is null.\n");
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, msg, pidType);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_migrate_out Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_migrate_out", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_migrate_out error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_migrate_out Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_migrate_back(struct MigrateBackMsg *msg)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapMigrateBackCodec handler;
    if (!msg) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Migrate back msg is null.\n");
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, msg);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_migrate_back Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_migrate_back", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_migrate_back error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_migrate_back Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_remove(struct RemoveMsg *msg, int pidType)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapRemoveCodec handler;
    if (!msg) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Remove msg is null.\n");
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, msg, pidType);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_remove Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_remove", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_remove error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_remove Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_node_enable(struct EnableNodeMsg *msg)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapEnableNodeCodec handler;
    if (!msg) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Enable node msg is null.\n");
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, msg);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_node_enable Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_node_enable", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_node_enable error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_node_enable Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

static int InitLog(Logfunc extlog)
{
    if (!extlog) {
        return -EINVAL;
    }
    UpstreamSubscribeLogger(extlog);
    return 0;
}

int ubturbo_smap_start(uint32_t pageType, Logfunc extlog)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapInitCodec handler;

    int ret = InitLog(extlog);
    if (ret) {
        return ret;
    }
    ret = handler.EncodeRequest(send, pageType);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_start Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_start", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_start error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_start Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_stop(void)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapStopCodec handler;
    int ret = handler.EncodeRequest(send);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_stop Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_stop", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_stop error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_stop Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

void ubturbo_smap_urgent_migrate_out(uint64_t size)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapUrgentMigrateOutCodec handler;
    int ret = handler.EncodeRequest(send, size);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_urgent_migrate_out Encode request error %d.\n", ret);
        return;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_urgent_migrate_out", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_urgent_migrate_out error %u.\n", ipcRet);
        delete[] send.data;
        return;
    }
    delete[] send.data;
    delete[] recv.data;
}

int ubturbo_smap_process_tracking_add(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapAddProcessTrackingCodec handler;
    if (!pidArr || !scanTime || !duration) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Add process tracking pidArr or scanTime is null.\n");
        return -EINVAL;
    }
    if (len <= 0 || len > MAX_NR_MIGOUT) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Add process tracking len is %d.\n", len);
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, pidArr, scanTime, duration, len, scanType);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_process_tracking_add Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_process_tracking_add", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_process_tracking_add error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_process_tracking_add Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_process_tracking_remove(pid_t *pidArr, int len, int flag)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapRemoveProcessTrackingCodec handler;
    if (!pidArr) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Remove process trackking pidArr is null.\n");
        return -EINVAL;
    }
    if (len <= 0 || len > MAX_NR_MIGOUT) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Remove process tracking len is %d.\n", len);
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, pidArr, len, flag);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_process_tracking_remove Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_process_tracking_remove", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_process_tracking_remove error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_process_tracking_remove Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_process_migrate_enable(pid_t *pidArr, int len, int enable, int flags)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapEnableProcessMigrateCodec handler;
    if (!pidArr) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Enable process migrate pidArr is null.\n");
        return -EINVAL;
    }
    if (len <= 0 || len > MAX_NR_MIGOUT) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Enable process migrate len is %d.\n", len);
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, pidArr, len, enable, flags);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_process_migrate_enable Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_process_migrate_enable", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_process_migrate_enable error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_process_migrate_enable Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_remote_numa_info_set(struct SetRemoteNumaInfoMsg *msg)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SetSmapRemoteNumaInfoCodec handler;
    if (!msg) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Set smap remote numa info msg is null.\n");
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, msg);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_remote_numa_info_set Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_remote_numa_info_set", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_remote_numa_info_set error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_remote_numa_info_set Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_freq_query(int pid, uint16_t *data, uint32_t lengthIn, uint32_t *lengthOut, int dataSource)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapQueryVmFreqCodec handler;
    if (!data || !lengthOut) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Query vm freq data or lengthOut is null.\n");
        return -EINVAL;
    }
    if (lengthIn == 0) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Query vm freq lengthIn is %d.\n", lengthIn);
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, pid, lengthIn, dataSource);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_freq_query Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_freq_query", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_freq_query error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    int result = handler.DecodeResponse(recv, data, *lengthOut, ret);
    if (result == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_freq_query Decode response error %d.\n", result);
        ret = IPC_ERROR;
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_run_mode_set(int runMode)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SetSmapRunModeCodec handler;
    int ret = handler.EncodeRequest(send, runMode);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_run_mode_set Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_run_mode_set", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_run_mode_set error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_run_mode_set Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

bool ubturbo_smap_is_running(void)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapIsRunningCodec handler;
    int ret = handler.EncodeRequest(send);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_is_running Encode request error %d.\n", ret);
        return false;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_is_running", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_is_running error %u.\n", ipcRet);
        delete[] send.data;
        return false;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_is_running Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_migrate_out_sync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapMigrateOutSyncCodec handler;
    if (!msg) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Migrate out sync msg is null.\n");
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, msg, pidType, maxWaitTime);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_migrate_out_sync Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_migrate_out_sync", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_migrate_out_sync error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_migrate_out_sync Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_remote_numa_migrate(struct MigrateNumaMsg *msg)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapMigrateRemoteNumaCodec handler;
    if (!msg) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Migrate remote numa msg is null.\n");
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, msg);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_remote_numa_migrate Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_remote_numa_migrate", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_remote_numa_migrate error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_remote_numa_migrate Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_pid_remote_numa_migrate(struct MigrateEscapeMsg *msg)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapMigratePidRemoteNumaCodec handler;
    if (!msg) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Migrate pid remote numa msg is null.\n");
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, msg);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_pid_remote_numa_migrate Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_pid_remote_numa_migrate", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_pid_remote_numa_migrate error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    ret = handler.DecodeResponse(recv);
    if (ret == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_pid_remote_numa_migrate Decode response error %d.\n", ret);
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_process_config_query(int nid, struct ProcessPayload *result, int inLen, int *outLen)
{
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapQueryProcessConfigCodec handler;
    if (!result || !outLen) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Query process config result or outLen is null.\n");
        return -EINVAL;
    }
    if (inLen <= 0 || inLen > MAX_4K_PROCESSES_CNT) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Query process config inLen is %d.\n", inLen);
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, nid, inLen);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_process_config_query Encode request error %d.\n", ret);
        return IPC_ERROR;
    }
    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_process_config_query", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_process_config_query error %u.\n", ipcRet);
        delete[] send.data;
        return ipcRet;
    }
    int tmp = handler.DecodeResponse(recv, result, *outLen, ret);
    if (tmp == IPC_ERROR) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] SmapQueryProcessConfig Decode response error %d.\n", tmp);
        ret = IPC_ERROR;
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}

int ubturbo_smap_remote_numa_freq_query(uint16_t *numa, uint64_t *freq, uint16_t length)
{
    uint16_t outLen;
    TurboByteBuffer send;
    TurboByteBuffer recv;
    SmapQueryRemoteNumaFreqCodec handler;

    if (!numa || !freq) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Query remote numa freq numa or freq is null.\n");
        return -EINVAL;
    }
    if (length == 0 || length > REMOTE_NUMA_BITS) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Query remote numa freq length is %d.\n", length);
        return -EINVAL;
    }
    int ret = handler.EncodeRequest(send, numa, length);
    if (ret) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_remote_numa_freq_query Encode request error %d.\n", ret);
        return IPC_ERROR;
    }

    uint32_t ipcRet = UBTurboFunctionCaller("ubturbo_smap_remote_numa_freq_query", send, recv);
    if (ipcRet != IPC_OK) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] Call ubturbo_smap_remote_numa_freq_query error %u.\n", ipcRet);
        return ipcRet;
    }

    int result = handler.DecodeResponse(recv, freq, outLen, ret);
    if (result == IPC_ERROR || outLen != length) {
        IPC_CLIENT_LOGGER_ERROR("[Smap] ubturbo_smap_remote_numa_freq_query Decode response error %d.\n", result);
        ret = IPC_ERROR;
    }
    delete[] send.data;
    delete[] recv.data;
    return ret;
}