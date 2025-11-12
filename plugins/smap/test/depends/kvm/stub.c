// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: kvm stub
*/

#include <linux/srcutree.h>
#include <linux/kvm_host.h>

struct kvm_memslots *kvm_memslots(struct kvm *kvm)
{
    return NULL;
}

unsigned long gfn_to_hva_memslot(struct kvm_memory_slot *slot, gfn_t gfn)
{
    return 0;
}

int srcu_read_lock(struct srcu_struct *ssp)
{
    return 0;
}

void srcu_read_unlock(struct srcu_struct *ssp, int idx)
{
}
