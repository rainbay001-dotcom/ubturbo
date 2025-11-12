/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: SMAP5.0 内存地址模块测试代码
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <asm/kvm_pgtable.h>
#include <linux/list.h>
#include <linux/dmi.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "kvm_pgtable.h"

using namespace std;

struct stage2_attr_data {
    kvm_pte_t attr_set;
    kvm_pte_t attr_clr;
    kvm_pte_t pte;
    u32 level;
};

struct kvm_pgtable_walk_data {
    struct kvm_pgtable *pgt;
    struct kvm_pgtable_walker *walker;
    u64 addr;
    u64 end;
};

class KvmPgTableTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::cout << "[Phase SetUp Begin]" << std::endl;
        std::cout << "[Phase SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[Phase TearDown Begin]" << std::endl;
        GlobalMockObject::reset();
        std::cout << "[Phase TearDown End]" << std::endl;
    }
};

extern "C" bool kvm_pte_table(kvm_pte_t pte, u32 level);
TEST_F(KvmPgTableTest, PteTableTest)
{
    kvm_pte_t pte = 0;
    u32 level = 3;
    bool ret = kvm_pte_table(pte, level);
    EXPECT_EQ(false, ret);
}

TEST_F(KvmPgTableTest, PteTableTestTwo)
{
    kvm_pte_t pte = 1;
    u32 level = 0;
    bool ret = kvm_pte_table(pte, level);
    EXPECT_EQ(false, ret);
}

static void* stubFunc(phys_addr_t phys)
{
    return NULL;
}

extern "C" kvm_pte_t *kvm_pte_follow(kvm_pte_t pte, struct kvm_pgtable_mm_ops *mm_ops);
TEST_F(KvmPgTableTest, PteFollowTest)
{
    struct kvm_pgtable_mm_ops mm_ops = {
        .phys_to_virt = stubFunc,
    };
    kvm_pte_t pte = 0;
    kvm_pte_t *ret = kvm_pte_follow(pte, &mm_ops);
    EXPECT_EQ(nullptr, ret);
}

static int my_callback_function(const struct kvm_pgtable_visit_ctx *ctx,
    enum kvm_pgtable_walk_flags visit)
{
    return 0;
}
extern "C" int kvm_pgtable_visitor_cb(struct kvm_pgtable_walk_data *data,
    const struct kvm_pgtable_visit_ctx *ctx, enum kvm_pgtable_walk_flags visit);
extern "C" u32 kvm_pgtable_idx(struct kvm_pgtable_walk_data *data, u32 level);
TEST_F(KvmPgTableTest, PgtableIdxTest)
{
    struct kvm_pgtable_walk_data data = {
        .addr = 0x10000,
    };
    u32 level = 0;
    int ret = kvm_pgtable_idx(&data, level);
    EXPECT_EQ(0, ret);
}

extern "C" int __kvm_pgtable_visit(struct kvm_pgtable_walk_data *data,
    struct kvm_pgtable_mm_ops *mm_ops, kvm_pteref_t pteref, u32 level);
extern "C" int __kvm_pgtable_walk(struct kvm_pgtable_walk_data *data,
    struct kvm_pgtable_mm_ops *mm_ops, kvm_pteref_t pgtable, u32 level);
TEST_F(KvmPgTableTest, PgtableWalkTest)
{
    struct kvm_pgtable_walk_data data;
    struct kvm_pgtable_mm_ops mm_ops;
    u64 *arry = (u64 *)malloc(sizeof(u64));
    kvm_pteref_t pgtable = arry;
    u32 level = 0;
    MOCKER(kvm_pgtable_idx).stubs().will(returnValue(0));
    int ret = __kvm_pgtable_walk(&data, &mm_ops, pgtable, level);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" u32 kvm_pgd_page_idx(struct kvm_pgtable *pgt, u64 addr);
TEST_F(KvmPgTableTest, PgdPageIdxTest)
{
    struct kvm_pgtable pgt = {
        .ia_bits = 1,
        .start_level = 0,
    };
    u64 addr = 0;
    u64 num = 0;
    int ret = kvm_pgd_page_idx(&pgt, addr);
    EXPECT_EQ(0, ret);
}

extern "C" int _kvm_pgtable_walk(struct kvm_pgtable *pgt, struct kvm_pgtable_walk_data *data);
TEST_F(KvmPgTableTest, KvmPgtableWalkTest)
{
    struct kvm_pgtable pgt = {
        .ia_bits = 0x200000,
        .pgd = nullptr,
    };
    struct kvm_pgtable_walk_data data = {
        .pgt = &pgt,
        .addr = 0x1,
        .end = 0x20,
    };
    int ret = _kvm_pgtable_walk(&pgt, &data);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(KvmPgTableTest, KvmPgtableWalkTestTwo)
{
    struct kvm_pgtable pgt = {
        .ia_bits = 0x200000,
    };
    pgt.pgd = (u64 *)malloc(sizeof(u64));
    struct kvm_pgtable_walk_data data = {
        .pgt = &pgt,
        .addr = 0x1,
        .end = 0x20,
    };
    MOCKER(kvm_pgtable_idx).stubs().will(returnValue(0));
    MOCKER(__kvm_pgtable_walk).stubs().will(returnValue(-EINVAL));
    int ret = _kvm_pgtable_walk(&pgt, &data);
    EXPECT_EQ(-EINVAL, ret);
    free(pgt.pgd);
}

extern "C" int kvm_pgtable_walk(struct kvm_pgtable *pgt, u64 addr, u64 size,
                                struct kvm_pgtable_walker *walker);
TEST_F(KvmPgTableTest, WalkTest)
{
    struct kvm_pgtable pgt;
    struct kvm_pgtable_walker walker = {
        .flags = KVM_PGTABLE_WALK_TABLE_POST,
    };
    u64 addr = 0;
    u64 size = 1;
    MOCKER(_kvm_pgtable_walk).stubs().will(returnValue(0));
    int ret = kvm_pgtable_walk(&pgt, addr, size, &walker);
    EXPECT_EQ(0, ret);
}

extern "C" bool stage2_try_set_pte(const struct kvm_pgtable_visit_ctx *ctx, kvm_pte_t new_pte);
TEST_F(KvmPgTableTest, stage2_try_set_pte)
{
    struct kvm_pgtable_visit_ctx ctx;
    kvm_pte_t pte = 0;
    kvm_pte_t ptep = 0;
    ctx.ptep = &ptep;
    bool ret = stage2_try_set_pte(&ctx, pte);
    EXPECT_EQ(true, ret);
}

extern "C" bool smap_kvm_pgtable_stage2_test_clear_young(struct kvm_pgtable *pgt, u64 addr, u64 size, bool mkold);
TEST_F(KvmPgTableTest, ClearYoungTest)
{
    struct kvm_pgtable pgt;
    u64 addr = 0;
    u64 size = 1;
    bool ret = smap_kvm_pgtable_stage2_test_clear_young(&pgt, addr, size, true);
    EXPECT_EQ(false, ret);
}

extern "C" bool smap_kvm_pgtable_stage2_is_young(struct kvm_pgtable *pgt, u64 addr);
TEST_F(KvmPgTableTest, YoungTest)
{
    u64 addr = 0;
    struct kvm_pgtable pgt;
    MOCKER(smap_kvm_pgtable_stage2_test_clear_young).stubs().will(returnValue(false));
    bool ret = smap_kvm_pgtable_stage2_is_young(&pgt, addr);
    EXPECT_EQ(false, ret);
}

extern "C" bool smap_kvm_pgtable_stage2_mkold(struct kvm_pgtable *pgt, u64 addr);
TEST_F(KvmPgTableTest, MkOldTest)
{
    u64 addr = 0;
    struct kvm_pgtable pgt;
    MOCKER(smap_kvm_pgtable_stage2_test_clear_young).stubs().will(returnValue(true));
    bool ret = smap_kvm_pgtable_stage2_mkold(&pgt, addr);
    EXPECT_EQ(true, ret);
}

extern "C" bool kvm_pgtable_walk_continue(const struct kvm_pgtable_walker *walker, int r);
TEST_F(KvmPgTableTest, WalkContinueTest)
{
    struct kvm_pgtable_walker walker = {
        .flags = KVM_PGTABLE_WALK_TABLE_POST,
    };
    int r = 0;
    bool ret = kvm_pgtable_walk_continue(&walker, r);
    EXPECT_EQ(true, ret);

    r = -EAGAIN;
    ret = kvm_pgtable_walk_continue(&walker, r);
    EXPECT_EQ(true, ret);
}
