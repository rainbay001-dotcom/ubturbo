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
#include "rmrs_libvirt_module.h"

#include <cerrno>
#include <dlfcn.h>

#include "turbo_logger.h"
#include "rmrs_error.h"
#include "rmrs_config.h"

namespace rmrs::libvirt {
using namespace turbo::log;

void *LibvirtModule::libvirtHandle = nullptr;
VirConnectOpenFunc LibvirtModule::virConnectOpenFunc = nullptr;
VirConnectCloseFunc LibvirtModule::virConnectCloseFunc = nullptr;
VirConnectListAllDomainsFunc LibvirtModule::virConnectListAllDomainsFunc = nullptr;
VirDomainGetNameFunc LibvirtModule::virDomainGetNameFunc = nullptr;
VirDomainGetUUIDStringFunc LibvirtModule::virDomainGetUUIDStringFunc = nullptr;
VirDomainGetInfoFunc LibvirtModule::virDomainGetInfoFunc = nullptr;
VirDomainGetVcpusFunc LibvirtModule::virDomainGetVcpusFunc = nullptr;
VirDomainGetIDFunc LibvirtModule::virDomainGetIDFunc = nullptr;
VirConnectGetHostnameFunc LibvirtModule::virConnectGetHostnameFunc = nullptr;
VirDomainFreeFunc LibvirtModule::virDomainFreeFunc = nullptr;
VirConnectIsAliveFunc LibvirtModule::virConnectIsAliveFunc = nullptr;
VirEventRegisterDefaultImplFunc LibvirtModule::virEventRegisterDefaultImplFunc = nullptr;
VirEventRunDefaultImplFunc LibvirtModule::virEventRunDefaultImplFunc = nullptr;
VirConnectDomainEventRegisterFunc LibvirtModule::virConnectDomainEventRegisterFunc = nullptr;
VirConnectDomainEventDeRegisterFunc LibvirtModule::virConnectDomainEventDeRegisterFunc = nullptr;

RmrsResult LibvirtModule::Init()
{
    libvirtHandle = dlopen("libvirt.so.0", RTLD_LAZY);
    if (libvirtHandle == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Load libvirt.so.0 failed. " << strerror(errno) << ".";
        return RMRS_ERROR;
    }
    return RMRS_OK;
}

VirConnectOpenFunc LibvirtModule::VirConnectOpen()
{
    if (virConnectOpenFunc != nullptr) {
        return virConnectOpenFunc;
    }
    virConnectOpenFunc = (VirConnectOpenFunc)(dlsym(libvirtHandle, "virConnectOpen"));
    if (virConnectOpenFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirConnectOpen ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectOpenFunc;
}

VirConnectCloseFunc LibvirtModule::VirConnectClose()
{
    if (virConnectCloseFunc != nullptr) {
        return virConnectCloseFunc;
    }
    virConnectCloseFunc = (VirConnectCloseFunc)(dlsym(libvirtHandle, "virConnectClose"));
    if (virConnectCloseFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirConnectListAllDomains ptr failed. " << strerror(errno)
            << ".";
        return nullptr;
    }
    return virConnectCloseFunc;
}

VirConnectListAllDomainsFunc LibvirtModule::VirConnectListAllDomains()
{
    if (virConnectListAllDomainsFunc != nullptr) {
        return virConnectListAllDomainsFunc;
    }
    virConnectListAllDomainsFunc = (VirConnectListAllDomainsFunc)(dlsym(libvirtHandle, "virConnectListAllDomains"));
    if (virConnectListAllDomainsFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirConnectListAllDomains ptr failed. " << strerror(errno)
            << ".";
        return nullptr;
    }
    return virConnectListAllDomainsFunc;
}

VirDomainGetNameFunc LibvirtModule::VirDomainGetName()
{
    if (virDomainGetNameFunc != nullptr) {
        return virDomainGetNameFunc;
    }
    virDomainGetNameFunc = (VirDomainGetNameFunc)(dlsym(libvirtHandle, "virDomainGetName"));
    if (virDomainGetNameFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirDomainGetName ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetNameFunc;
}

VirDomainGetIDFunc LibvirtModule::VirDomainGetID()
{
    if (virDomainGetIDFunc != nullptr) {
        return virDomainGetIDFunc;
    }
    virDomainGetIDFunc = (VirDomainGetIDFunc)(dlsym(libvirtHandle, "virDomainGetID"));
    if (virDomainGetIDFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirDomainGetID ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetIDFunc;
}

VirDomainGetUUIDStringFunc LibvirtModule::VirDomainGetUUIDString()
{
    if (virDomainGetUUIDStringFunc != nullptr) {
        return virDomainGetUUIDStringFunc;
    }
    virDomainGetUUIDStringFunc = (VirDomainGetUUIDStringFunc)(dlsym(libvirtHandle, "virDomainGetUUIDString"));
    if (virDomainGetUUIDStringFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get virDomainGetUUIDString ptr failed. "
            << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetUUIDStringFunc;
}

VirDomainGetInfoFunc LibvirtModule::VirDomainGetInfo()
{
    if (virDomainGetInfoFunc != nullptr) {
        return virDomainGetInfoFunc;
    }
    virDomainGetInfoFunc = (VirDomainGetInfoFunc)(dlsym(libvirtHandle, "virDomainGetInfo"));
    if (virDomainGetInfoFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirDomainGetInfo ptr failed, errno " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetInfoFunc;
}

VirDomainGetVcpusFunc LibvirtModule::VirDomainGetVcpus()
{
    if (virDomainGetVcpusFunc != nullptr) {
        return virDomainGetVcpusFunc;
    }
    virDomainGetVcpusFunc = (VirDomainGetVcpusFunc)(dlsym(libvirtHandle, "virDomainGetVcpus"));
    if (virDomainGetVcpusFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirDomainGetVcpus ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainGetVcpusFunc;
}

VirConnectGetHostnameFunc LibvirtModule::VirConnectGetHostname()
{
    if (virConnectGetHostnameFunc != nullptr) {
        return virConnectGetHostnameFunc;
    }
    virConnectGetHostnameFunc = (VirConnectGetHostnameFunc)(dlsym(libvirtHandle, "virConnectGetHostname"));
    if (virConnectGetHostnameFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirConnectGetHostname ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectGetHostnameFunc;
}

VirDomainFreeFunc LibvirtModule::VirDomainFree()
{
    if (virDomainFreeFunc != nullptr) {
        return virDomainFreeFunc;
    }
    virDomainFreeFunc = (VirDomainFreeFunc)(dlsym(libvirtHandle, "virDomainFree"));
    if (virDomainFreeFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirDomainFree ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virDomainFreeFunc;
}

VirConnectIsAliveFunc LibvirtModule::VirConnectIsAlive()
{
    if (virConnectIsAliveFunc != nullptr) {
        return virConnectIsAliveFunc;
    }
    virConnectIsAliveFunc = (VirConnectIsAliveFunc)(dlsym(libvirtHandle, "virConnectIsAlive"));
    if (virConnectIsAliveFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get VirConnectIsAlive ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectIsAliveFunc;
}

VirEventRegisterDefaultImplFunc LibvirtModule::VirEventRegisterDefaultImpl()
{
    if (virEventRegisterDefaultImplFunc != nullptr) {
        return virEventRegisterDefaultImplFunc;
    }
    virEventRegisterDefaultImplFunc =
        (VirEventRegisterDefaultImplFunc)(dlsym(libvirtHandle, "virEventRegisterDefaultImpl"));
    if (virEventRegisterDefaultImplFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get virEventRegisterDefaultImpl ptr failed. "
            << strerror(errno) << ".";
        return nullptr;
    }
    return virEventRegisterDefaultImplFunc;
}

VirEventRunDefaultImplFunc LibvirtModule::VirEventRunDefaultImpl()
{
    if (virEventRunDefaultImplFunc != nullptr) {
        return virEventRunDefaultImplFunc;
    }
    virEventRunDefaultImplFunc =
        (VirEventRunDefaultImplFunc)(dlsym(libvirtHandle, "virEventRunDefaultImpl"));
    if (virEventRunDefaultImplFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get virEventRunDefaultImpl ptr failed. " << strerror(errno) << ".";
        return nullptr;
    }
    return virEventRunDefaultImplFunc;
}

VirConnectDomainEventRegisterFunc LibvirtModule::VirConnectDomainEventRegister()
{
    if (virConnectDomainEventRegisterFunc != nullptr) {
        return virConnectDomainEventRegisterFunc;
    }
    virConnectDomainEventRegisterFunc =
        (VirConnectDomainEventRegisterFunc)(dlsym(libvirtHandle, "virConnectDomainEventRegister"));
    if (virConnectDomainEventRegisterFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get virConnectDomainEventRegister ptr failed. "
            << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectDomainEventRegisterFunc;
}

VirConnectDomainEventDeRegisterFunc LibvirtModule::VirConnectDomainEventDeRegister()
{
    if (virConnectDomainEventDeRegisterFunc != nullptr) {
        return virConnectDomainEventDeRegisterFunc;
    }
    virConnectDomainEventDeRegisterFunc =
        (VirConnectDomainEventDeRegisterFunc)(dlsym(libvirtHandle, "virConnectDomainEventDeRegister"));
    if (virConnectDomainEventDeRegisterFunc == nullptr) {
        UBTURBO_LOG_ERROR(RMRS_MODULE_NAME, RMRS_MODULE_CODE)
            << "[RmrsResourceExport] [LibvirtModule] Get virConnectDomainEventDeRegister ptr failed. "
            << strerror(errno) << ".";
        return nullptr;
    }
    return virConnectDomainEventDeRegisterFunc;
}
} // namespace rmrs::libvirt