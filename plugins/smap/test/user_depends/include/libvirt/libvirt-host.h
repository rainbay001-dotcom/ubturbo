/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: stub libvirt-host.h
 */

#ifndef LIBVIRT_HOST_H
# define LIBVIRT_HOST_H

# ifndef __VIR_LIBVIRT_H_INCLUDES__
#  error "Don't include this file directly, only use libvirt/libvirt.h"
# endif

typedef struct _virConnect virConnect;
typedef virConnect *virConnectPtr;

struct _virConnect {
    int refs;
};

#endif /* LIBVIRT_HOST_H */
