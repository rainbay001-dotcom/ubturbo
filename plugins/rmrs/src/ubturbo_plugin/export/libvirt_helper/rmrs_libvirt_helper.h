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
#ifndef RMRS_LIBVIRT_HELPER_H
#define RMRS_LIBVIRT_HELPER_H

#include <mutex>

#include <map>
#include "turbo_logger.h"
#include "rmrs_libvirt_module.h"
#include "rmrs_error.h"
#include "rmrs_resource_export.h"
#include "rmrs_vm_info.h"

namespace rmrs {
using std::string;
using namespace rmrs::exports;
using namespace turbo::log;
using libvirt::VirConnectListAllDomainsFlags;
using libvirt::VirConnectPtr;
using libvirt::VirDomainInfo;
using libvirt::VirVcpuInfo;

const size_t VM_UUID_LEN = 37; // 36(UUID位数) + 1(\0)

class LibvirtHelper {
public:
    static void FreeDomains(void **&domains, size_t domainNums);
    static uint32_t GetVmUuid(ResourceExport *vmInfoHandler, void *domain, VmDomainInfo &vmDomainInfo);
    static uint32_t GetVmStatus(void *domain, VmDomainInfo &vmInfo, VirDomainInfo &info);

    static uint32_t Init();
    uint32_t Connect();
    uint32_t CloseConn();
    bool IsConnectAlive();
    uint32_t GetDomainList(void **&domains, int &numDomains);
    uint32_t GetVmBasicInfo(ResourceExport *vmInfoHandler);
    uint32_t GetHostName(string &hostName);

private:
    static uint32_t GetVmVCpuInfo(ResourceExport *vmInfoHandler, VirDomainInfo info, void *domain,
                                  VmDomainInfo &vmInfo);
    static void UpdateVmMemInfoOnNuma(ResourceExport *vmInfoHandler, VmDomainInfo &vmInfo);

    static uint32_t GetVirDomainVCpus(void *domain, VirDomainInfo info, VirVcpuInfo **virCpu, int &cpuNums);

    VirConnectPtr virConnect{};
};
} // namespace mempooling

#endif // MP_LIBVIRT_HELPER_H
