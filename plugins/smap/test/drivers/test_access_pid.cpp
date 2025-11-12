/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: SMAP3.0 access_pid模块测试代码
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "access_ioctl.h"
#include "access_tracking.h"
#include "access_mmu.h"
#include "access_acpi_mem.h"
#include "access_pid.h"

using namespace std;

extern "C" struct list_head ham_pid_list;
extern "C" spinlock_t ham_lock;
extern "C" struct access_pid_struct ap_data;
extern struct list_head access_dev;

class DriversAccessPidTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        INIT_LIST_HEAD(&ap_data.list);
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(DriversAccessPidTest, DestroyAccessPid)
{
    struct access_pid *elem;

    elem = (struct access_pid *)kzalloc(sizeof(struct access_pid), GFP_KERNEL);
    elem->paddr_bm[0] = (unsigned long *)kmalloc(sizeof(unsigned long) * 2, GFP_KERNEL);
    elem->paddr_bm[1] = (unsigned long *)kmalloc(sizeof(unsigned long) * 2, GFP_KERNEL);
    ASSERT_NE(nullptr, elem);
    ASSERT_NE(nullptr, elem->paddr_bm[0]);
    ASSERT_NE(nullptr, elem->paddr_bm[1]);

    destroy_access_pid(elem);
}

extern "C" void destroy_access_ham_pid(struct ham_tracking_info *elem);
extern "C" void destroy_access_statistic_pid(struct statistics_tracking_info *elem);
TEST_F(DriversAccessPidTest, DestroyAccessHamPid)
{
    struct ham_tracking_info *elem;

    elem = (struct ham_tracking_info *)kzalloc(sizeof(struct ham_tracking_info), GFP_KERNEL);
    elem->paddr[0] = (u64 *)kmalloc(sizeof(u64) * 2, GFP_KERNEL);
    elem->freq[0] = (u16 *)kmalloc(sizeof(u16) * 2, GFP_KERNEL);
    ASSERT_NE(nullptr, elem);
    ASSERT_NE(nullptr, elem->paddr[0]);
    ASSERT_NE(nullptr, elem->freq[0]);
    elem ->l2_node = -1;
    destroy_access_ham_pid(elem);
}

extern "C" void destroy_access_statistic_pid(struct statistics_tracking_info *elem);
TEST_F(DriversAccessPidTest, DestroyAccessStatisticPid)
{
    struct statistics_tracking_info elem;

    elem.window_num = 1;
    MOCKER(vfree).stubs();
    MOCKER(kfree).stubs();
    destroy_access_statistic_pid(&elem);
    EXPECT_EQ(nullptr, elem.sliding_windows);
}

extern "C" void add_kvm_seg(struct vm_mapping_info *info, struct kvm_memory_slot *memslot, u64 gfn_start, u64 gfn_end);
TEST_F(DriversAccessPidTest, AddKvmSeg)
{
    struct vm_mapping_info info;
    struct kvm_memory_slot memslot;
    info.nr_segs = 0;
    memslot.base_gfn = 0;
    memslot.userspace_addr = 0;
    add_kvm_seg(&info, &memslot, 0, 1);
    EXPECT_EQ(1, info.nr_segs);
}

extern "C" int scan_kvm_gfn(struct kvm *kvm, struct vm_mapping_info *info);
TEST_F(DriversAccessPidTest, ScanKvmGfn)
{
    struct kvm kvm;
    struct kvm_memslots slots;
    struct vm_mapping_info info;
    struct kvm_memory_slot memslots;
    slots.memslots = &memslots;
    int ret = scan_kvm_gfn(nullptr, nullptr);
    EXPECT_EQ(-EINVAL, ret);

    ret = scan_kvm_gfn(&kvm, nullptr);
    EXPECT_EQ(-EINVAL, ret);

    slots.used_slots = 1;
    memslots.npages = 0;
    MOCKER(kvm_memslots).stubs().will(returnValue((struct kvm_memslots *)&slots));
    MOCKER(add_kvm_seg).stubs();
    ret = scan_kvm_gfn(&kvm, &info);
    EXPECT_EQ(0, ret);
}

extern "C" int pre_scan_kvm_gfn(struct kvm *kvm);
TEST_F(DriversAccessPidTest, PreScanKvmGfn)
{
    struct kvm kvm;
    int ret = pre_scan_kvm_gfn(&kvm);
    EXPECT_EQ(0, ret);
}

extern "C" void post_scan_kvm_gfn(struct kvm *kvm, int idx);
TEST_F(DriversAccessPidTest, PostScanKvmGfn)
{
    struct kvm kvm;
    post_scan_kvm_gfn(&kvm, 0);
}

extern "C" struct pid *find_get_pid(pid_t nr);
extern "C" int init_vm_mapping_info(pid_t pid, struct vm_mapping_info *info);
TEST_F(DriversAccessPidTest, InitVmMappingInfo)
{
    struct file filp;
    struct task_struct *task;
    struct pid pid_s;

    int ret = init_vm_mapping_info(1, nullptr);
    EXPECT_EQ(-EINVAL, ret);

    MOCKER(find_get_pid).stubs().will(returnValue((struct pid *)&pid_s));
    MOCKER(get_pid_task).stubs().will(returnValue((struct task_struct *)&task));
    MOCKER(get_kvm_file_from_task).stubs().will(returnValue((struct file *)nullptr));
    ret = init_vm_mapping_info(1, nullptr);
    EXPECT_EQ(-EINVAL, ret);

    GlobalMockObject::verify();
    filp.private_data = nullptr;
    MOCKER(find_get_pid).stubs().will(returnValue((struct pid *)&pid_s));
    MOCKER(get_pid_task).stubs().will(returnValue((struct task_struct *)&task));
    MOCKER(get_kvm_file_from_task).stubs().will(returnValue((struct file *)&filp));
    ret = init_vm_mapping_info(1, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

extern "C" struct pid *find_get_pid(pid_t nr);
TEST_F(DriversAccessPidTest, InitVmMappingInfoTwo)
{
    struct file filp;
    struct task_struct *task;
    int private_data;
    struct pid pid_s;

    filp.private_data = &private_data;
    MOCKER(find_get_pid).stubs().will(returnValue((struct pid *)&pid_s));
    MOCKER(get_pid_task).stubs().will(returnValue((struct task_struct *)&task));
    MOCKER(get_kvm_file_from_task).stubs().will(returnValue((struct file *)&filp));
    MOCKER(pre_scan_kvm_gfn).stubs().will(returnValue(0));
    MOCKER(scan_kvm_gfn).stubs().will(returnValue(1));
    int ret = init_vm_mapping_info(1, nullptr);
    EXPECT_EQ(1, ret);

    GlobalMockObject::verify();
    struct vm_mapping_info info;
    MOCKER(find_get_pid).stubs().will(returnValue((struct pid *)&pid_s));
    MOCKER(get_pid_task).stubs().will(returnValue((struct task_struct *)&task));
    MOCKER(get_kvm_file_from_task).stubs().will(returnValue((struct file *)&filp));
    MOCKER(pre_scan_kvm_gfn).stubs().will(returnValue(0));
    MOCKER(scan_kvm_gfn).stubs().will(returnValue(0));
    ret = init_vm_mapping_info(1, &info);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversAccessPidTest, InitAccessPid)
{
    int ret;
    MOCKER(kzalloc).stubs().will(returnValue((void*)NULL));
    ret = init_access_pid(nullptr, nullptr);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    struct access_pid ap;
    struct access_pid *tmp;
    struct access_add_pid_payload payload = { 0 };
    MOCKER(kmalloc).stubs().will(returnValue((void *)&ap));
    MOCKER(init_vm_mapping_info).stubs().will(returnValue(0));
    ret = init_access_pid(&payload, &tmp);
    EXPECT_EQ(0, ret);
}

TEST_F(DriversAccessPidTest, PrintAccessPidList)
{
    print_access_pid_list();
}

TEST_F(DriversAccessPidTest, PrintfAccessStatisticPidList)
{
    print_access_statistic_pid_list();
}

TEST_F(DriversAccessPidTest, PrintAccessHamPidList)
{
    struct ham_tracking_info ap;
    struct ham_tracking_info *ap1;
    struct ham_tracking_info *tmp;

    list_for_each_entry_safe(ap1, tmp, &ham_pid_list, node) {
        list_del(&ap1->node);
    }
    ap.pid = 112;
    ap.l1_node = 0;
    ap.l2_node = -1;
    list_add(&ap.node, &ham_pid_list);
    print_access_ham_pid_list();
    list_del(&ap.node);
}

extern "C" int init_ham_pid_memory(struct ham_tracking_info *info, enum node_level level);
TEST_F(DriversAccessPidTest, InitHamPidMemery)
{
    u64 paddr;
    struct ham_tracking_info info;
    MOCKER(vzalloc).stubs().will(returnValue((void *)nullptr));
    int ret = init_ham_pid_memory(&info, L1);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    MOCKER(vzalloc).stubs().will(returnValue((void *)&paddr)).then(returnValue((void *)nullptr));
    MOCKER(vfree).stubs();
    ret = init_ham_pid_memory(&info, L1);
    EXPECT_EQ(-ENOMEM, ret);
}

extern "C" int init_ham_pid_len(struct ham_tracking_info *tmp);
TEST_F(DriversAccessPidTest, InitHamPidLenOne)
{
    struct ham_tracking_info info;
    struct access_tracking_dev adev;

    info.l1_node = 0;
    info.l2_node = -1;
    adev.node = 0;
    adev.page_count = 10;
    list_add(&adev.list, &access_dev);
    MOCKER(init_ham_pid_memory).stubs().will(returnValue(0));
    int ret = init_ham_pid_len(&info);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(10, info.len[0]);
    list_del(&adev.list);
}

TEST_F(DriversAccessPidTest, InitHamPidLenTwo)
{
    struct ham_tracking_info *info;
    struct access_tracking_dev adev;

    info = (struct ham_tracking_info *)kmalloc(sizeof(struct ham_tracking_info), GFP_KERNEL);
    ASSERT_NE(nullptr, info);
    info->l1_node = -1,
    info->l2_node = 4;
    info->paddr[0] = NULL;
    info->freq[0] = NULL;
    adev.node = 4;
    adev.page_count = 10;
    list_add(&adev.list, &access_dev);
    MOCKER(init_ham_pid_memory).stubs().will(returnValue(-1));
    int ret = init_ham_pid_len(info);
    EXPECT_EQ(-ENOMEM, ret);
    list_del(&adev.list);
}

extern "C" int find_first_remote_numa(u32 *nodes);
extern "C" int init_access_ham_pid(struct access_add_pid_payload *payload);
TEST_F(DriversAccessPidTest, InitAccessHamPidOne)
{
    struct ham_tracking_info tmp;
    struct access_add_pid_payload payload;

    nr_local_numa = 4;
    tmp.pid = 1;
    tmp.l1_node = 0;
    tmp.l2_node = -1;
    payload.pid = 1;
    payload.numa_nodes = 0x11;
    int ret = list_empty(&ham_pid_list);
    EXPECT_EQ(1, ret);
    list_add(&tmp.node, &ham_pid_list);
    MOCKER(find_first_remote_numa).stubs().will(returnValue(4));
    ret = init_access_ham_pid(&payload);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(4, tmp.l2_node);
    list_del(&tmp.node);
}

TEST_F(DriversAccessPidTest, AccessAddHamPidOne)
{
    struct access_add_pid_payload payload;

    payload.type = HAM_SCAN;
    MOCKER(init_access_ham_pid).stubs().will(returnValue(0));
    int ret = access_add_ham_pid(1, &payload);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(init_access_ham_pid).stubs().will(returnValue(-1));
    ret = access_add_ham_pid(1, &payload);
    EXPECT_EQ(-1, ret);

    // wrong type case, should return 0
    payload.type = NORMAL_SCAN;
    ret = access_add_ham_pid(1, &payload);
    EXPECT_EQ(0, ret);
}

extern "C" int init_access_statistic_pid(struct access_add_pid_payload *payload);
extern "C" int scan_hva_info(pid_t pid_nr, u64 *l1_page_num, u64 *l2_page_num,
                             u64 **l1_vaddr, u64 **l2_vaddr);
extern "C" int init_statistic_window(u8 ***sliding_windows, u32 duration, u32 scan_time,
                                     u64 total_page_num, u64 *windows_num);
extern "C" void destroy_access_statistic_pid(struct statistics_tracking_info *elem);
TEST_F(DriversAccessPidTest, InitAccessStatisticPidTest)
{
    struct statistics_tracking_info *tmp;
    struct statistics_tracking_info *pos;
    struct access_add_pid_payload payload;
    bool findFlag = false;
    nr_local_numa = 4;
    payload.pid = 17986;
    payload.numa_nodes = 0x01;

    int ret = list_empty(&statistic_pid_list);
    EXPECT_EQ(1, ret);

    MOCKER(scan_hva_info).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));
    MOCKER(init_statistic_window).stubs()
        .will(returnValue(-EINVAL))
        .then(returnValue(0));

    // failed case
    ret = init_access_statistic_pid(&payload);
    EXPECT_EQ(-EINVAL, ret);
    ret = init_access_statistic_pid(&payload);
    EXPECT_EQ(-EINVAL, ret);

    // success case
    ret = init_access_statistic_pid(&payload);
    EXPECT_EQ(0, ret);
    list_for_each_entry(tmp, &statistic_pid_list, node) {
        if (tmp->pid == 17986) {
            findFlag = true;
            EXPECT_EQ(0, tmp->l1_node);
            break;
        }
    }
    EXPECT_EQ(true, findFlag);

    // duplicate payload case, should update statistic_pid_list
    MOCKER(destroy_access_statistic_pid).stubs();
    payload.pid = 17986;
    payload.numa_nodes = 0x02;
    ret = init_access_statistic_pid(&payload);
    EXPECT_EQ(0, ret);
    // check whether statistic_pid_list update
    findFlag = false;
    list_for_each_entry(tmp, &statistic_pid_list, node) {
        if (tmp->pid == 17986) {
            findFlag = true;
            EXPECT_EQ(1, tmp->l1_node);
            break;
        }
    }
    EXPECT_EQ(true, findFlag);

    // free memory and reset statistic_pid_list
    list_for_each_entry_safe(pos, tmp, &statistic_pid_list, node) {
        list_del(&pos->node);
        kfree(pos);
    }
}

TEST_F(DriversAccessPidTest, AccessAddStatisticPid)
{
    struct access_add_pid_payload payload;
    payload.type = STATISTIC_SCAN;
    MOCKER(init_access_statistic_pid).stubs().will(returnValue(0));
    int ret = access_add_statistic_pid(1, &payload);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    MOCKER(init_access_statistic_pid).stubs().will(returnValue(-EINVAL));
    ret = access_add_statistic_pid(1, &payload);
    EXPECT_EQ(-EINVAL, ret);

    // wrong type case, should return 0
    payload.type = NORMAL_SCAN;
    ret = access_add_statistic_pid(1, &payload);
    EXPECT_EQ(0, ret);
}

extern "C" int get_total_huge_page_nr(struct kvm *kvm, u64 *total_huge_page_nr);
extern "C" struct vm_area_struct* get_vma_if_huge_page(struct kvm *kvm, unsigned long host_va);
TEST_F(DriversAccessPidTest, GetTotalHugePageNrTest)
{
    struct kvm_memory_slot memslots[] = {
        { .base_gfn = 0x0, .npages = 2048, .userspace_addr = 0xffff000000 },
        { .base_gfn = 0x1000, .npages = 2048, .userspace_addr = 0xffff800000 },
    };
    struct vm_area_struct vma;
    struct kvm_memslots slots = {.used_slots = 2, .memslots = memslots};
    MOCKER(kvm_memslots).stubs().will(returnValue((struct kvm_memslots *)&slots));
    MOCKER(get_vma_if_huge_page).stubs()
        .will(returnValue(static_cast<struct vm_area_struct*>(NULL)))
        .then(returnValue(static_cast<struct vm_area_struct*>(&vma)));
    struct kvm kvm;
    u64 total_huge_page_nr = 0;
    int ret = get_total_huge_page_nr(&kvm, &total_huge_page_nr);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(7, total_huge_page_nr);
}

extern "C" void submit_one_work(struct access_pid *ap);
TEST_F(DriversAccessPidTest, AccessAddPid)
{
    int ret;
    struct access_pid ap;
    struct access_pid *tmp;
    ap.pid = 1;
    tmp = &ap;
    struct access_add_pid_payload payload = {0};

    // failed case
    MOCKER(init_access_pid).stubs().will(returnValue(1));
    MOCKER(submit_one_work).stubs();
    ret = access_add_pid(1, &payload);
    EXPECT_EQ(1, ret);

    // success case
    GlobalMockObject::verify();
    INIT_LIST_HEAD(&ap_data.list);
    MOCKER(init_access_pid).stubs().with(&payload, outBoundP(&tmp, sizeof(tmp))).will(returnValue(0));
    MOCKER(submit_one_work).stubs();
    ret = access_add_pid(1, &payload);
    EXPECT_EQ(0, ret);
}

int ap_data_len()
{
    int cnt = 0;
    struct access_pid *tmp;
    list_for_each_entry(tmp, &ap_data.list, node) {
        cnt++;
    }
    return cnt;
}

TEST_F(DriversAccessPidTest, AccessAddPidDuplicateCase)
{
    int ret;
    struct access_pid *tmp;
    struct access_add_pid_payload payload[2] = {0};
    bool findFlag = false;
    payload[0].pid = 14587;
    payload[1].pid = 14587;

    // duplicate payload case, should failed
    INIT_LIST_HEAD(&ap_data.list);
    struct access_pid ap = {.pid=14587, .type=NORMAL_SCAN, .scan_time=50};
    tmp = &ap;
    MOCKER(init_access_pid).stubs().with(&payload[0], outBoundP(&tmp, sizeof(tmp))).will(returnValue(0));
    MOCKER(destroy_access_pid).stubs().will(returnValue(0));
    MOCKER(submit_one_work).stubs();
    ret = access_add_pid(2, payload);
    EXPECT_EQ(-EINVAL, ret);
    EXPECT_EQ(0, ap_data_len());

    // duplicate pid case, should update value
    GlobalMockObject::verify();
    struct access_pid ap2 = {.pid=14587, .type=NORMAL_SCAN, .scan_time=50};
    tmp = &ap2;
    MOCKER(init_access_pid).stubs().with(&payload[0], outBoundP(&tmp, sizeof(tmp))).will(returnValue(0));
    MOCKER(destroy_access_pid).stubs().will(returnValue(0));
    MOCKER(submit_one_work).stubs();
    ret = access_add_pid(1, payload);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, ap_data_len());

    GlobalMockObject::verify();
    struct access_pid ap3 = {.pid=14587, .type=HAM_SCAN, .scan_time=100};
    tmp = &ap3;
    MOCKER(init_access_pid).stubs().with(&payload[0], outBoundP(&tmp, sizeof(tmp))).will(returnValue(0));
    MOCKER(destroy_access_pid).stubs().will(returnValue(0));
    MOCKER(submit_one_work).stubs();
    ret = access_add_pid(1, payload);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, ap_data_len());
    // check whether ap_data update
    list_for_each_entry(tmp, &ap_data.list, node) {
        if (tmp->pid == 14587) {
            findFlag = true;
            EXPECT_EQ(ap3.type, tmp->type);
            EXPECT_EQ(ap3.scan_time, tmp->scan_time);
            break;
        }
    }
    EXPECT_EQ(true, findFlag);
}

TEST_F(DriversAccessPidTest, accessRemoveStatisticPid)
{
    INIT_LIST_HEAD(&statistic_pid_list);
    struct access_remove_pid_payload payload[4];
    struct access_remove_pid_payload singlePayload[1];
    struct statistics_tracking_info *tmp;
    struct statistics_tracking_info info[4];
    int len = 4;
    for (int i = 0; i < len; ++i) {
        info[i].pid = 14583 + i;
        payload[i].pid = info[i].pid;
        list_add(&info[i].node, &statistic_pid_list);
    }
    singlePayload[0].pid = payload[0].pid;
    MOCKER(destroy_access_statistic_pid).stubs();

    // remove single payload case
    access_remove_statistic_pid(1, singlePayload);
    bool findFlag = false;
    list_for_each_entry(tmp, &statistic_pid_list, node) {
        if (tmp->pid == singlePayload[0].pid) {
            findFlag = true;
            break;
        }
    }
    EXPECT_EQ(false, findFlag);

    // remove all payload case
    access_remove_statistic_pid(len, payload);
    int ret = list_empty(&statistic_pid_list);
    EXPECT_EQ(1, ret);
}

TEST_F(DriversAccessPidTest, AccessRemoveHamPid)
{
    INIT_LIST_HEAD(&ham_pid_list);
    struct access_remove_pid_payload payload[4];
    struct access_remove_pid_payload singlePayload[1];
    struct ham_tracking_info *tmp;
    struct ham_tracking_info info[4];
    int len = 4;
    for (int i = 0; i < len; ++i) {
        info[i].pid = 14583 + i;
        payload[i].pid = info[i].pid;
        list_add(&info[i].node, &ham_pid_list);
    }
    singlePayload[0].pid = payload[0].pid;
    MOCKER(destroy_access_ham_pid).stubs();

    // remove single payload case
    access_remove_ham_pid(1, singlePayload);
    bool findFlag = false;
    list_for_each_entry(tmp, &ham_pid_list, node) {
        if (tmp->pid == singlePayload[0].pid) {
            findFlag = true;
            break;
        }
    }
    EXPECT_EQ(false, findFlag);

    // remove all payload case
    access_remove_ham_pid(len, payload);
    int ret = list_empty(&ham_pid_list);
    EXPECT_EQ(1, ret);
}

TEST_F(DriversAccessPidTest, AccessRemovePid)
{
    INIT_LIST_HEAD(&ap_data.list);
    struct access_remove_pid_payload payload[4];
    struct access_remove_pid_payload singlePayload[1];
    struct access_pid ap[4];
    int len = 4;
    for (int i = 0; i < len; ++i) {
        ap[i].pid = 14583 + i;
        payload[i].pid = ap[i].pid;
        list_add(&ap[i].node, &ap_data.list);
    }
    MOCKER(destroy_access_pid).stubs();

    // remove single payload case
    singlePayload[0].pid = payload[0].pid;
    access_remove_pid(1, singlePayload);
    EXPECT_EQ(3, ap_data_len());

    // remove all payload case
    access_remove_pid(len, payload);
    EXPECT_EQ(0, ap_data_len());
    int ret = list_empty(&ap_data.list);
    EXPECT_EQ(1, ret);
}

TEST_F(DriversAccessPidTest, AccessRemoveAllPid)
{
    struct access_pid ap;
    struct ham_tracking_info ap_ham;
    struct statistics_tracking_info ap_statistic;
    list_add(&ap.node, &ap_data.list);
    list_add(&ap_ham.node, &ham_pid_list);
    list_add(&ap_statistic.node, &statistic_pid_list);
    MOCKER(destroy_access_pid).stubs();
    MOCKER(destroy_access_ham_pid).stubs();
    MOCKER(destroy_access_statistic_pid).stubs();
    access_remove_all_pid();
    EXPECT_EQ(0, ap_data_len());
    int ret = list_empty(&ap_data.list);
    EXPECT_EQ(1, ret);
    ret = list_empty(&ham_pid_list);
    EXPECT_EQ(1, ret);
    ret = list_empty(&statistic_pid_list);
    EXPECT_EQ(1, ret);
}

extern "C" void free_ap_white_list_bm(struct access_pid *ap);
TEST_F(DriversAccessPidTest, FreeApWhiteListBm)
{
    struct access_pid ap;
    MOCKER(vfree).stubs();
    ap.bm_len[0] = 1;
    free_ap_white_list_bm(&ap);
    EXPECT_EQ(0, ap.bm_len[0]);
}

extern "C" int init_ap_bm(int node_len, u64 *node_page_count, struct access_pid *ap);
TEST_F(DriversAccessPidTest, InitApBm)
{
    int ret;
    struct access_pid ap;

    ap.numa_nodes = 0x10;
    u64 node_page_count[SMAP_MAX_NUMNODES] = { 0 };
    ret = init_ap_bm(2, node_page_count, &ap);
    EXPECT_EQ(0, ret);

    GlobalMockObject::verify();
    node_page_count[0] = 1;
    MOCKER(vzalloc).stubs().will(returnValue((void *)nullptr));
    MOCKER(vfree).stubs();
    ret = init_ap_bm(2, node_page_count, &ap);
    EXPECT_EQ(-ENOMEM, ret);
}

TEST_F(DriversAccessPidTest, ChangeApType)
{
    struct access_pid ap;

    ap.pid = 1;
    ap.type = HAM_SCAN;
    list_add(&ap.node, &ap_data.list);
    change_ap_type(1);
    EXPECT_EQ(NO_SCAN, ap.type);
    list_del(&ap.node);
}

TEST_F(DriversAccessPidTest, CleanLastApData)
{
    struct access_pid ap;
    ap.bm_len[0] = 1;
    ap.page_num[0] = 1;
    MOCKER(kfree).stubs();
    MOCKER(vfree).stubs();
    clean_last_ap_data(&ap);
    EXPECT_EQ(0, ap.bm_len[0]);
    EXPECT_EQ(0, ap.page_num[0]);
}

extern "C" int init_vm_mapping(struct vm_mapping_info *info);
TEST_F(DriversAccessPidTest, InitVmMapping)
{
    struct vm_mapping_info info;
    info.vm_size = 1;
    MOCKER(vmalloc).stubs().will(returnValue((void *)nullptr));
    int ret = init_vm_mapping(&info);
    EXPECT_EQ(-ENOMEM, ret);

    GlobalMockObject::verify();
    ret = init_vm_mapping(&info);
    EXPECT_EQ(0, ret);
    vfree(info.mapping);
}

TEST_F(DriversAccessPidTest, AccessWalkPagemap)
{
    int ret;
    int len = 1;
    struct access_pid tmp;
    struct access_pid *ap;
    struct access_tracking_dev adev;
    ap = &tmp;
    tmp.type = NORMAL_SCAN;
    adev.node = 0;
    adev.page_count = 1;
    list_add(&adev.list, &access_dev);
    MOCKER(clean_last_ap_data).stubs();
    MOCKER(init_ap_bm).stubs().will(returnValue(0));
    MOCKER(walk_pid_pagemap).stubs().will(ignoreReturnValue());
    ret = access_walk_pagemap(ap);
    EXPECT_EQ(0, ret);
    list_del(&adev.list);
}

TEST_F(DriversAccessPidTest, AccessWalkPagemapFail)
{
    int ret;
    int len = 1;
    struct access_pid tmp;
    struct access_pid *ap;
    struct access_tracking_dev adev;
    ap = &tmp;
    tmp.type = NORMAL_SCAN;
    adev.node = 0;
    adev.page_count = 1;
    list_add(&adev.list, &access_dev);
    MOCKER(clean_last_ap_data).stubs();
    MOCKER(init_ap_bm).stubs().will(returnValue(0)).then(returnValue(-ENOMEM));
    MOCKER(walk_pid_pagemap).stubs().will(ignoreReturnValue());

    ret = access_walk_pagemap(ap);
    EXPECT_EQ(0, ret);

    ret = access_walk_pagemap(ap);
    EXPECT_EQ(-ENOMEM, ret);
    list_del(&adev.list);
}

TEST_F(DriversAccessPidTest, ReadPidFreqFailed)
{
    int ret = read_pid_freq(0, nullptr, nullptr);
    EXPECT_EQ(-EINVAL, ret);

    actc_t *freq[SMAP_MAX_NUMNODES] = { 0 };
    size_t data_len;
    ret = read_pid_freq(0, &data_len, freq);
    EXPECT_EQ(-EINVAL, ret);
}

struct absolute_pos {
    unsigned long last_pos;
    unsigned long pos;
    long last_index;
    long index;
};

#define INITIAL_POS ULONG_MAX

constexpr uint64_t INVALID_PADDR = 0;
constexpr long INITIAL_POS_INDEX = -1;

extern "C" int calc_absolute_pos(unsigned long *paddr_bm, size_t bm_len, struct absolute_pos *abs_pos);
TEST_F(DriversAccessPidTest, CalcAbsolutePosInvalidParams)
{
    int ret;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct absolute_pos absPos;

    ret = calc_absolute_pos(nullptr, 1, &absPos);
    EXPECT_EQ(-EINVAL, ret);

    ret = calc_absolute_pos(bitmap, 0, &absPos);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(DriversAccessPidTest, CalcAbsolutePosEqualToLast)
{
    int ret;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct absolute_pos absPos = {
        .last_pos = 66,
        .pos = INVALID_PADDR,
        .last_index = 2,
        .index = 2,
    };

    ret = calc_absolute_pos(bitmap, 2, &absPos);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(absPos.last_pos, absPos.pos);
}

TEST_F(DriversAccessPidTest, CalcAbsolutePosLessThanLast)
{
    int ret;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct absolute_pos absPos = {
        .last_pos = 66,
        .pos = INVALID_PADDR,
        .last_index = 2,
        .index = 1,
    };

    ret = calc_absolute_pos(bitmap, 2, &absPos);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(DriversAccessPidTest, CalcAbsolutePosLastExceedLimit)
{
    int ret;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct absolute_pos absPos = {
        .last_pos = BITS_PER_LONG * 2,
        .pos = INVALID_PADDR,
        .last_index = 4,
        .index = 5,
    };

    ret = calc_absolute_pos(bitmap, 2, &absPos);
    EXPECT_EQ(-ERANGE, ret);
}

TEST_F(DriversAccessPidTest, CalcAbsolutePosCurrExceedLimit)
{
    int ret;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct absolute_pos absPos = {
        .last_pos = 69,
        .pos = INVALID_PADDR,
        .last_index = 3,
        .index = 4,
    };

    ret = calc_absolute_pos(bitmap, 2, &absPos);
    EXPECT_EQ(-ERANGE, ret);
}

TEST_F(DriversAccessPidTest, CalcAbsolutePosCurrNoLast)
{
    int ret;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct absolute_pos absPos = {
        .last_pos = INITIAL_POS,
        .pos = INVALID_PADDR,
        .last_index = INITIAL_POS_INDEX,
        .index = 1,
    };

    ret = calc_absolute_pos(bitmap, 2, &absPos);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(5, absPos.pos);
}

TEST_F(DriversAccessPidTest, CalcAbsolutePosCurrHasLast)
{
    int ret;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct absolute_pos absPos = {
        .last_pos = 5,
        .pos = INVALID_PADDR,
        .last_index = 1,
        .index = 3,
    };

    ret = calc_absolute_pos(bitmap, 2, &absPos);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(69, absPos.pos);
}

extern "C" int check_parameters_and_state(u64 len, u64 *addr);
TEST_F(DriversAccessPidTest, ConvertPosToPaddrSortedInvalidParams)
{
    int ret;
    pid_t pid = 1025;
    int nid = 0;
    u64 addr[] = { 1, 2 };

    ret = convert_pos_to_paddr_sorted(pid, nid, 0, addr);
    EXPECT_EQ(-EINVAL, ret);

    ret = convert_pos_to_paddr_sorted(pid, nid, 1, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(DriversAccessPidTest, ConvertPosToPaddrSortedInvalidPid)
{
    int ret;
    pid_t pid = 1025;
    int nid = 0;
    u64 addr[] = { 1, 2 };
    u64 bmLen = 2;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct access_pid ap = { .pid = 1026, .bm_len = { bmLen }, .paddr_bm = { bitmap } };
    INIT_LIST_HEAD(&ap_data.list);
    list_add_tail(&ap.node, &ap_data.list);

    ret = convert_pos_to_paddr_sorted(pid, nid, 1, nullptr);
    EXPECT_EQ(-EINVAL, ret);
}

TEST_F(DriversAccessPidTest, ConvertPosToPaddrSortedInvalidAddr)
{
    int ret;
    pid_t pid = 1025;
    int nid = 0;
    u64 bmLen = 2;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct access_pid ap = { .pid = pid, .bm_len = { bmLen }, .paddr_bm = { bitmap } };
    INIT_LIST_HEAD(&ap_data.list);
    list_add_tail(&ap.node, &ap_data.list);
    u64 addr[] = { 4 };

    MOCKER(check_parameters_and_state).stubs().with().will(returnValue(0));
    ret = convert_pos_to_paddr_sorted(pid, nid, 1, addr);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, addr[0]);
}

TEST_F(DriversAccessPidTest, ConvertPosToPaddrSortedPartialFailure)
{
    int ret;
    pid_t pid = 1025;
    int nid = 0;
    u64 addr[] = { 1, 2 };
    u64 bmLen = 2;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct access_pid ap = { .pid = pid, .bm_len = { bmLen }, .paddr_bm = { bitmap } };
    INIT_LIST_HEAD(&ap_data.list);
    list_add_tail(&ap.node, &ap_data.list);
    u64 pa1 = 0x20000200000;
    u64 pa2 = 0x30000200000;

    nr_local_numa = 4;
    MOCKER(check_parameters_and_state).stubs().with().will(returnValue(0));
    MOCKER(calc_acidx_paddr_acpi)
        .stubs()
        .with(nid, static_cast<u64>(5), outBoundP(&pa1, sizeof(pa1)))
        .will(returnValue(0));
    MOCKER(calc_acidx_paddr_acpi)
        .stubs()
        .with(nid, static_cast<u64>(66))
        .will(returnValue(-ERANGE));
    ret = convert_pos_to_paddr_sorted(pid, nid, 2, addr);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(pa1, addr[0]);
    EXPECT_EQ(0, addr[1]);
}

TEST_F(DriversAccessPidTest, ConvertPosToPaddrSortedOK)
{
    int ret;
    pid_t pid = 1025;
    int nid = 0;
    u64 addr[] = { 1, 2 };
    u64 bmLen = 2;
    unsigned long bitmap[] = {
        0b0000000000000000000000000000000000000000000000000000000000100100,
        0b0000000000000000000000000000000000000000000000000000000000100100,
    };
    struct access_pid ap = { .pid = pid, .bm_len = { bmLen }, .paddr_bm = { bitmap } };
    INIT_LIST_HEAD(&ap_data.list);
    list_add_tail(&ap.node, &ap_data.list);
    u64 pa1 = 0x20000200000;
    u64 pa2 = 0x30000200000;

    nr_local_numa = 4;
    MOCKER(check_parameters_and_state).stubs().with().will(returnValue(0));
    MOCKER(calc_acidx_paddr_acpi)
        .stubs()
        .with(nid, static_cast<u64>(5), outBoundP(&pa1, sizeof(pa1)))
        .will(returnValue(0));
    MOCKER(calc_acidx_paddr_acpi)
        .stubs()
        .with(nid, static_cast<u64>(66), outBoundP(&pa2, sizeof(pa2)))
        .will(returnValue(0));
    ret = convert_pos_to_paddr_sorted(pid, nid, 2, addr);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(pa1, addr[0]);
    EXPECT_EQ(pa2, addr[1]);
}

TEST_F(DriversAccessPidTest, HistActcDataReinit)
{
    int ret;
    nr_local_numa = 2;
    g_hist_actc_page_count[5] = 5;
    MOCKER(get_node_page_cnt_iomem).stubs().will(returnValue((u64)0));
    ret = hist_actc_data_reinit();
    EXPECT_EQ(0, ret);
    EXPECT_EQ(nullptr, g_hist_actc_data[5]);

    GlobalMockObject::reset();
    MOCKER(get_node_page_cnt_iomem).stubs().will(returnValue((u64)10));
    ret = hist_actc_data_reinit();
    EXPECT_EQ(0, ret);
    EXPECT_EQ(10, g_hist_actc_page_count[5]);
    ASSERT_NE(nullptr, g_hist_actc_data[5]);

    GlobalMockObject::reset();
    MOCKER(get_node_page_cnt_iomem).stubs().will(returnValue((u64)5));
    ret = hist_actc_data_reinit();
    EXPECT_EQ(0, ret);
    EXPECT_EQ(5, g_hist_actc_page_count[5]);
    ASSERT_NE(nullptr, g_hist_actc_data[5]);
}

TEST_F(DriversAccessPidTest, HistActDataInit)
{
    int ret;
    MOCKER(get_node_page_cnt_iomem).stubs().will(returnValue((u64)0));
    ret = hist_actc_data_init();
    EXPECT_EQ(0, ret);

    GlobalMockObject::reset();
    MOCKER(get_node_page_cnt_iomem).stubs().will(returnValue((u64)10));
    ret = hist_actc_data_init();
    EXPECT_EQ(0, ret);
}

TEST_F(DriversAccessPidTest, HistActcDataDeinit)
{
    g_hist_actc_data[1] = (u16*)vzalloc(1 * sizeof(u16));
    hist_actc_data_deinit();
    EXPECT_EQ(nullptr, g_hist_actc_data[1]);
}