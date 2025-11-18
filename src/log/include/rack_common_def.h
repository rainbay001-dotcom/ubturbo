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
#ifndef RACK_COMMON_DEF_H
#define RACK_COMMON_DEF_H
#include <cstdint>
#include <map>
#include <string>

namespace rack::common::def {
using IpAddress = std::pair<std::string, uint16_t>;
using RackResult = uint32_t;

#ifndef RACK_LIKELY
#define RACK_LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef RACK_UNLIKELY
#define RACK_UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

// 数字常量宏，代表数字1，2，3，4...
constexpr int16_t NO_0 = 0;
constexpr int16_t NO_1 = 1;
constexpr int16_t NO_2 = 2;
constexpr int16_t NO_3 = 3;
constexpr int16_t NO_4 = 4;
constexpr int16_t NO_10 = 10;
constexpr int16_t NO_16 = 16;
constexpr int16_t NO_32 = 32;
constexpr int16_t NO_64 = 64;
constexpr int16_t NO_128 = 128;
constexpr int16_t NO_1000 = 1000;
constexpr int16_t NO_1024 = 1024;
constexpr int16_t NO_2048 = 2048;
constexpr int32_t NO_60000 = 60000;
constexpr int16_t NO_NEGATIVE_1 = -1;
constexpr int MAX_IPC_DATA_PACKAGE_LEN = 10485760;
constexpr int16_t DUMP_DELETE_LEN = -1;
constexpr int16_t RACK_UDS_MODE = 666;
constexpr size_t httpMaxBodySize = 524288;
constexpr size_t httpMaxStrSize = 504;
constexpr size_t httpMaxQuerySize = 11264;
constexpr uint32_t STACK_WANT_DEPTH = 20;
constexpr uint32_t STACK_IGNORE_DEPTH = 0;
constexpr uint32_t PRINTSIG = 35;
constexpr uint32_t PSKIDSIZE = 35;

const uint16_t MIN_PORT = 1024;
const uint16_t MAX_PORT = 65535;
const std::string MASTER_RPC_SERVER_NAME = "RackMasterRpcServer";
const std::string AGENT_RPC_SERVER_NAME = "RackAgentClient";
const std::string RACK_HCOM_FILE_PATH_PREFIX = "HCOM_FILE_PATH_PREFIX";
const std::string RACK_IPC_UDS_PATH_PREFIX_ENV = "RACK_IPC_UDS_PATH_PREFIX";
const std::string RACK_IPC_UDS_PATH_PREFIX_DEFAULT = "/run/scbus";
const std::string RACK_AGENT_IPC_SERVER_NAME = "RackAgentIpcServer";
const std::string RACK_AGENT_IPC_SERVER_ENGINE_NAME = "RackAgentIpcServerEngine";
const std::string RACK_AGENT_IPC_CLIENT_ENGINE_NAME = "RackAgentIpcClientEngine";
const std::string RACK_ROLE_MASTER = "Master";
const std::string RACK_ROLE_SLAVE = "Slaver";
const std::string RACK_ROLE_AGENT = "Agent";
const std::string RACK_COMMON_SECTION = "rack.common";
const std::string RACK_MASTER_ADDRESS_KEY = "masterAddress";
const std::string RACK_NODE_ID_KEY = "nodeId";
const std::string DEFAULT_UDS_ADDRESS = "/var/run/scbus/rackAgentUds.socket";
const std::string SYSTEM_GROUP = "scbus";
const std::string RACK_UDS_USER_VERIFY_SECTION = "rack_uds_user_verify";
const std::string RACK_ACCEPT_SERVICE_LIST_USER_KEY = "accept.service.list.user";
const std::string RACK_EVENT_SECTION = "rack.event";
const std::string RACK_EVENT_QUEUE_MAX_ITEM = "queue.maxItem";
const std::string RACK_EVENT_HIGH_THREAD_MAX_ITEM = "high.thread.maxItem";
const std::string RACK_EVENT_MEDIUM_THREAD_MAX_ITEM = "medium.thread.maxItem";
const std::string RACK_EVENT_LOW_THREAD_MAX_ITEM = "low.thread.maxItem";
const std::string RACK_EVENT_NODE_STATE = "rack.node.state";
const std::string RACK_EVENT_MEM_FAULT = "rack.mem.fault";
const std::string RACK_EVENT_XALARM_REBOOT = "ALARM_REBOOT_EVENT";
const std::string RACK_EVENT_XALARM_REBOOT_ACK = "ALARM_REBOOT_ACK_EVENT";
const std::string RACK_EVENT_FAULT_UPDATE = "rack.resource.fault.update";
const std::string MEM_FAULT_FILE_PATH = "/var/log/scbus/mem_fault";
const std::string ORCHESTRATE_REQUEST_URL = "/rack/resource_request/";
const std::string DOPRA_CFG_PATH = "/conf/dopra_default.cfg";
const std::string RACK_UDS_ADDRESS_ENV_VAR = "RACK_UDS_ADDRESS";
const std::string RACK_HTTP_INFO_LOG_KEY = "rack_http_info_log";
const std::string RACK_HTTP_INFO_LOG = "info.log";
const std::string RACK_CONFIG_STATUS_SUCCESS = "Success";
const std::string RACK_CONFIG_STATUS_FAIL = "Fail";
extern const std::string UDS_ADDRESS;
const std::string SSU_ONLINE_EVENT = "ssu.online";
const std::string SSU_OFFLINE_EVENT = "ssu.offline";
const std::string SSU_ONLINE_UPDATE_EVENT = "ssu.online.update";
const std::string SSU_OFFLINE_UPDATE_EVENT = "ssu.offline.update";
const std::string RACK_ELECTION_SECTION = "rack.election";
const std::string RACK_ELECTION_HEARTBEAT_TIME_INTERVAL = "heartbeat.timeInterval";
const std::string RACK_HTTP_SECTION = "rack.http";
const std::string RACK_HTTP_TCP_SERVER_PORT = "tcp.server.port";
const std::string COMMON_PSK_ID = "common_psk_id";
} // namespace rack::common::def
#endif // RACK_COMMON_DEF_H
