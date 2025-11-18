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
#include "rmrs_libvirt_helper.h"

#include "rmrs_config.h"
#include "rmrs_time_util.h"
#include "rmrs_vm_info.h"
#include "turbo_logger.h"

namespace rmrs {
using std::lock_guard;
using std::nothrow;

using namespace libvirt;

static std::map<unsigned char, std::string> VirStateStringMap = {{0, "NOSTATE"}, {1, "RUNNING"},     {2, "BLOCKED"},
                                                                 {3, "PAUSED"},  {4, "SHUTDOWN"},    {5, "SHUTOFF"},
                                                                 {6, "CRASHED"}, {7, "PMSUSPENDED"}, {8, "LAST"}};

uint32_t LibvirtHelper::Init()
{
    uint32_t ret = LibvirtModule::Init();
    if (ret != RMRS_OK) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Init failed, LibvirtModule Init failed.";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

/**
 * 连接Libvirt
 * @return uint32_t
 */
uint32_t LibvirtHelper::Connect()
{
    try {
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Start to get libvirt connection.";
        virConnect = LibvirtModule::VirConnectOpen()("qemu:///system");
        if (virConnect == nullptr) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsResourceExport] [LibvirtHelper] Libvirt conn "
                   "failed, please check the virsh environment. error is "
                << strerror(errno) << ".";
            return RMRS_ERROR;
        }
        UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[LibvirtHelper] Libvirt connection succeed.";
        return RMRS_OK;
    } catch (std::exception &e) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Connect libvirt failed. err: " << e.what() << ".";
        return RMRS_ERROR;
    }
}

/**
 * 关闭Libvirt连接
 * @return
 */
uint32_t LibvirtHelper::CloseConn()
{
    UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[RmrsResourceExport] [LibvirtHelper] Start to close libvirt connect.";
    if (!virConnect) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Libvirt connect is empty.";
        return RMRS_ERROR;
    }

    const int maxRetry = 10000;
    const int sleepMilliseconds = 100;
    for (int i = 0; i < maxRetry; ++i) {
        int ret = LibvirtModule::VirConnectClose()(virConnect);
        if (ret == 0) {
            UBTURBO_LOG_INFO(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsResourceExport] [LibvirtHelper] Libvirt connection closed successfully.";
            return RMRS_OK;
        } else if (ret < 0) {
            UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsResourceExport] [LibvirtHelper] Failed to close libvirt connection, ret=" << ret;
            return RMRS_ERROR;
        } else {
            UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsResourceExport] [LibvirtHelper] Connection still has referencesk, retry " << (i + 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMilliseconds));
        }
    }

    UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
        << "[RmrsResourceExport] [LibvirtHelper] Failed to close libvirt connection after max retries.";

    return RMRS_ERROR;
}

bool LibvirtHelper::IsConnectAlive()
{
    libvirt::VirConnectIsAliveFunc virConnectIsAlive = LibvirtModule::VirConnectIsAlive();
    if (virConnectIsAlive == nullptr) {
        return false;
    }
    if (virConnect != nullptr && virConnectIsAlive(virConnect) > 0) {
        return true;
    }
    return false;
}

/**
 * 采集虚拟机列表
 * @param vmInfoHandler
 * @return
 */
uint32_t LibvirtHelper::GetVmBasicInfo(ResourceExport *vmInfoHandler)
{
    void **domains = nullptr;
    int numDomains = 0;
    auto ret = GetDomainList(domains, numDomains);
    if (ret != RMRS_OK) {
        FreeDomains(domains, numDomains);
        return ret;
    }
    libvirt::VirDomainGetNameFunc virDomainGetName = LibvirtModule::VirDomainGetName();
    if (virDomainGetName == nullptr) {
        FreeDomains(domains, numDomains);
        return RMRS_ERROR;
    }
    if (vmInfoHandler == nullptr) {
        return RMRS_ERROR;
    }
    auto uuidNameMap = vmInfoHandler->GetUuidNameMap();
    for (auto i = 0; i < numDomains; ++i) {
        VmDomainInfo vmInfo{};
        void *domain = domains[i];
        // 获取虚拟机名称, 获取虚机UUID
        vmInfo.metaData.name = string(virDomainGetName(domain));
        ret = GetVmUuid(vmInfoHandler, domain, vmInfo);
        if (ret != RMRS_OK) {
            FreeDomains(domains, numDomains);
            return ret;
        }
        (*uuidNameMap)[vmInfo.metaData.uuid] = vmInfo.metaData.name;
        // 获取虚机状态、申请内存、使用内存
        VirDomainInfo info{};
        ret = GetVmStatus(domain, vmInfo, info);
        if (ret != RMRS_OK) {
            FreeDomains(domains, numDomains);
            return ret;
        }
        // 获取cpuNums、cpuInfos信息, 维护vmNumaCpuInfos、vmNumaCpuIds
        ret = GetVmVCpuInfo(vmInfoHandler, info, domain, vmInfo);
        if (ret != RMRS_OK) {
            FreeDomains(domains, numDomains);
            return RMRS_ERROR;
        }
        if (vmInfo.cpuInfos.empty()) {
            continue;
        }
        UpdateVmMemInfoOnNuma(vmInfoHandler, vmInfo);
    }
    FreeDomains(domains, numDomains);
    return RMRS_OK;
}

/**
 * 采集virsh中hostName
 * @param hostName
 * @return
 */
uint32_t LibvirtHelper::GetHostName(string &hostName)
{
    libvirt::VirConnectGetHostnameFunc virConnectGetHostname = LibvirtModule::VirConnectGetHostname();
    if (virConnectGetHostname == nullptr) {
        return RMRS_ERROR;
    }
    char *pHostName = virConnectGetHostname(virConnect);
    if (pHostName == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Libvirt get hostName failed. " << strerror(errno) << ".";
        return RMRS_ERROR;
    }
    hostName = string(pHostName);
    return RMRS_OK;
}

uint32_t LibvirtHelper::GetVmUuid(ResourceExport *vmInfoHandler, void *domain, VmDomainInfo &vmDomainInfo)
{
    auto uuidList = vmInfoHandler->GetUuidList();

    libvirt::VirDomainGetUUIDStringFunc virDomainGetUUIDString = LibvirtModule::VirDomainGetUUIDString();
    if (virDomainGetUUIDString == nullptr) {
        return RMRS_ERROR;
    }

    char uuid[VM_UUID_LEN];
    auto ret = virDomainGetUUIDString(domain, uuid);
    if (ret < 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Get vmDomain UUID failed. " << strerror(errno) << ".";
        return RMRS_ERROR;
    }
    vmDomainInfo.metaData.uuid = string(uuid);
    uuidList->emplace_back(vmDomainInfo.metaData.uuid);
    return RMRS_OK;
}

uint32_t LibvirtHelper::GetVmStatus(void *domain, VmDomainInfo &vmInfo, VirDomainInfo &info)
{
    libvirt::VirDomainGetInfoFunc virDomainGetInfo = LibvirtModule::VirDomainGetInfo();
    if (virDomainGetInfo == nullptr) {
        return RMRS_ERROR;
    }
    // 获取虚拟机内存信息
    vmInfo.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto ret = virDomainGetInfo(domain, &info);
    if (ret < 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Get vm domain info error.";
        return RMRS_ERROR;
    }
    vmInfo.state = VirStateStringMap[info.state];
    vmInfo.metaData.maxMem = info.maxMem;
    vmInfo.metaData.usedMem = info.memory;
    vmInfo.metaData.cpuTime = info.cpuTime;
    return RMRS_OK;
}

uint32_t LibvirtHelper::GetVmVCpuInfo(ResourceExport *vmInfoHandler, VirDomainInfo info, void *domain,
                                      VmDomainInfo &vmInfo)
{
    if (vmInfoHandler == nullptr || domain == nullptr) {
        return RMRS_ERROR;
    }
    auto cpuNumaMap = ResourceExport::GetCpuNumaMap();
    auto cpuSocketMap = ResourceExport::GetCpuSocketMap();
    auto vmNumaCpuInfos = vmInfoHandler->GetVmNumaCpuInfos();
    auto vmNumaCpuIds = vmInfoHandler->GetVmNumaCpuIds();
    auto *virCpu = static_cast<VirVcpuInfo *>(malloc(sizeof(VirVcpuInfo) * info.nrVirtCpu));
    if (virCpu == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] malloc virCpu failed. " << strerror(errno) << ".";
        return RMRS_ERROR;
    }
    int cpuNums = 0;
    auto ret = GetVirDomainVCpus(domain, info, &virCpu, cpuNums);
    if (ret != RMRS_OK) {
        free(virCpu);
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Get vm cpu info failed.";
        return RMRS_ERROR;
    }
    vmInfo.metaData.cpuNums = cpuNums;
    if (cpuNums < 0) {
        UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE) << "[RmrsResourceExport] [LibvirtHelper] cpuNums is 0.";
        free(virCpu);
        return RMRS_OK;
    }
    VmCpuInfo vmCpuInfo{};
    for (auto n = 0; n < cpuNums; n++) {
        // 获取cpuInfos
        vmCpuInfo.cpuId = virCpu[n].cpu;
        vmCpuInfo.vCpuId = virCpu[n].number;
        vmCpuInfo.cpuNumaId = (*cpuNumaMap)[vmCpuInfo.cpuId];
        vmCpuInfo.cpuTime = static_cast<time_t>(virCpu[n].cpuTime);
        vmCpuInfo.socketId = (*cpuSocketMap)[vmCpuInfo.cpuId];
        vmInfo.cpuInfos.push_back(vmCpuInfo);
        // 虚机所绑定的numa上占用的cpu数量
        if (vmNumaCpuInfos->find(vmCpuInfo.cpuNumaId) != vmNumaCpuInfos->end()) {
            (*vmNumaCpuInfos)[vmCpuInfo.cpuNumaId]++;
        } else {
            (*vmNumaCpuInfos)[vmCpuInfo.cpuNumaId] = 1;
        }
        // 虚机所绑定的numa上占用的cpu列表
        (*vmNumaCpuIds)[vmCpuInfo.cpuNumaId].push_back(vmCpuInfo.cpuId);
    }
    free(virCpu);
    return RMRS_OK;
}

uint32_t LibvirtHelper::GetVirDomainVCpus(void *domain, VirDomainInfo info, VirVcpuInfo **virCpu, int &cpuNums)
{
    size_t cpuMapLen = 128; // 每个cpu长度占用128
    unsigned char *cpuMaps = static_cast<unsigned char *>(malloc(info.nrVirtCpu * cpuMapLen));

    if (cpuMaps == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] malloc cpuMaps failed. " << strerror(errno) << ".";
        return RMRS_ERROR;
    }

    libvirt::VirDomainGetVcpusFunc virDomainGetVcpus = LibvirtModule::VirDomainGetVcpus();
    if (virDomainGetVcpus == nullptr) {
        free(cpuMaps);
        return RMRS_ERROR;
    }
    cpuNums = virDomainGetVcpus(domain, *virCpu, info.nrVirtCpu, cpuMaps, cpuMapLen);
    if (cpuNums == -1) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] virDomainGetVcpus failed. " << strerror(errno) << ".";
        free(cpuMaps);
        return RMRS_ERROR;
    }
    free(cpuMaps);
    return RMRS_OK;
}

void LibvirtHelper::UpdateVmMemInfoOnNuma(ResourceExport *vmInfoHandler, VmDomainInfo &vmInfo)
{
    // 更新各numa上已被虚机申请的内存
    auto vmCpuInfo = vmInfo.cpuInfos[0];
    auto vmNumaMaxMemInfos = vmInfoHandler->GetVmNumaMaxMemInfos();
    auto vmNumaUsedMemInfos = vmInfoHandler->GetVmNumaUsedMemInfos();
    uint16_t tNumaId = vmCpuInfo.cpuNumaId;
    if (vmNumaMaxMemInfos->find(tNumaId) != vmNumaMaxMemInfos->end()) {
        (*vmNumaMaxMemInfos)[tNumaId] += vmInfo.metaData.maxMem;
        (*vmNumaUsedMemInfos)[tNumaId] += vmInfo.metaData.usedMem;
    } else {
        (*vmNumaMaxMemInfos)[tNumaId] = vmInfo.metaData.maxMem;
        (*vmNumaUsedMemInfos)[tNumaId] = vmInfo.metaData.usedMem;
    }
    vmInfo.metaData.numaId = vmCpuInfo.cpuNumaId;

    // 增加新的虚机
    auto VmDomainInfos = vmInfoHandler->GetVmDomainInfos();
    vmInfo.metaData.hostName = ResourceExport::GetHostName();
    vmInfo.metaData.nodeId = ResourceExport::GetNodeId();
    vmInfo.metaData.socketId = vmCpuInfo.socketId;
    VmDomainInfos->push_back(vmInfo);

    // 维护虚机绑定的numaId
    auto uuidNumaMap = vmInfoHandler->GetUuidNumaMap();
    (*uuidNumaMap)[vmInfo.metaData.uuid] = tNumaId;
}


void LibvirtHelper::FreeDomains(void **&domains, size_t domainNums)
{
    if (domains == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Domains ptr is already empty.";
        return;
    }

    libvirt::VirDomainFreeFunc virDomainFree = LibvirtModule::VirDomainFree();
    if (virDomainFree == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport][LibvirtHelper] VirDomainFree function is not available.";
        return;
    }

    for (size_t i = 0; i < domainNums; i++) {
        if (domains[i] != nullptr) {
            virDomainFree(domains[i]);
            domains[i] = nullptr; // 防止后续的双重释放
        } else {
            UBTURBO_LOG_WARN(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
                << "[RmrsResourceExport][LibvirtHelper] Domain at index " << i << " is already null.";
        }
    }

    free(domains);
    domains = nullptr; // 防止后续的双重释放
}


uint32_t LibvirtHelper::GetDomainList(void **&domains, int &numDomains)
{
    libvirt::VirConnectListAllDomainsFunc virConnectListAllDomains = LibvirtModule::VirConnectListAllDomains();
    if (virConnectListAllDomains == nullptr) {
        return RMRS_ERROR;
    }
    numDomains = virConnectListAllDomains(virConnect, &domains, VirConnectListAllDomainsFlags::VM_LIST_DOMAINS_ACTIVE);
    if (numDomains < 0) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Get vmDomain infos failed by libvirt. " << strerror(errno) << ".";

        if (domains != nullptr) {
            free(domains);
        }
        return RMRS_ERROR;
    }

    if (domains == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtHelper] Get vmDomain infos failed by libvirt. " << strerror(errno) << ".";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

} // namespace rmrs