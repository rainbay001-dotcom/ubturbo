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
#ifndef RACK_ERROR_H
#define RACK_ERROR_H
#include <cstdint>

/**
 * 描述:生成模块内的私有错误码。(模块ID + 模块内具体错误码 = 完整的错误码)
 * MID（高两个字节）＋ERRNO（低两个字节）
 * 如错误码：0x10081000
 * 高字节0x1008表示模块ID：即0x1000 + 8，表示communication子系统的net模块，可在本头文件找到RACK_COMMUNICATION_MID_NET
 * 低字节0x1000表示错误类型：0x1000 + 0，表示模块内的私有错误码基础值 + 错误码，可在头文件rack_net_error.h中找到错误类型
 */
#define RACK_ERROR_BEGIN_USER 0X1000                                 /* 模块内的私有错误码基础值 */
const int SIXTEEN = 16;
constexpr uint32_t RACK_ERROR_USERNO(uint32_t n)
{
    return RACK_ERROR_BEGIN_USER + n;
}
constexpr uint32_t RACK_MID_HI16(uint32_t MID)
{
    return (MID) << SIXTEEN;
}


/* 各个子系统，MID分段起始ID定义，各个模块定义时选择相应的起始ID */
#define RACK_MID_BEGIN0 0x0000 /* common        */
#define RACK_MID_BEGIN1 0x1000 /* Rack Manager  */
#define RACK_MID_BEGIN2 0x2000 /* Rack CLI  */

/* 宏定义各个子模块MID计算方法 */
constexpr uint32_t RACK_COMMON_ERROR(uint32_t n)
{
    return RACK_MID_BEGIN0 + n;
}
constexpr uint32_t RACK_MID_MAKE_Manager(uint32_t n)
{
    return RACK_MID_BEGIN1 + n;
}
constexpr uint32_t RACK_MID_MAKE_CLI(uint32_t n)
{
    return RACK_MID_BEGIN2 + n;
}

/* ********************************************* */
/* common错误码定义，全局唯一，记录系统的标准错误返回 */
/* ********************************************* */

#define RACK_OK RACK_COMMON_ERROR(0)                         /* 正确 */
#define RACK_ERROR RACK_COMMON_ERROR(1)                      /* 错误 */
#define RACK_ERROR_NOENT RACK_COMMON_ERROR(2)                /* No such file or directory */
#define RACK_ERROR_NOMEM RACK_COMMON_ERROR(3)                /* Out of memory */
#define RACK_ERROR_ACCES RACK_COMMON_ERROR(4)                /* Permission denied */
#define RACK_ERROR_SRCH RACK_COMMON_ERROR(5)                 /* No such process */
#define RACK_ERROR_EXIST RACK_COMMON_ERROR(6)                /* File exists */
#define RACK_ERROR_NOSPC RACK_COMMON_ERROR(7)                /* No space left on device */
#define RACK_ERROR_AGAIN RACK_COMMON_ERROR(8)                /* Try again */
#define RACK_ERROR_IO RACK_COMMON_ERROR(9)                   /* I/O error */
#define RACK_ERROR_BADF RACK_COMMON_ERROR(10)                /* Bad file descriptor */
#define RACK_ERROR_CONF_INVALID RACK_COMMON_ERROR(11)        /* 非法的配置文件 */
#define RACK_ERROR_NULLPTR RACK_COMMON_ERROR(12)             /* 空指针 */
#define RACK_MASTER_EMPTY_VECTOR_ERROR RACK_COMMON_ERROR(13) /* 空数组 */
#define RACK_ERROR_INVAL RACK_COMMON_ERROR(14)               /* Invalid argument */
#define RACK_ERROR_MODULE_LOAD_FAILED RACK_COMMON_ERROR(15)  /* 模块加载失败 */
#define RACK_ERROR_CLI_ARGS_FAILED RACK_COMMON_ERROR(16)     /* 解析参数失败 */


/* **************************************** */
/* 各个模块MID定义                            */
/* 0:基础模块                                */
/* 1:com模块                                */
/* 2:config模块                             */
/* 3:event模块                              */
/* 4:http模块                               */
/* 5:log模块                                */
/* 6:message模块                            */
/* 7:plugin模块                             */
/* 8:role模块                               */
/* 9:storage模块                            */
/* 10:telemetry模块                         */
/* 11:资源管理框架                            */
/* 30:kmc加解密模块                          */
/* **************************************** */
#define RACK_MANAGER_MID_BASE RACK_MID_MAKE_Manager(0) /* 0X1000 基础模块 */

#define RACK_COM_MID RACK_MID_MAKE_Manager(1)            /* 0X1001 */
#define RACK_CONF_MID RACK_MID_MAKE_Manager(2)           /* 0X1002 */
#define RACK_EVENT_MID RACK_MID_MAKE_Manager(3)          /* 0X1003 */
#define RACK_HTTP_MID RACK_MID_MAKE_Manager(4)           /* 0X1004 */
#define TURBO_LOG_MID RACK_MID_MAKE_Manager(5)            /* 0X1005 */
#define RACK_MESSAGE_MID RACK_MID_MAKE_Manager(6)        /* 0X1006 */
#define RACK_PLUGIN_MID RACK_MID_MAKE_Manager(7)         /* 0X1007 */
#define RACK_ROLE_MID RACK_MID_MAKE_Manager(8)           /* 0X1008 */
#define RACK_STORAGE_MID RACK_MID_MAKE_Manager(9)        /* 0X1009 */
#define RACK_TELEMETRY_MID RACK_MID_MAKE_Manager(10)     /* 0X100A */
#define RACK_RESOURCE_MGR_MID RACK_MID_MAKE_Manager(11)  /* 0X100B */
#define RACK_DEV_MID RACK_MID_MAKE_Manager(12)           /* 0X100C */
#define RACK_UTILS_MID RACK_MID_MAKE_Manager(13)         /* 0X100D */
#define RACK_PARSE_MID RACK_MID_MAKE_Manager(14)         /* 0X100E */
#define RACK_SYSTEM_MID RACK_MID_MAKE_Manager(15)        /* 0X100F */
#define RACK_TASK_EXECUTOR_MID RACK_MID_MAKE_Manager(16) /* 0X1010 */
#define RACK_DBG_DEADLOOP_MID RACK_MID_MAKE_Manager(17)  /* 0X1011 */
#define RACK_SDK_REGISTER_MID RACK_MID_MAKE_Manager(18)  /* 0X1012 */

#define RACK_DBG_TRACE_MID RACK_MID_MAKE_Manager(18)        /* 0X1012 */
#define RACK_NODE_MID RACK_MID_MAKE_Manager(19)             /* 0X1013 */
#define RACK_DBG_EXCEPTION_MID RACK_MID_MAKE_Manager(20)    /* 0X1014 */
#define RACK_DBG_MEMORY_MID RACK_MID_MAKE_Manager(21)       /* 0X1015 */
#define RACK_FAULT_COLLECTION_MID RACK_MID_MAKE_Manager(22) /* 0X1016 */
#define RACK_REMOTE_MID RACK_MID_MAKE_Manager(23)           /* 0X1017 */
#define RACK_LCNE_MID RACK_MID_MAKE_Manager(24)             /* 0X1018 */
#define RACK_RPC_MID RACK_MID_MAKE_Manager(25)              /* 0X1019 */
#define RACK_DRIVER_MID RACK_MID_MAKE_Manager(26)           /* 0X101A */
#define RACK_PLUGIN_PROXY_MID RACK_MID_MAKE_Manager(27)     /* 0X101B */
#define RACK_ELECTION_MID RACK_MID_MAKE_Manager(28)         /* 0X101C */
#define RACK_DATA_SYNC_MID RACK_MID_MAKE_Manager(29)        /* 0X101D */
#define RACK_UBUS_MID RACK_MID_MAKE_Manager(30)             /* 0X101E */
#define RACK_KMC_CRYPT_MID RACK_MID_MAKE_Manager(30)        /* 0X101E */
#define RACK_PSK_MID RACK_MID_MAKE_Manager(30)        /* 0X101F */

/* Rack CLI */
#define RACK_CLI_MID_BASE RACK_MID_MAKE_CLI(0)   /* 0X2000 基础模块 */
#define RACK_CLI_MID_CLIENT RACK_MID_MAKE_CLI(1) /* 0X2001 */
#define RACK_CLI_MID_PARSE RACK_MID_MAKE_CLI(2)  /* 0X2002 */
#define RACK_CLI_MID_PLUGIN RACK_MID_MAKE_CLI(3) /* 0X2003 */
#define RACK_CLI_MID_SDK RACK_MID_MAKE_CLI(4)    /* 0X2004 */

/* 公共方法判断错误码 */
#define RACK_RESULT_FAIL(ret) (static_cast<RackResult>(ret) != RACK_OK)
#define RACK_RESULT_OK(ret) (static_cast<RackResult>(ret) == RACK_OK)

#endif // RACK_ERROR_H
