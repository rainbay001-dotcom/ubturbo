/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP Tiering Memory Solution: kvm interface
 */

#ifndef _KVM_PGTABLE_H
#define _KVM_PGTABLE_H

#include <asm/kvm_pgtable.h>
#include <linux/kvm_host.h>

typedef u64 kvm_pte_t;
bool smap_kvm_pgtable_stage2_mkold(struct kvm_pgtable *pgt, u64 addr);

bool smap_kvm_pgtable_stage2_is_young(struct kvm_pgtable *pgt, u64 addr);

#endif /* _KVM_PGTABLE_H */
