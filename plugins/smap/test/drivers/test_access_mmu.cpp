/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP access mmu测试代码
*/
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/hugetlb.h>
#include <linux/pagewalk.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/sched/mm.h>

#include "check.h"
#include "access_iomem.h"
#include "access_mmu.h"

#define PM_PRESENT BIT_ULL(63)
extern "C" bool drivers_is_access_hugepage(void);

using namespace std;

inline bool operator==(const pte_t& a, const pte_t& b)
{
    return a.pte == b.pte;
}

inline bool operator==(const pagemap_entry_t& a, const pagemap_entry_t& b)
{
    return a.pme == b.pme;
}

class AccessMMUTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

extern "C" pagemap_entry_t make_pme(u64 frame, u64 flags);
TEST_F(AccessMMUTest, MakePME)
{
    pagemap_entry_t entry = make_pme(0, 1);
    EXPECT_EQ(1, entry.pme);
}

extern "C" int mock_calc_paddr_acidx(u64 paddr, int *nid, u64 *index);
extern "C" bool drivers_is_paddr_local(u64 pa);
extern "C" int drivers_calc_paddr_acidx_iomem(u64 pa, int *nid, u64 *index, int page_size);
TEST_F(AccessMMUTest, CalcPaddrACidx)
{
    MOCKER(drivers_is_access_hugepage).stubs().will(returnValue(true));
    MOCKER(drivers_is_paddr_local).stubs().will(returnValue(false));
    MOCKER(drivers_calc_paddr_acidx_iomem).stubs().will(ignoreReturnValue());

    int nid = 0;
    u64 index = 1;
    mock_calc_paddr_acidx((u64)0x00005000, &nid, &index);
    EXPECT_EQ(0, nid);
    EXPECT_EQ(1, index);
}

extern "C" int calc_paddr_acidx_acpi(u64 paddr, int *nid, u64 *index, int page_size);
TEST_F(AccessMMUTest, CalcPaddrACidxTwo)
{
    int nid;
    u64 index;
    MOCKER(drivers_is_access_hugepage).stubs().will(returnValue(true));
    MOCKER(drivers_is_paddr_local).stubs().will(returnValue(true));
    MOCKER(calc_paddr_acidx_acpi).stubs().will(returnValue(0));

    nid = 0;
    index = 1;
    int ret = mock_calc_paddr_acidx((u64)0x00005000, &nid, &index);
    EXPECT_EQ(0, ret);
}

static int fake_calc_paddr_acidx(u64 paddr, int *nid, u64 *index)
{
    *nid = 0;
    *index = 1;
    return 0;
}

extern "C" int set_non_anon_bm(struct access_pid *ap, u64 acidx, u64 paddr, int nid);
extern "C" bool pfn_valid(unsigned long pfn);
extern "C" struct page *pfn_to_online_page(unsigned long pfn);
extern "C" int PageHuge(struct page *page);
extern "C" bool PageAnon(struct page *page);
TEST_F(AccessMMUTest, set_non_anon_bm)
{
    int ret = 0;
    struct page page;
    MOCKER(pfn_valid).stubs().will(returnValue(true));
    MOCKER(pfn_to_online_page).stubs().will(returnValue(&page));
    MOCKER(PageHuge).stubs().will(returnValue(1));
    MOCKER(PageAnon).stubs().will(returnValue(true));
    ret = set_non_anon_bm(nullptr, 0, 0, 0);
    EXPECT_EQ(0, ret);
}

extern "C" int add_to_bm_page(u64 paddr, struct access_pid *ap);
TEST_F(AccessMMUTest, AddToBMPage)
{
    MOCKER(mock_calc_paddr_acidx).stubs().will(invoke(fake_calc_paddr_acidx));

    int ret = 0;
    struct access_pid ap = {
        .numa_nodes = 0x11,
    };
    ap.bm_len[0] = 2;
    ap.paddr_bm[0] = (unsigned long *)malloc(sizeof(unsigned long) * 2);

    MOCKER(set_non_anon_bm).stubs().will(returnValue(0));
    ret = add_to_bm_page((u64)0x00005000, &ap);
    EXPECT_EQ(0, ret);

    free(ap.paddr_bm[0]);
}

TEST_F(AccessMMUTest, AddToBMPageTwo)
{
    int ret = 0;
    struct access_pid ap = {
        .numa_nodes = 0x11,
    };

    MOCKER(mock_calc_paddr_acidx).stubs().will(returnValue(1));
    ret = add_to_bm_page((u64)0x00005000, &ap);
    EXPECT_EQ(1, ret);

    free(ap.paddr_bm[L1]);
}

extern "C" void set_pa_prior(struct access_pid *ap, u64 vaddr, u64 pa_idx, int nid);
extern "C" int add_to_bm_hugepage(u64 vaddr, u64 paddr, struct access_pid *ap);
TEST_F(AccessMMUTest, AddToBmHugepage)
{
    MOCKER(mock_calc_paddr_acidx).stubs().will(invoke(fake_calc_paddr_acidx));

    int ret = 0;
    struct access_pid ap = {
        .numa_nodes = 0x10,
    };
    ap.bm_len[0] = 2;
    ap.paddr_bm[0] = (unsigned long *)malloc(sizeof(unsigned long) * 2);

    ret = add_to_bm_hugepage((u64)0x00005000, 0, &ap);
    EXPECT_EQ(-EINVAL, ret);

    ap.numa_nodes = 0x11;
    MOCKER(set_pa_prior).stubs();
    ret = add_to_bm_hugepage((u64)0x00005000, 0, &ap);
    EXPECT_EQ(0, ret);
}

TEST_F(AccessMMUTest, AddToBmHugepageTwo)
{
    int ret = 0;
    struct access_pid ap = {
        .numa_nodes = 0x10,
    };
    ap.bm_len[L1] = 2;
    ap.paddr_bm[L1] = (unsigned long *)malloc(sizeof(unsigned long) * 2);

    MOCKER(mock_calc_paddr_acidx).stubs().will(returnValue(1));
    ret = add_to_bm_hugepage((u64)0x00005000, 0, &ap);
    EXPECT_EQ(1, ret);
}

extern "C" int calc_vaddr_acidx(u64 vaddr, struct vm_mapping_info *info, u64 *acidx);
TEST_F(AccessMMUTest, CalcVaddrAcidx)
{
    int ret;
    u64 vaddr;
    u64 acidx;
    struct vm_mapping_info info = {
        .nr_segs = 2,
    };
    info.segs[0].start = 0;
    info.segs[0].end = info.segs[0].start + TWO_MEGA_SIZE;
    info.segs[0].hugepages = 1;

    info.segs[1].start = 0xffffffff;
    info.segs[1].end = info.segs[1].start + 2 * TWO_MEGA_SIZE;
    info.segs[1].hugepages = 2;

    vaddr = info.segs[1].start + TWO_MEGA_SIZE;
    ret = calc_vaddr_acidx(vaddr, &info, &acidx);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, acidx);

    vaddr = info.segs[1].start + 2 * TWO_MEGA_SIZE;
    ret = calc_vaddr_acidx(vaddr, &info, &acidx);
    EXPECT_EQ(-ERANGE, ret);
}

extern "C" void add_to_bm_normal(u64 paddr, struct access_pid *ap);
TEST_F(AccessMMUTest, AddToBMNormal)
{
    MOCKER(add_to_bm_page).stubs().will(ignoreReturnValue());

    struct access_pid ap = {
        .numa_nodes = 0x11,
    };
    ap.bm_len[0] = 2;
    ap.paddr_bm[0] = (unsigned long *)malloc(sizeof(unsigned long) * 2);

    add_to_bm_normal((u64)0x00005000, &ap);

    free(ap.paddr_bm[0]);
}

extern "C" bool is_paddr_belong_remote_node(u64 pa, int nid);
TEST_F(AccessMMUTest, PaddrBelongRemoteNote)
{
    struct ram_segment seg;
    int ret1 = list_empty(&remote_ram_list);
    EXPECT_EQ(1, ret1);

    seg.numa_node = 0;
    seg.start = 0x0;
    seg.end = 0xFF;

    list_add(&seg.node, &remote_ram_list);
    bool ret2 = is_paddr_belong_remote_node(0xF, 0);
    EXPECT_EQ(true, ret2);
    list_del(&seg.node);
}

TEST_F(AccessMMUTest, PaddrNotBelongRemoteNote)
{
    struct ram_segment seg;

    seg.numa_node = 0;
    seg.start = 0xF;
    seg.end = 0xFF;

    list_add(&seg.node, &remote_ram_list);
    bool ret = is_paddr_belong_remote_node(0x0, 0);
    EXPECT_EQ(false, ret);

    ret = is_paddr_belong_remote_node(0xFFF, 0);
    EXPECT_EQ(false, ret);
    list_del(&seg.node);
}

extern "C" int PageHead(struct page *page);
extern "C" bool is_page_ready_for_migrate(struct page *page);
TEST_F(AccessMMUTest, HugePageReadyForMigrate)
{
    struct page page;
    MOCKER(drivers_is_access_hugepage).stubs().will(returnValue(true));
    MOCKER(PageHuge).stubs().will(returnValue(1));
    MOCKER(PageHead).stubs().will(returnValue(1));
    bool ret = is_page_ready_for_migrate(&page);
    EXPECT_EQ(true, ret);
}

extern "C" int add_to_bm(unsigned long vaddr, pagemap_entry_t *pme, struct pagemapread *pm);
extern "C" void add_to_bm_huge(u64 vaddr, u64 paddr, struct access_pid *ap);
TEST_F(AccessMMUTest, AddToBM)
{
    MOCKER(drivers_is_access_hugepage).stubs().will(returnValue(true));
    MOCKER(add_to_bm_huge).stubs().will(ignoreReturnValue());

    int ret = 0;
    pagemap_entry_t pe {
        .pme = 0,
    };

    struct access_pid ap = {
        .numa_nodes = 0x10,
    };
    ap.bm_len[0] = 2;
    ap.paddr_bm[0] = (unsigned long *)malloc(sizeof(unsigned long) * 2);

    struct pagemapread pm = {
        .pos = 0,
        .len = 2,
        .mig_type = NORMAL_MIGRATE,
        .ap = &ap,
    };

    ret = add_to_bm(0, &pe, &pm);
    EXPECT_EQ(0, ret);

    free(ap.paddr_bm[0]);
}

TEST_F(AccessMMUTest, AddToBMTwo)
{
    pagemap_entry_t pe {
        .pme = 1,
    };
    struct access_pid ap = {
        .numa_nodes = 0x10,
    };
    struct pagemapread pm = {
        .pos = 2,
        .len = 2,
        .mig_type = NORMAL_MIGRATE,
        .ap = &ap,
    };
    MOCKER(drivers_is_access_hugepage).stubs().will(returnValue(true));
    MOCKER(add_to_bm_page).stubs();
    int ret = add_to_bm(0, &pe, &pm);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    MOCKER(drivers_is_access_hugepage).stubs().will(returnValue(false));
    MOCKER(add_to_bm_normal).stubs();
    pm.pos = 0;
    ret = add_to_bm(0, &pe, &pm);
    EXPECT_EQ(0, ret);
}

extern "C" pagemap_entry_t pte_to_pagemap_entry(struct pagemapread *pm,
    struct vm_area_struct *vma, unsigned long addr, pte_t pte);
TEST_F(AccessMMUTest, PTE2PageMapEntry)
{
    pte_t pte = {
        .pte = 0x56780000
    };

    pagemap_entry_t pe = pte_to_pagemap_entry(nullptr, nullptr, 0, pte);

    EXPECT_EQ(PM_PRESENT, pe.pme & PM_PRESENT);

    pte.pte = 0;
    pe = pte_to_pagemap_entry(nullptr, nullptr, 0, pte);
    EXPECT_EQ(0, pe.pme & PM_PRESENT);
}

extern "C" void pmd_clear_bad(pmd_t *pmd);
TEST_F(AccessMMUTest, PMDClearBas)
{
    pmd_t pmd = {
        .pmd = 1,
    };

    pmd_clear_bad(&pmd);

    EXPECT_EQ(0, pmd.pmd);
}

extern "C" spinlock_t *__pmd_trans_huge_lock(pmd_t *pmd, struct vm_area_struct *vma);
TEST_F(AccessMMUTest, PMDTransHugeLock)
{
    struct vm_area_struct vma = {
        .vm_mm = nullptr,
    };
    pmd_t pmd = {
        .pmd = 1,
    };

    spinlock_t *lock = __pmd_trans_huge_lock(&pmd, &vma);

    EXPECT_EQ(nullptr, lock);
}

extern "C" int pagemap_pte_range(pte_t *pte, unsigned long addr, unsigned long next, struct mm_walk *walk);
TEST_F(AccessMMUTest, PagemapPteRange)
{
    struct mm_walk walk;
    struct vm_area_struct vma;
    struct pagemapread pm;
    pagemap_entry_t pme;

    walk.vma = &vma;
    walk.private_data = &pm;
    MOCKER(pte_to_pagemap_entry).stubs().will(returnValue(pme));
    MOCKER(add_to_bm).stubs().will(returnValue(0));
    int ret = pagemap_pte_range(nullptr, 0, 0, &walk);
    EXPECT_EQ(0, ret);
}

extern "C" pte_t smap_huge_ptep_get(pte_t *ptep);
extern "C" int pagemap_hugetlb_range(pte_t *ptep, unsigned long hmask, unsigned long addr, unsigned long end,
                                     struct mm_walk *walk);
TEST_F(AccessMMUTest, PagemapHugetlbRange)
{
    struct mm_walk walk;
    struct vm_area_struct vma;
    struct pagemapread pm;
    pte_t pte;
    pagemap_entry_t pme;

    walk.vma = &vma;
    walk.private_data = &pm;
    vma.vm_flags = 1;
    pte.pte = 1;
    MOCKER(smap_huge_ptep_get).stubs().will(returnValue(pte));
    MOCKER(add_to_bm).stubs().will(returnValue(0));
    int ret = pagemap_hugetlb_range(nullptr, 0, 0, 0, &walk);
    EXPECT_EQ(0, ret);
}

extern "C" u32 to_hugepage_count(u32 nr);
TEST_F(AccessMMUTest, ToHugepageCount)
{
    int nr = 1;
    u32 count = to_hugepage_count(nr);

    EXPECT_EQ(1, count);
}

extern "C" struct mm_struct *mock_get_mm_by_pid(pid_t pid);
TEST_F(AccessMMUTest, GetMMByPid)
{
    struct mm_struct *mm = mock_get_mm_by_pid((pid_t)1);

    EXPECT_EQ(ERR_PTR(-ESRCH), mm);

    struct task_struct task;
    MOCKER(get_pid_task).stubs().will(returnValue(&task));
    mm = mock_get_mm_by_pid((pid_t)1);
    EXPECT_EQ(nullptr, mm);
}

extern "C" bool IS_ERR(const void *ptr);
extern "C" void pos_to_addr(struct mm_struct *mm, unsigned long pos, unsigned long *vaddr);
TEST_F(AccessMMUTest, FillAPPAddrBM)
{
    struct pagemapread pm;
    struct mm_struct mm1;
    unsigned long start_vaddr = 0;
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(mock_get_mm_by_pid).stubs().will(returnValue(&mm1));
    MOCKER(pos_to_addr).stubs().with(any(), any(), outBoundP(&start_vaddr, sizeof(start_vaddr)));
    MOCKER(walk_page_range).stubs().will(ignoreReturnValue());
    walk_pid_pagemap(&pm);
    EXPECT_EQ(16384, pm.len);
}

TEST_F(AccessMMUTest, SetPaPrior)
{
    access_pid ap;
    memset(&ap, 0, sizeof(ap));

    ap.info.vm_size = 1;

    ap.info.mapping = (u32 *)malloc(sizeof(u32) * ap.info.vm_size);
    ASSERT_NE(ap.info.mapping, nullptr);
    memset(ap.info.mapping, 0xFF, sizeof(u32) * ap.info.vm_size);

    ap.info.nr_segs = 1;
    ap.info.segs[0].start = 0x0;
    ap.info.segs[0].end = 0x200000;
    ap.info.segs[0].hugepages = 1;

    u64 vaddr = 0x0;
    u64 pa_idx = 0;
    int nid = 0;

    set_pa_prior(&ap, vaddr, pa_idx, nid);

    EXPECT_EQ(0u, ap.info.mapping[0]);

    free(ap.info.mapping);
}

TEST_F(AccessMMUTest, PosToAddr)
{
    struct vm_area_struct vma;
    struct mm_struct mm;
    unsigned long vaddr;

    MOCKER(vma_find).stubs().will(returnValue(&vma));
    vma.vm_end = 0x100;
    vma.vm_start = 0;
    pos_to_addr(&mm, 0x1, &vaddr);
}