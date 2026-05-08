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
/*
 * NUMA 节点 bitmap 编码说明：
 * 使用一个 uint32_t 同时表示本地（L1）和远端（L2）NUMA 节点集合。
 *   - bit[0..LOCAL_NUMA_BITS-1]  对应本地 NUMA（L1），最多 4 个本地节点
 *   - bit[LOCAL_NUMA_BITS..MAX_NODES-1] 对应远端 NUMA（L2），最多 18 个远端节点
 * GetL1/SetL1/AddL1 等函数操作本地 NUMA 域；
 * GetL2/SetL2/AddL2 等函数操作远端 NUMA 域。
 */
#ifndef __NUMA_NODES_H__
#define __NUMA_NODES_H__

#define NUMA_NO_NODE (-1)            /* 无效 NUMA 节点标识 */

#define LOCAL_NUMA_BITS 4            /* 本地 NUMA 节点最大数量 */
#define REMOTE_NUMA_BITS 18          /* 远端 NUMA 节点最大数量 */
#define MAX_NODES (LOCAL_NUMA_BITS + REMOTE_NUMA_BITS) /* NUMA 节点总上限 */

#define LOCAL_NUMA_SHIFT 0           /* 本地 NUMA 在 bitmap 中的起始位偏移 */
#define REMOTE_NUMA_SHIFT (LOCAL_NUMA_SHIFT + LOCAL_NUMA_BITS) /* 远端 NUMA 在 bitmap 中的起始位偏移 */

#define LOCAL_NUMA_MASK ((~(1UL << LOCAL_NUMA_BITS)) << LOCAL_NUMA_SHIFT)   /* 本地 NUMA 位域掩码 */
#define REMOTE_NUMA_MASK ((~(1UL << REMOTE_NUMA_BITS)) << REMOTE_NUMA_SHIFT) /* 远端 NUMA 位域掩码 */

#define BITS_PER_LONG 64             /* long 类型位宽 */

/* 测试位图 addr 中第 nr 位是否置位，返回 0 或 1 */
static inline int TestBit(int nr, const volatile unsigned long *addr)
{
    return 1UL & (addr[nr / BITS_PER_LONG] >> (nr & (BITS_PER_LONG - 1)));
}

/* 获取 nodes 中第一个置位的本地 NUMA（L1）ID，无效时返回 NUMA_NO_NODE */
static inline int GetL1(uint32_t nodes)
{
    int nid = __builtin_ffs(nodes) - 1;
    return (nid >= 0 && nid < LOCAL_NUMA_BITS) ? nid : NUMA_NO_NODE;
}

/* 清除 nodes 中所有本地 NUMA（L1）位 */
static inline void ClearL1(uint32_t *nodes)
{
    *nodes &= ~LOCAL_NUMA_MASK;
}

/* 将本地 NUMA 设为唯一节点 nid（先清除再置位） */
static inline void SetL1(uint32_t *nodes, int nid)
{
    ClearL1(nodes);
    *nodes |= (1 << nid);
}

/* 在 nodes 中追加本地 NUMA nid（不清除已有位） */
static inline void AddL1(uint32_t *nodes, int nid)
{
    *nodes |= (1 << nid);
}

/* 判断 nodes 的唯一本地 NUMA 是否等于 nid */
static inline bool EqualToL1(uint32_t nodes, int nid)
{
    return nid >= 0 && nid < LOCAL_NUMA_BITS && (GetL1(nodes) == nid);
}

/* 判断 nodes 的唯一本地 NUMA 是否不等于 nid */
static inline bool NotEqualToL1(uint32_t nodes, int nid)
{
    return !EqualToL1(nodes, nid);
}

/* 判断本地 NUMA nid 是否在 nodes 集合中 */
static inline bool InL1(uint32_t nodes, int nid)
{
    unsigned long bitmap = nodes;
    return nid >= 0 && nid < LOCAL_NUMA_BITS && !!TestBit(nid, &bitmap);
}

/* 获取 nodes 中第一个置位的远端 NUMA（L2）ID（绝对编号），无效时返回 NUMA_NO_NODE */
static inline int GetL2(uint32_t nodes)
{
    int nid = __builtin_ffs(nodes >> REMOTE_NUMA_SHIFT) - 1;
    if (nid >= 0 && nid < REMOTE_NUMA_BITS) {
        return nid + LOCAL_NUMA_BITS;
    }
    return NUMA_NO_NODE;
}

/* 清除 nodes 中所有远端 NUMA（L2）位 */
static inline void ClearL2(uint32_t *nodes)
{
    *nodes &= ~REMOTE_NUMA_MASK;
}

/* 清除 nodes 中指定位位置 pos 的 bit */
static inline void ClearNodeBit(uint32_t *nodes, int pos)
{
    *nodes &= ~(1 << pos);
}

/* 将远端 NUMA 设为唯一节点 pos（先清除再置位，pos 为绝对位位置） */
static inline void SetL2(uint32_t *nodes, int pos)
{
    ClearL2(nodes);
    *nodes |= (1 << pos);
}

/* 在 nodes 中追加远端 NUMA pos（不清除已有位） */
static inline void AddL2(uint32_t *nodes, int pos)
{
    *nodes |= (1 << pos);
}

/* 判断 nodes 的唯一远端 NUMA 是否等于 pos */
static inline bool EqualToL2(uint32_t nodes, int pos)
{
    return pos >= LOCAL_NUMA_BITS && pos < MAX_NODES && (GetL2(nodes) == pos);
}

/* 判断 nodes 的唯一远端 NUMA 是否不等于 pos */
static inline bool NotEqualToL2(uint32_t nodes, int pos)
{
    return !EqualToL2(nodes, pos);
}

/* 判断远端 NUMA pos 是否在 nodes 集合中 */
static inline bool InL2(uint32_t nodes, int pos)
{
    unsigned long bitmap = nodes;
    return pos >= LOCAL_NUMA_BITS && pos < MAX_NODES && !!TestBit(pos, &bitmap);
}

/* 统计 nodes 中已置位的本地 NUMA（L1）数量 */
static inline int GetL1Count(uint32_t nodes)
{
    int count = 0;
    for (int i = 0; i < LOCAL_NUMA_BITS; i++) {
        if (nodes & (1U << i)) {
            count++;
        }
    }
    return count;
}

/* 统计 nodes 中已置位的远端 NUMA（L2）数量 */
static inline int GetL2Count(uint32_t nodes)
{
    int count = 0;
    for (int i = LOCAL_NUMA_BITS; i < MAX_NODES; i++) {
        if (nodes & (1U << i)) {
            count++;
        }
    }
    return count;
}

#endif /* __NUMA_NODES_H__ */
