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
#ifndef __NUMA_NODES_H__
#define __NUMA_NODES_H__

#define NUMA_NO_NODE (-1)

#define LOCAL_NUMA_BITS 4
#define REMOTE_NUMA_BITS 18
#define MAX_NODES (LOCAL_NUMA_BITS + REMOTE_NUMA_BITS)

#define LOCAL_NUMA_SHIFT 0
#define REMOTE_NUMA_SHIFT (LOCAL_NUMA_SHIFT + LOCAL_NUMA_BITS)

#define LOCAL_NUMA_MASK ((~(1UL << LOCAL_NUMA_BITS)) << LOCAL_NUMA_SHIFT)
#define REMOTE_NUMA_MASK ((~(1UL << REMOTE_NUMA_BITS)) << REMOTE_NUMA_SHIFT)

#define BITS_PER_LONG 64

static inline int TestBit(int nr, const volatile unsigned long *addr)
{
    return 1UL & (addr[nr / BITS_PER_LONG] >> (nr & (BITS_PER_LONG - 1)));
}

static inline int GetL1(uint32_t nodes)
{
    int nid = __builtin_ffs(nodes) - 1;
    return (nid >= 0 && nid < LOCAL_NUMA_BITS) ? nid : NUMA_NO_NODE;
}

static inline void ClearL1(uint32_t *nodes)
{
    *nodes &= ~LOCAL_NUMA_MASK;
}

static inline void SetL1(uint32_t *nodes, int nid)
{
    ClearL1(nodes);
    *nodes |= (1 << nid);
}

static inline void AddL1(uint32_t *nodes, int nid)
{
    *nodes |= (1 << nid);
}

static inline bool EqualToL1(uint32_t nodes, int nid)
{
    return nid >= 0 && nid < LOCAL_NUMA_BITS && (GetL1(nodes) == nid);
}

static inline bool NotEqualToL1(uint32_t nodes, int nid)
{
    return !EqualToL1(nodes, nid);
}

static inline bool InL1(uint32_t nodes, int nid)
{
    unsigned long bitmap = nodes;
    return nid >= 0 && nid < LOCAL_NUMA_BITS && !!TestBit(nid, &bitmap);
}

static inline int GetL2(uint32_t nodes)
{
    int nid = __builtin_ffs(nodes >> REMOTE_NUMA_SHIFT) - 1;
    if (nid >= 0 && nid < REMOTE_NUMA_BITS) {
        return nid + LOCAL_NUMA_BITS;
    }
    return NUMA_NO_NODE;
}

static inline void ClearL2(uint32_t *nodes)
{
    *nodes &= ~REMOTE_NUMA_MASK;
}

static inline void SetL2(uint32_t *nodes, int pos)
{
    ClearL2(nodes);
    *nodes |= (1 << pos);
}

static inline bool EqualToL2(uint32_t nodes, int pos)
{
    return pos >= LOCAL_NUMA_BITS && pos < MAX_NODES && (GetL2(nodes) == pos);
}

static inline bool NotEqualToL2(uint32_t nodes, int pos)
{
    return !EqualToL2(nodes, pos);
}

static inline bool InL2(uint32_t nodes, int pos)
{
    unsigned long bitmap = nodes;
    return pos >= LOCAL_NUMA_BITS && pos < MAX_NODES && !!TestBit(pos, &bitmap);
}

#endif /* __NUMA_NODES_H__ */
