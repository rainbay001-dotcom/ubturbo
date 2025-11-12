/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: stub libvirt-domain.h
 */

#ifndef LIBVIRT_DOMAIN_H
# define LIBVIRT_DOMAIN_H

# ifndef __VIR_LIBVIRT_H_INCLUDES__
#  error "Don't include this file directly, only use libvirt/libvirt.h"
# endif

typedef struct _virDomain virDomain;
typedef virDomain *virDomainPtr;
struct _virDomain {
    int refs;
};

struct _virDomainInfo {
    unsigned char state;
    unsigned long maxMem;
    unsigned long memory;
    unsigned short nrVirtCpu;
    unsigned long long cpuTime;
};
typedef struct _virDomainInfo virDomainInfo;

typedef virDomainInfo *virDomainInfoPtr;

typedef struct _virVcpuInfo virVcpuInfo;
struct _virVcpuInfo {
    unsigned int number;
    int state;
    unsigned long long cpuTime;
    int cpu;
};
typedef virVcpuInfo *virVcpuInfoPtr;

#endif /* LIBVIRT_DOMAIN_H */
