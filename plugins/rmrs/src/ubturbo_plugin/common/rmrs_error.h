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
#ifndef RMRS_ERROR_H
#define RMRS_ERROR_H

#include <cstdint>

namespace rmrs {

using RMRS_RES = uint32_t;
using RmrsResult = uint32_t;

/**
 * 描述:生成模块内的私有错误码。(模块ID + 模块内具体错误码 = 完整的错误码)
 * MID（高两个字节）＋ERRNO（低两个字节）
 * 如错误码：0x10081000
 * 高字节0x1008表示模块ID：即0x1000 + 8，表示communication子系统的net模块，可在本头文件找到RMRS_COMMUNICATION_MID_NET
 * 低字节0x1000表示错误类型：0x1000 + 0，表示模块内的私有错误码基础值 + 错误码，可在头文件RMRS_net_error.h中找到错误类型
 */
#define RMRS_ERROR_BEGIN_USER 0X1000                               /* 模块内的私有错误码基础值 */
#define RMRS_ERROR_USERNO(n) (uint32_t(RMRS_ERROR_BEGIN_USER + (n))) /* 计算模块内的私有错误码加基础值 */
#define RMRS_MID_HI16(MID) (uint32_t((MID) << 16))                 /* 模块ID左移到高字节 */

/* 各个子系统，MID分段起始ID定义，各个模块定义时选择相应的起始ID */
#define RMRS_MID_BEGIN0 0x0000 /* common        */
#define RMRS_MID_BEGIN1 0x1000 /* Rack Manager  */

/* 宏定义各个子模块MID计算方法 */
#define RMRS_COMMON_ERROR(n) (uint32_t(RMRS_MID_BEGIN0 + (n)))     /* common        */
#define RMRS_MID_MAKE_MANAGER(n) (uint32_t(RMRS_MID_BEGIN1 + (n))) /* Rack Manager  */

#define RMRS_MASTER_MID_BASE RMRS_MID_MAKE_MANAGER(1) /* 0X1001 rack master base */
#define RMRS_MASTER_MID_VM RMRS_MID_MAKE_MANAGER(2)   /* 0X1002 rack master vm */
#define RMRS_MASTER_MID_MEM RMRS_MID_MAKE_MANAGER(3)  /* 0X1003 rack master mem */

#define RMRS_AGENT_MID_BASE RMRS_MID_MAKE_MANAGER(4) /* 0X1004 rack agent base */
#define RMRS_AGENT_MID_VM RMRS_MID_MAKE_MANAGER(5)   /* 0X1005 rack agent vm */
#define RMRS_AGENT_MID_MEM RMRS_MID_MAKE_MANAGER(6)  /* 0X1006 rack agent mem */

#define RMRS_CLOUD_ADAPTER_MID RMRS_MID_MAKE_MANAGER(7) /* 0X1007 cloud adapter */

#define RMRS_COMMUNICATION_MID_NET RMRS_MID_MAKE_MANAGER(8)  /* 0X1008 通信的net模块 */
#define RMRS_COMMUNICATION_MID_HTTP RMRS_MID_MAKE_MANAGER(9) /* 0X1009 通信的http模块 */

#define RMRS_DB_MID_LDC RMRS_MID_MAKE_MANAGER(10) /* 0X100A LDC模块 */

#define RMRS_SERIALIZE_MID_BASE RMRS_MID_MAKE_MANAGER(11) /* 0X100B 序列化模块 */

#define RMRS_MONITOR_MID_BASE RMRS_MID_MAKE_MANAGER(12) /* 0X100C 设备监听模块 */

/* ********************************************* */
/* common错误码定义，全局唯一，记录系统的标准错误返回 */
/* ********************************************* */

#define RMRS_ERROR_SIGN_INT (-1)                           /* 错误, 有符号数-1 */
#define RMRS_OK RMRS_COMMON_ERROR(0)                         /* 正确 */
#define RMRS_ERROR RMRS_COMMON_ERROR(1)                      /* 错误 */
#define RMRS_ERROR_NOENT RMRS_COMMON_ERROR(2)                /* No such file or directory */
#define RMRS_ERROR_NOMEM RMRS_COMMON_ERROR(3)                /* Out of memory */
#define RMRS_ERROR_ACCES RMRS_COMMON_ERROR(4)                /* Permission denied */
#define RMRS_ERROR_SRCH RMRS_COMMON_ERROR(5)                 /* No such process */
#define RMRS_ERROR_EXIST RMRS_COMMON_ERROR(6)                /* File exists */
#define RMRS_ERROR_NOSPC RMRS_COMMON_ERROR(7)                /* No space left on device */
#define RMRS_ERROR_AGAIN RMRS_COMMON_ERROR(8)                /* Try again */
#define RMRS_ERROR_IO RMRS_COMMON_ERROR(9)                   /* I/O error */
#define RMRS_ERROR_BADF RMRS_COMMON_ERROR(10)                /* Bad file descriptor */
#define RMRS_ERROR_CONF_INVALID RMRS_COMMON_ERROR(11)        /* 非法的配置文件 */
#define RMRS_ERROR_NULLPTR RMRS_COMMON_ERROR(12)             /* 空指针 */
#define RMRS_MASTER_EMPTY_VECTOR_ERROR RMRS_COMMON_ERROR(13) /* 空数组 */
#define RMRS_ERROR_INVAL RMRS_COMMON_ERROR(14)               /* Invalid argument */
#define RMRS_ERROR_EXCEEDS_RANGE RMRS_COMMON_ERROR(15)       /* Out of range */
#define RMRS_WARN RMRS_COMMON_ERROR(16)                      /* 告警错误码 */

/* **************************************** */
/* http模块错误码定义                        */
/* **************************************** */

/* 0x10021000 rack master vm invalid strategy */
#define RMRS_INVALID_STRATEGY_ERROR (RMRS_MID_HI16(RMRS_MASTER_MID_VM) | \
		RMRS_ERROR_USERNO(0x00))

/* 0x10021001 rack master vm reclaim mem error */
#define RMRS_RECLAIM_MEMORY_ERROR (RMRS_MID_HI16(RMRS_MASTER_MID_VM) | \
		RMRS_ERROR_USERNO(0x01))

/* 0x10021002 rack master vm migrate error */
#define RMRS_MIGRATE_ERROR (RMRS_MID_HI16(RMRS_MASTER_MID_VM) | \
		RMRS_ERROR_USERNO(0x02))

/* 0x10021003 rack master return memory invalid param error */
#define RMRS_INVALID_PARAM_ERROR (RMRS_MID_HI16(RMRS_MASTER_MID_VM) | \
		RMRS_ERROR_USERNO(0x03))

#define RMRS_RESULT_FAIL(ret) (static_cast<VmResult>(ret) != RMRS_OK)
#define RMRS_RESULT_OK(ret) (static_cast<VmResult>(ret) == RMRS_OK)

/* **************************************** */
/* 序列化模块错误码定义                       */
/* **************************************** */

/* 0x100B1000  序列化错误 */
#define RMRS_ERROR_SERIALIZE_ERROR (RMRS_MID_HI16(RMRS_SERIALIZE_MID_BASE) | \
		RMRS_ERROR_USERNO(0x00))

/* 0x100B1001 反序列化错误 */
#define RMRS_ERROR_DESERIALIZE_ERROR (RMRS_MID_HI16(RMRS_SERIALIZE_MID_BASE) | \
		RMRS_ERROR_USERNO(0x01))

/* 0x100B1002 序列化反序列化公共错误 */
#define RMRS_ERROR_SERIALIZE_DESERIALIZE_COMMON_ERROR (RMRS_MID_HI16(RMRS_SERIALIZE_MID_BASE) | \
		RMRS_ERROR_USERNO(0x02))
}

#endif // RMRS_ERROR_H
