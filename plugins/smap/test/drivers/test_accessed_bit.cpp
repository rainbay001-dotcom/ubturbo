/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: SMAP3.0 accessed_bit测试代码
*/

#include <random>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <asm/pgtable.h>

#include <linux/spinlock.h>
#include <linux/hugetlb.h>
#include <linux/list.h>

#include <linux/mmap_lock.h>
#include <linux/errno.h>
#include <linux/pid.h>
#include <linux/mm_types.h>
#include <linux/kvm_host.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/pagewalk.h>

#include "kvm_pgtable.h"
#include "access_pid.h"
#include "accessed_bit.h"
#include "access_tracking_wrapper.h"
#include "access_iomem.h"

using namespace std;

inline bool operator==(const pte_t& a, const pte_t& b)
{
    return a.pte == b.pte;
}

struct smap_vma_struct {
    unsigned long start_vaddr;
    unsigned long end_vaddr;
};

extern "C" unsigned long huge_page_size(struct hstate *h);
extern "C" int get_numa_range(int nid, u64 *pa_start, u64 *pa_end);
extern "C" struct mem_info acpi_mem;
;

class AccessedBitTest : public ::testing::Test {
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

extern "C" void free_mem(struct acpi_mem_segment *mem);
TEST_F(AccessedBitTest, free_mem)
{
    free_mem(nullptr);
}

extern "C" struct page *pfn_to_online_page(unsigned long pfn);
extern "C" bool is_kvm_file(struct file *filp);
TEST_F(AccessedBitTest, is_kvm_file)
{
    bool ret;
    struct file_system_type anonFsType = {
        .name = "anon_inodefs",
        .fs_flags = 0
    };
    struct file_system_type normalFsType = {
        .name = "inodefs",
        .fs_flags = 0
    };
    struct super_block sb = {
        .s_type = &anonFsType
    };
    struct inode inode = {
        .i_sb = &sb
    };
    struct dentry dentry = {
        .d_name = { .name = "kvm-vm" }
    };
    struct file file = {
        .f_path = { .dentry = &dentry },
        .f_inode = &inode
    };
    ret = is_kvm_file(&file);
    EXPECT_TRUE(ret);

    sb.s_type = &normalFsType;
    ret = is_kvm_file(&file);
    EXPECT_FALSE(ret);
}

extern "C" struct file *get_kvm_file_from_task(struct task_struct *task);
TEST_F(AccessedBitTest, get_kvm_file_from_task)
{
    struct file *ret;
    struct task_struct task = {
        .files = nullptr
    };
    ret = get_kvm_file_from_task(nullptr);
    EXPECT_EQ(nullptr, ret);

    ret = get_kvm_file_from_task(&task);
    EXPECT_EQ(nullptr, ret);
}

extern "C" struct fdtable *files_fdtable(struct files_struct *files);
extern "C" bool get_file_rcu(struct file *filp);
extern "C" struct file *files_lookup_fd_rcu(struct files_struct *files, unsigned int fd);
TEST_F(AccessedBitTest, get_kvm_file_from_task_two)
{
    struct file *ret;
    struct file file;
    struct fdtable fdtable = {
        .max_fds = 0
    };
    struct files_struct fs;
    struct task_struct task = {
        .files = &fs
    };

    MOCKER(files_fdtable).stubs().will(returnValue(&fdtable));
    ret = get_kvm_file_from_task(&task);
    EXPECT_EQ(nullptr, ret);

    fdtable.max_fds = 1;
    MOCKER(files_lookup_fd_rcu).stubs().will(returnValue((&file)));
    MOCKER(get_file_rcu).stubs().will(returnValue((true)));
    MOCKER(is_kvm_file).stubs().will(returnValue((true)));
    ret = get_kvm_file_from_task(&task);
    EXPECT_NE(nullptr, ret);
}

TEST_F(AccessedBitTest, get_kvm_file_from_task_three)
{
    struct file *ret;
    struct file file;
    struct fdtable fdtable = {
        .max_fds = 0
    };
    struct files_struct fs;
    struct task_struct task = {
        .files = &fs
    };

    MOCKER(files_fdtable).stubs().will(returnValue(&fdtable));
    fdtable.max_fds = 1;

    MOCKER(files_lookup_fd_rcu).stubs().will(returnValue((&file)));
    MOCKER(get_file_rcu).stubs().will(returnValue((false)));
    ret = get_kvm_file_from_task(&task);
    EXPECT_EQ(nullptr, ret);

    MOCKER(files_lookup_fd_rcu).stubs().will(returnValue((&file)));
    MOCKER(get_file_rcu).stubs().will(returnValue((true)));
    MOCKER(is_kvm_file).stubs().will(returnValue((false)));
    ret = get_kvm_file_from_task(&task);
    EXPECT_EQ(nullptr, ret);
}

extern "C" int get_pid_from_tracking_file(const struct file *file);
TEST_F(AccessedBitTest, GetPidFromTrackingFile)
{
    struct file file;
    struct dentry tempDentry;
    struct dentry dparent;

    file.f_path.dentry = &tempDentry;
    dparent.d_name.name = "123_t";
    tempDentry.d_parent = &tempDentry;
    MOCKER(strncpy).stubs().will(ignoreReturnValue());
    MOCKER(kstrtoint).stubs().will(returnValue(-1));
    int ret = get_pid_from_tracking_file(&file);
    EXPECT_EQ(-1, ret);
}

extern "C" ssize_t proc_file_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
TEST_F(AccessedBitTest, ProcFileRead)
{
    char buf[10];
    struct file file;
    struct ham_tracking_info info;
    loff_t pos = 0;

    info.pid = 1;
    info.len[0] = 0;
    info.len[1] = 0;
    list_add(&info.node, &ham_pid_list);
    MOCKER(get_pid_from_tracking_file).stubs().will(returnValue(1));
    MOCKER(copy_to_user).stubs().will(returnValue(0UL));
    int ret = proc_file_read(&file, buf, 0, &pos);
    EXPECT_EQ(sizeof(uint64_t), ret);
    list_del(&info.node);
}

TEST_F(AccessedBitTest, SmapCreatTrackingInfoFile)
{
    struct ham_tracking_info info;
    struct proc_dir_entry proc_dir;
    struct proc_dir_entry proc_file;

    info.pid = 1;
    MOCKER(proc_mkdir).stubs().will(returnValue(&proc_dir));
    MOCKER(proc_create).stubs().will(returnValue(&proc_file));
    int ret = smap_create_tracking_info_file(&info);
    EXPECT_EQ(0, ret);
}

extern "C" int scan_forward_2M(pid_t pid, int page_size, scan_type type);
extern "C" int scan_accessed_bit_forward_vm(pid_t pid, int page_size, scan_type type);
TEST_F(AccessedBitTest, scan_accessed_bit_forward)
{
    int ret;
    int page_size = PAGE_SIZE_4K;
    ret = scan_accessed_bit_forward_vm(1, page_size, NORMAL_SCAN);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" void clear_tracking_info(struct ham_tracking_info *info);
TEST_F(AccessedBitTest, ClearTrackingInfo)
{
    struct ham_tracking_info info;
    u64 paddr1;
    u64 paddr2;
    u16 freq1;
    u16 freq2;
    info.paddr[L1] = &paddr1;
    info.paddr[L2] = &paddr2;
    info.freq[L1] = &freq1;
    info.freq[L2] = &freq2;
    info.len[L1] = info.len[L2] = 1;
    clear_tracking_info(&info);
    EXPECT_EQ(0, paddr1);
}

extern "C" void vfree(void *ptr);
extern "C" struct freq_info *get_freq_info(struct ham_tracking_info *info, uint64_t *freq_info_num);
TEST_F(AccessedBitTest, GetFreqInfo)
{
    uint64_t n;
    struct freq_info *freq_info_array;
    struct ham_tracking_info info;
    info.l2_node = 1;
    u64 paddr1;
    u64 paddr2;
    u16 freq1 = 1;
    u16 freq2 = 1;
    info.paddr[L1] = &paddr1;
    info.paddr[L2] = &paddr2;
    info.freq[L1] = &freq1;
    info.freq[L2] = &freq2;
    info.len[L1] = info.len[L2] = 1;
    freq_info_array = get_freq_info(&info, &n);
    EXPECT_EQ(2, n);
    vfree(freq_info_array);
}

TEST_F(AccessedBitTest, GetHamPagesFreqs)
{
    struct ham_tracking_info info;
    struct freq_info *freq_info_array;
    uint64_t num;

    EXPECT_TRUE(list_empty(&ham_pid_list));
    list_add(&info.node, &ham_pid_list);
    info.pid = 1;

    MOCKER(change_ap_type).stubs();
    MOCKER(get_freq_info).stubs().will(returnValue((struct freq_info *)nullptr));
    int ret = get_ham_pages_freqs(1, &freq_info_array, &num);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    struct freq_info array;
    MOCKER(change_ap_type).stubs();
    MOCKER(get_freq_info).stubs().will(returnValue(&array));
    MOCKER(clear_tracking_info).stubs();
    ret = get_ham_pages_freqs(1, &freq_info_array, &num);
    EXPECT_EQ(0, ret);
    list_del(&info.node);
}

extern "C" int hva_to_hpa_hugetlb(struct kvm *kvm, u64 host_va);
TEST_F(AccessedBitTest, hva_to_hpa_hugetlb)
{
    int ret;
    struct kvm kvm = { .mm = NULL };

    MOCKER(huge_page_size).stubs().will(returnValue(PAGE_SIZE_2M));
    ret = hva_to_hpa_hugetlb(nullptr, 0);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" pte_t *huge_pte_offset(struct mm_struct *mm, unsigned long addr, unsigned long sz);
TEST_F(AccessedBitTest, hva_to_hpa_hugetlb_two)
{
    int ret;
    struct kvm kvm = { .mm = NULL };

    MOCKER(huge_page_size).stubs().will(returnValue(PAGE_SIZE_4K));
    ret = hva_to_hpa_hugetlb(&kvm, 0);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" pte_t smap_huge_ptep_get(pte_t *ptep);
TEST_F(AccessedBitTest, hva_to_hpa_hugetlb_three)
{
    int ret;
    struct kvm kvm = { .mm = NULL };
    pte_t pte = { .pte = 0 };

    MOCKER(huge_page_size).stubs().will(returnValue(PAGE_SIZE_2M));
    MOCKER(huge_pte_offset).stubs().will(returnValue((pte_t *)nullptr));
    ret = hva_to_hpa_hugetlb(&kvm, 0);
    EXPECT_EQ(-EFAULT, ret);

    GlobalMockObject::verify();
    MOCKER(huge_page_size).stubs().will(returnValue(PAGE_SIZE_2M));
    MOCKER(huge_pte_offset).stubs().will(returnValue(&pte));
    MOCKER(smap_huge_ptep_get).stubs().will(returnValue(pte));
    ret = hva_to_hpa_hugetlb(&kvm, 0);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" void actc_data_add(phys_addr_t paddr, u32 page_size);
TEST_F(AccessedBitTest, hva_to_hpa_hugetlb_success)
{
    int ret;
    struct kvm kvm = { .mm = NULL };
    pte_t pte = { .pte = 1 };

    MOCKER(huge_page_size).stubs().will(returnValue(PAGE_SIZE_2M));
    MOCKER(huge_pte_offset).stubs().will(returnValue(&pte));
    MOCKER(actc_data_add).expects(once()).will(ignoreReturnValue());
    ret = hva_to_hpa_hugetlb(&kvm, 0);
    EXPECT_EQ(0, ret);
}

extern "C" int hva_to_hpa(struct kvm *kvm, u64 host_va, scan_type type, pid_t pid);
TEST_F(AccessedBitTest, hvaToHpaWithNullKvm)
{
    int ret;
    struct kvm kvm = { .mm = NULL };

    ret = hva_to_hpa(nullptr, 0, NORMAL_SCAN, 1);
    EXPECT_EQ(-EINVAL, ret);

    ret = hva_to_hpa(&kvm, 0, NORMAL_SCAN, 1);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr);
TEST_F(AccessedBitTest, hvaToHpaWithErrorVma)
{
    int ret;
    struct mm_struct mm;
    struct kvm kvm = { .mm = &mm };
    struct vm_area_struct vma;

    MOCKER(find_vma).stubs().will(returnValue(static_cast<struct vm_area_struct *>(nullptr)));
    ret = hva_to_hpa(&kvm, 0, NORMAL_SCAN, 1);
    EXPECT_EQ(-EFAULT, ret);
}

extern "C" int hva_to_hpa_ham(struct kvm *kvm, u64 host_va, pid_t pid);
TEST_F(AccessedBitTest, hvaToHpaTest)
{
    int ret;
    struct mm_struct mm;
    struct kvm kvm = { .mm = &mm };
    struct vm_area_struct vma;

    // ret value of hva_to_hpa_hugetlb and hva_to_hpa_ham is ignored,
    // since all following tests should success
    MOCKER(find_vma).stubs().will(returnValue(&vma));
    MOCKER(is_vm_hugetlb_page).stubs().will(returnValue(false));
    ret = hva_to_hpa(&kvm, 0, HAM_SCAN, 1);
    EXPECT_EQ(0, ret);
    ret = hva_to_hpa(&kvm, 0, HAM_SCAN, 1);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(find_vma).stubs().will(returnValue(&vma));
    MOCKER(is_vm_hugetlb_page).stubs().will(returnValue(true));
    MOCKER(hva_to_hpa_ham).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    MOCKER(hva_to_hpa_hugetlb).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    ret = hva_to_hpa(&kvm, 0, HAM_SCAN, 1);
    EXPECT_EQ(0, ret);
    ret = hva_to_hpa(&kvm, 0, HAM_SCAN, 1);
    EXPECT_EQ(0, ret);
}

extern "C" bool find_tracking_index(struct statistics_tracking_info *stat_info,
                                    unsigned long vaddr, unsigned int *index);
TEST_F(AccessedBitTest, FindTrackingIndex)
{
    unsigned long vaddr = 0x2;
    unsigned int index;
    struct statistics_tracking_info stat_info;
    stat_info.page_num[0] = 1;
    stat_info.page_num[1] = 1;
    u64 addr1[1] = { 0x1 };
    u64 addr2[1] = { 0x2 };
    stat_info.vaddr[0] = addr1;
    stat_info.vaddr[1] = addr2;

    bool ret = find_tracking_index(&stat_info, vaddr, &index);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(1, index);
}

extern "C" int calc_tracking_index(unsigned long vaddr, pid_t pid, unsigned int *index);
TEST_F(AccessedBitTest, CalcTrackingIndex)
{
    unsigned long vaddr = 0x2;
    unsigned int index;
    struct statistics_tracking_info stat_info;

    EXPECT_EQ(true, list_empty(&statistic_pid_list));
    list_add(&stat_info.node, &statistic_pid_list);
    stat_info.pid = 1;
    stat_info.page_num[0] = 1;
    stat_info.page_num[1] = 1;
    u64 addr1[1] = { 0x1 };
    u64 addr2[1] = { 0x2 };
    stat_info.vaddr[0] = addr1;
    stat_info.vaddr[1] = addr2;

    int ret = calc_tracking_index(0x2, 1, &index);
    EXPECT_EQ(0, ret);

    ret = calc_tracking_index(0x3, 1, &index);
    EXPECT_EQ(-ERANGE, ret);
    list_del(&stat_info.node);
}

extern "C" void update_tracking_info(pid_t pid, unsigned int index);
extern "C" int fill_statistics_tracking(unsigned long hva, pid_t pid);
TEST_F(AccessedBitTest, FillStatisticsTracking)
{
    MOCKER(calc_tracking_index).stubs().will(returnValue(0));
    MOCKER(update_tracking_info).stubs();
    int ret = fill_statistics_tracking(1, 1);
    EXPECT_EQ(0, ret);
}

extern "C" struct vm_area_struct *get_vma_if_huge_page(struct kvm *kvm, unsigned long host_va);
extern "C" int scan_kvm_memslots(struct kvm *kvm, pid_t pid, int page_size, scan_type type);
TEST_F(AccessedBitTest, scanKvmMemslotsWithNullKvm)
{
    int ret;
    struct mm_struct mm;
    struct kvm_pgtable pgt;
    struct kvm_arch arch = {
        .mmu = { 0 }
    };
    struct kvm kvm = {
        .mm = &mm,
        .arch = arch
    };
    struct kvm_memslots slots;

    ret = scan_kvm_memslots(&kvm, 1, 0, STATISTIC_SCAN);
    EXPECT_EQ(-EINVAL, ret);

    MOCKER(kvm_memslots).stubs().will(returnValue((struct kvm_memslots *)nullptr));
    ret = scan_kvm_memslots(&kvm, 1, 0, STATISTIC_SCAN);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(AccessedBitTest, scanKvmMemslotsTest)
{
    int ret;
    struct mm_struct mm;
    struct kvm_pgtable pgt;
    struct vm_area_struct vma;
    struct kvm_arch arch = {
        .mmu = { .pgt = &pgt }
    };
    struct kvm kvm = {
        .mm = &mm,
        .arch = arch
    };
    struct kvm_memory_slot memslots[] = {
        { .base_gfn = 0x0, .npages = 2048, .userspace_addr = 0xffff000000 },
        { .base_gfn = 0x1000, .npages = 2048, .userspace_addr = 0xffff800000 },
    };
    struct kvm_memslots slots = {
        .used_slots = 2,
        .memslots = memslots
    };

    // when scanning kvm_memslots, errors will be ignored
    MOCKER(kvm_memslots).stubs().will(returnValue(&slots));
    MOCKER(smap_kvm_pgtable_stage2_is_young).stubs().will(returnValue(true));
    MOCKER(gfn_to_hva_memslot).stubs().will(returnValue((unsigned long)0));
    MOCKER(get_vma_if_huge_page).stubs()
        .will(returnValue(static_cast<struct vm_area_struct *>(NULL)))
        .then(returnValue(&vma));
    MOCKER(hva_to_hpa).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    MOCKER(smap_kvm_pgtable_stage2_mkold).stubs().will(returnValue(true));
    MOCKER(kvm_flush_remote_tlbs).stubs().will(ignoreReturnValue());
    ret = scan_kvm_memslots(&kvm, 1, PAGE_SIZE_2M, STATISTIC_SCAN);
    EXPECT_EQ(0, ret);
}

TEST_F(AccessedBitTest, hva_to_hpa_ham)
{
    int ret = hva_to_hpa_ham(nullptr, 0, 0);
    EXPECT_EQ(-EINVAL, ret);

    struct kvm kvm;
    pte_t ptep;
    struct ham_tracking_info info;
    u64 paddr1;
    u64 paddr2;
    u16 freq1;
    u16 freq2;
    info.pid = 1;
    info.l1_node = 0;
    info.l2_node = 1;
    info.paddr[L1] = &paddr1;
    info.paddr[L2] = &paddr2;
    info.freq[L1] = &freq1;
    info.freq[L2] = &freq2;
    info.len[L1] = info.len[L2] = 1;
    ptep.pte = 1;
    MOCKER(huge_page_size).stubs().will(returnValue(0x200000UL));
    MOCKER(huge_pte_offset).stubs().will(returnValue((pte_t *)&ptep));
    list_add(&info.node, &ham_pid_list);
    ret = hva_to_hpa_ham(&kvm, 0, 1);
    EXPECT_EQ(0, ret);

    ret = hva_to_hpa_ham(&kvm, 0, 1);
    EXPECT_EQ(0, ret);
    list_del(&info.node);
}

extern "C" int pre_scan_kvm_memslots(struct kvm *kvm);
TEST_F(AccessedBitTest, pre_scan_kvm_memslots)
{
    int ret;
    int idx = 2;
    struct kvm kvm;

    MOCKER(srcu_read_lock).expects(once()).will(returnValue(idx));
    MOCKER(mmap_read_lock).expects(once()).will(ignoreReturnValue());
    ret = pre_scan_kvm_memslots(&kvm);
    EXPECT_EQ(idx, ret);
}

extern "C" void post_scan_kvm_memslots(struct kvm *kvm, int idx);
TEST_F(AccessedBitTest, post_scan_kvm_memslots)
{
    int idx;
    struct kvm kvm;

    MOCKER(mmap_read_unlock).expects(once()).will(ignoreReturnValue());
    MOCKER(srcu_read_unlock).expects(once()).will(ignoreReturnValue());
    post_scan_kvm_memslots(&kvm, idx);
}

extern "C" unsigned long huge_page_size(struct hstate *h);
extern "C" int drivers_calc_paddr_acidx_iomem(u64 pa, int *nid, u64 *index, int page_size);
extern "C" int get_vma_numa_node(struct kvm *kvm, struct vm_area_struct *vma, unsigned long addr);
TEST_F(AccessedBitTest, GetVmaNUmaNode)
{
    struct kvm kvm;
    pte_t pte;

    pte.pte = 1;
    MOCKER(huge_page_size).stubs().will(returnValue(PAGE_SIZE_2M));
    MOCKER(huge_pte_offset).stubs().will(returnValue(&pte));
    MOCKER(smap_huge_ptep_get).stubs().will(returnValue(pte));
    MOCKER(drivers_calc_paddr_acidx_iomem).stubs().will(returnValue(1));
    MOCKER(calc_paddr_acidx_acpi).stubs().will(returnValue(1));
    int ret = get_vma_numa_node(&kvm, nullptr, 0);
    EXPECT_EQ(NUMA_NO_NODE, ret);
}

extern "C" int fill_vaddrs_info(struct kvm *kvm, struct hva_info *hva_vec, u64 len, u64 *l1_page_num,
                                u64 *l2_page_num);
extern "C" int get_hva_info_by_scan_kvm_memslots(struct kvm *kvm, u64 *l1_page_num, u64 *l2_page_num,
                                                 u64 total_huge_page_num, u64 *vaddrs, struct hva_info *hva_vec);
TEST_F(AccessedBitTest, getHvaInfoByScanKvmMemslots)
{
    u64 len = 1024;
    struct hva_info *hva_vec = static_cast<struct hva_info*>(malloc(len * sizeof(struct hva_info)));
    u64 *vaddrs = static_cast<u64*>(malloc(len * sizeof(u64)));
    u64 l1_page_num;
    u64 l2_page_num;
    struct kvm kvm;
    MOCKER(fill_vaddrs_info).stubs().will(returnValue(0));

    // generate random address and nid with fixed seed
    mt19937_64 gen(1234);
    uniform_int_distribution<int> randomNid(0, 8);
    uniform_int_distribution<u64> randomAddr(0x1000, 0xFFFFF);
    for (int i = 0; i < len; ++i) {
        hva_vec[i].nid = randomNid(gen);
        hva_vec[i].va = randomAddr(gen);
    }

    // check whether hva_vec sorted
    int ret = get_hva_info_by_scan_kvm_memslots(&kvm, &l1_page_num, &l2_page_num, len, vaddrs, hva_vec);
    EXPECT_EQ(0, ret);
    for (int i = 1; i < len; ++i) {
        if (hva_vec[i].nid == hva_vec[i - 1].nid) {
            EXPECT_LE(hva_vec[i - 1].va, hva_vec[i].va);
        } else {
            EXPECT_LE(hva_vec[i - 1].nid, hva_vec[i].nid);
        }
    }
    free(hva_vec);
    free(vaddrs);
}

extern "C" int alloc_vaddr_info(struct kvm *kvm, struct hva_info **hva_vec, u64 **vaddrs, u64 total_huge_page_nr);
TEST_F(AccessedBitTest, alloc_vaddr_info)
{
    struct kvm kvm;
    struct kvm_pgtable p_pgt;
    struct hva_info *hva_vec;
    u64 *vaddrs;

    kvm.arch.mmu.pgt = &p_pgt;
    int ret = alloc_vaddr_info(nullptr, nullptr, nullptr, 0);
    EXPECT_EQ(-EINVAL, ret);

    ret = alloc_vaddr_info(&kvm, &hva_vec, &vaddrs, 1);
    EXPECT_EQ(0, ret);

    vfree(hva_vec);
    vfree(vaddrs);
}

extern "C" struct pid *find_get_pid(pid_t nr);
extern "C" int get_kvm_by_pid(pid_t pid_nr, struct pid **pid, struct task_struct **task, struct file **filp,
                              struct kvm **kvm);
TEST_F(AccessedBitTest, get_kvm_by_pid)
{
    struct pid pid_tmp;
    struct task_struct task_tmp;
    struct file filp_tmp;
    struct kvm kvm_tmp;
    pid_t pid_nr;
    struct pid *pid;
    struct task_struct *task;
    struct file *filp;
    struct kvm *kvm;

    MOCKER(find_get_pid).stubs().will(returnValue(&pid_tmp));
    MOCKER(get_pid_task).stubs().will(returnValue(&task_tmp));
    MOCKER(get_kvm_file_from_task).stubs().will(returnValue(static_cast<struct file *>(nullptr)));
    int ret = get_kvm_by_pid(pid_nr, &pid, &task, &filp, &kvm);
    EXPECT_EQ(-EBADF, ret);

    GlobalMockObject::verify();
    filp_tmp.private_data = nullptr;
    MOCKER(find_get_pid).stubs().will(returnValue(&pid_tmp));
    MOCKER(get_pid_task).stubs().will(returnValue(&task_tmp));
    MOCKER(get_kvm_file_from_task).stubs().will(returnValue(&filp_tmp));
    ret = get_kvm_by_pid(pid_nr, &pid, &task, &filp, &kvm);
    EXPECT_EQ(-EINVAL, ret);

    filp_tmp.private_data = &kvm_tmp;
    ret = get_kvm_by_pid(pid_nr, &pid, &task, &filp, &kvm);
    EXPECT_EQ(0, ret);
}

extern "C" int get_total_huge_page_nr(struct kvm *kvm, u64 *total_huge_page_nr);
extern "C" int scan_hva_info(pid_t pid_nr, u64 *l1_page_num, u64 *l2_page_num, u64 **l1_vaddr, u64 **l2_vaddr);
TEST_F(AccessedBitTest, scan_hva_info)
{
    pid_t pid_nr;
    u64 l1_page_num;
    u64 l2_page_num;
    u64 *l1_vaddr;
    u64 *l2_vaddr;

    MOCKER(get_kvm_by_pid).stubs().will(returnValue(-EINVAL));
    int ret = scan_hva_info(pid_nr, &l1_page_num, &l2_page_num, &l1_vaddr, &l2_vaddr);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    MOCKER(pre_scan_kvm_memslots).stubs().will(returnValue(1));
    MOCKER(post_scan_kvm_memslots).stubs();
    MOCKER(get_kvm_by_pid).stubs().will(returnValue(0));
    MOCKER(get_total_huge_page_nr).stubs().will(returnValue(-1));
    ret = scan_hva_info(pid_nr, &l1_page_num, &l2_page_num, &l1_vaddr, &l2_vaddr);
    EXPECT_EQ(-1, ret);

    GlobalMockObject::verify();
    MOCKER(pre_scan_kvm_memslots).stubs().will(returnValue(1));
    MOCKER(post_scan_kvm_memslots).stubs();
    MOCKER(get_kvm_by_pid).stubs().will(returnValue(0));
    MOCKER(get_total_huge_page_nr).stubs().will(returnValue(0));
    MOCKER(alloc_vaddr_info).stubs().will(returnValue(-1));
    ret = scan_hva_info(pid_nr, &l1_page_num, &l2_page_num, &l1_vaddr, &l2_vaddr);
    EXPECT_EQ(-1, ret);
}

extern "C" void release_resources(struct file *filp, struct task_struct *task, struct pid *pid);
extern "C" struct pid *find_get_pid(pid_t nr);
extern "C" void put_pid(struct pid *pid);
TEST_F(AccessedBitTest, scan_forward_2M_pid_fail)
{
    int ret;
    MOCKER(find_get_pid).stubs().will(returnValue(static_cast<struct pid *>(nullptr)));
    MOCKER(get_pid_task).stubs().will(returnValue((struct task_struct *)nullptr));
    MOCKER(put_pid).expects(once()).will(ignoreReturnValue());
    ret = scan_forward_2M(1, PAGE_SIZE_2M, NORMAL_SCAN);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(AccessedBitTest, scan_forward_2M_kvm_file_fail)
{
    int ret;
    struct task_struct task;
    MOCKER(find_get_pid).stubs().will(returnValue(static_cast<struct pid *>(nullptr)));
    MOCKER(get_pid_task).stubs().will(returnValue(&task));
    MOCKER(get_kvm_file_from_task).stubs().will(returnValue(static_cast<struct file *>(nullptr)));
    MOCKER(release_resources).expects(once()).will(ignoreReturnValue());
    ret = scan_forward_2M(1, PAGE_SIZE_2M, NORMAL_SCAN);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(AccessedBitTest, scan_forward_2M_success)
{
    int ret;
    struct task_struct task;
    struct kvm kvm;
    struct file filp = { .private_data = &kvm };
    MOCKER(find_get_pid).stubs().will(returnValue(static_cast<struct pid *>(nullptr)));
    MOCKER(get_pid_task).stubs().will(returnValue(&task));
    MOCKER(get_kvm_file_from_task).stubs().will(returnValue(&filp));
    MOCKER(pre_scan_kvm_memslots).expects(once()).will(returnValue(0));
    MOCKER(scan_kvm_memslots).stubs().will(returnValue(0));
    MOCKER(post_scan_kvm_memslots).expects(once()).will(ignoreReturnValue());
    ret = scan_forward_2M(1, PAGE_SIZE_2M, NORMAL_SCAN);
    EXPECT_EQ(0, ret);
}

extern "C" int check_pte_young(pte_t *pte, unsigned long addr, unsigned long next, struct mm_walk *walk);
TEST_F(AccessedBitTest, check_pte_young)
{
    pte_t pte;
    pte.pte = 0;
    struct mm_walk walk;
    struct pte_walk pte_walk;

    walk.private_data = &pte_walk;
    int ret = check_pte_young(&pte, 1, 1, &walk);
    EXPECT_EQ(0, ret);

    pte.pte = 1;
    ret = check_pte_young(&pte, 1, 1, &walk);
    EXPECT_EQ(0, ret);
}

extern "C" int small_vma_walk(struct mm_struct *mm, unsigned long start_vaddr,
                              unsigned long end_vaddr, struct pte_walk *pte_walk);
TEST_F(AccessedBitTest, small_vma_walk)
{
    MOCKER(mmap_read_lock_killable).stubs().will(returnValue(1));

    int ret = small_vma_walk(nullptr, 1, 10, nullptr);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    MOCKER(mmap_read_lock_killable).stubs().will(returnValue(0));
    MOCKER(walk_page_range).stubs().will(returnValue(1));
    ret = small_vma_walk(nullptr, 1, 10, nullptr);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    MOCKER(mmap_read_lock_killable).stubs().will(returnValue(0));
    MOCKER(walk_page_range).stubs().will(returnValue(0));
    ret = small_vma_walk(nullptr, 1, 10, nullptr);
    EXPECT_EQ(0, ret);
}

extern "C" int huge_vma_walk(struct mm_struct *mm, struct smap_vma_struct *vma, struct pte_walk *pte_walk);
TEST_F(AccessedBitTest, huge_vma_walk)
{
    struct smap_vma_struct vma;

    vma.start_vaddr = 0;
    vma.end_vaddr = 0x100;
    MOCKER(small_vma_walk).stubs().will(returnValue(0));
    int ret = huge_vma_walk(nullptr, &vma, nullptr);
    EXPECT_EQ(0, ret);
}

extern "C" int take_vma_snapshot(struct mm_struct *mm, struct smap_vma_struct **vma_arr, int *vma_count);
TEST_F(AccessedBitTest, take_vma_snapshot)
{
    MOCKER(mmap_read_lock_killable).stubs().will(returnValue(1));
    int ret = take_vma_snapshot(nullptr, nullptr, nullptr);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    MOCKER(mmap_read_lock_killable).stubs().will(returnValue(0));
    ret = take_vma_snapshot(nullptr, nullptr, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" bool IS_ERR(const void *ptr);
extern "C" struct mm_struct *mock_get_mm_by_pid(pid_t pid);
extern "C" int scan_forward_4k_mm(int pid, int page_size);
extern "C" int take_vma_snapshot(struct mm_struct *mm,
                                 struct smap_vma_struct **vma_arr, int *vma_count);
TEST_F(AccessedBitTest, scan_forward_4K_mm)
{
    struct mm_struct mm;

    MOCKER(mock_get_mm_by_pid).stubs().will(returnValue(static_cast<struct mm_struct *>(nullptr)));
    int ret = scan_forward_4k_mm(1, PAGE_SIZE_4K);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();

    MOCKER(take_vma_snapshot).stubs().will(returnValue(0));
    MOCKER(mock_get_mm_by_pid).stubs().will(returnValue(&mm));
    MOCKER(IS_ERR).stubs().will(returnValue(false));
    MOCKER(kfree).expects(once()).will(ignoreReturnValue());
    ret = scan_forward_4k_mm(1, PAGE_SIZE_4K);
    EXPECT_EQ(0, ret);
}

TEST_F(AccessedBitTest, scan_accessed_bit_forward_mm)
{
    int ret;
    int pid = 1234;
    MOCKER(scan_forward_4k_mm).stubs().will(returnValue(1));
    ret = scan_accessed_bit_forward_mm(pid, PAGE_SIZE_4K);
    EXPECT_EQ(1, ret);

    ret = scan_accessed_bit_forward_mm(pid, PAGE_SIZE_2M);
    EXPECT_EQ(-EINVAL, ret);
}
