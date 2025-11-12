/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef __VIRT_H__
#define __VIRT_H__

#include <libvirt/libvirt.h>
#include <sys/types.h>

int ReadDomainIdByPid(pid_t pid, int *id);
void CloseVirConn(virConnectPtr *virConn);
int OpenVirConn(virConnectPtr *virConn);
void CloseVirHandler(void);
int CheckVirConn(virConnectPtr *virConn);
int OpenVirHandler(void);
/**
 * @par Parse XML with libvirt API
 *
 * @param domainId: Id of vm domain, get by call ReadDomainIdByPid()
 * @return: a 0 terminated UTF-8 encoded XML instance, or NULL in case of error.
 * The caller must free() the return value.
 */
char *GetXMLByDomainId(int domainId);

#endif /* __VIRT_H__ */
