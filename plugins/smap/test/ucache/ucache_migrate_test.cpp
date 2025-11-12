/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */
 
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
 
#include "ucache_migrate.h"
#include "linux_mock.h"
 
using namespace std;
 
class UCacheMigrateTest : public ::testing::Test {
protected:
    void CreateInactiveFileLruvec()
    {
        for (int i = 0; i < inactiveFileLruvecLen; i++) {
            list_add(&inactiveFileLruvec[i].lru, &inactiveFileLruvecHead);
        }
    }
 
    void SetUp() override
    {
        CreateInactiveFileLruvec();
    }
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
 
    static const int inactiveFileLruvecLen = 5;
    struct folio inactiveFileLruvec[inactiveFileLruvecLen];
    LIST_HEAD(inactiveFileLruvecHead);
};

extern "C" struct lruvec* get_lruvec(int nid, pid_t pid);
TEST_F(UCacheMigrateTest, UCacheScanFolios)
{
    struct task_struct tst;
    struct mem_cgroup *pmemcg = nullptr;
    struct mem_cgroup memcg;
    struct lruvec lruvec1;
    lruvec1.lists[LRU_INACTIVE_FILE] = inactiveFileLruvecHead;
 
    const unsigned int originNrFolios = 4;
    struct folio *folios[originNrFolios];
    unsigned int nrFolios = originNrFolios;
    unsigned int *pNrFolios = nullptr;
    int ret;
 
    ret = ucache_scan_folios(0, 1, folios, pNrFolios);
    EXPECT_EQ(-1, ret);
 
    ret = ucache_scan_folios(0, 1, folios, &nrFolios);
    EXPECT_EQ(-1, ret);
 
    nrFolios = originNrFolios;
    MOCKER(pid_task).stubs().will(returnValue(&tst));
    ret = ucache_scan_folios(0, 1, folios, &nrFolios);
    EXPECT_EQ(-1, ret);
 
    nrFolios = originNrFolios;
    MOCKER(mem_cgroup_from_task).defaults().will(returnValue(pmemcg));
    ret = ucache_scan_folios(0, 1, folios, &nrFolios);
    EXPECT_EQ(-1, ret);
 
    nrFolios = originNrFolios;
    MOCKER(get_lruvec).stubs().will(returnValue(&lruvec1));
    ret = ucache_scan_folios(0, 1, folios, &nrFolios);
    EXPECT_EQ(0, ret);
 
    nrFolios = originNrFolios;
    lruvec1.lists[LRU_INACTIVE_FILE].next = &lruvec1.lists[LRU_INACTIVE_FILE];
    lruvec1.lists[LRU_INACTIVE_FILE].prev = &lruvec1.lists[LRU_INACTIVE_FILE];
    ret = ucache_scan_folios(0, 1, folios, &nrFolios);
    EXPECT_EQ(0, ret);
 
    nrFolios = originNrFolios;
    lruvec1.lists[LRU_INACTIVE_FILE] = inactiveFileLruvecHead;
    ret = ucache_scan_folios(0, 1, folios, &nrFolios);
    EXPECT_EQ(0, ret);
 
    nrFolios = originNrFolios;
    MOCKER(folio_try_get).stubs().will(returnValue(false)).then(returnValue(true));
    ret = ucache_scan_folios(0, 1, folios, &nrFolios);
    EXPECT_EQ(0, ret);
}
 
extern "C" void create_migrate_success(int nid, unsigned int nr_folios, unsigned int nr_succeeded);
TEST_F(UCacheMigrateTest, CreateMigrateSuccess)
{
    const unsigned int nrFolios = 10;
    const unsigned int nrSucceeded = 5;
    create_migrate_success(0, nrFolios, nrSucceeded);
}
 
TEST_F(UCacheMigrateTest, UCacheMigrateFolios)
{
    int ret;
    const int nrFolios = 4;
    struct folio *folios[nrFolios];
    struct migrate_success migrateSuccess;
 
    MOCKER(isolate_and_migrate_folios).stubs().will(returnValue(0));
    ret = ucache_migrate_folios(1, folios, nrFolios);
    EXPECT_EQ(0, ret);
 
    MOCKER(numa_is_remote_node).stubs().will(returnValue(true));
    ret = ucache_migrate_folios(1, folios, nrFolios);
    EXPECT_EQ(0, ret);
 
    MOCKER(idr_find).stubs().will(returnValue((void *)&migrateSuccess));
    ret = ucache_migrate_folios(1, folios, nrFolios);
    EXPECT_EQ(0, ret);
}
 
TEST_F(UCacheMigrateTest, GetMigrateSuccess)
{
    struct migrate_success *migrateSuccess;
    struct migrate_success ret;
    migrateSuccess = get_migrate_success(0);
    EXPECT_EQ(nullptr, migrateSuccess);
 
    MOCKER(idr_find).stubs().will(returnValue((void *)&ret));
    migrateSuccess = get_migrate_success(1);
    EXPECT_NE(nullptr, migrateSuccess);
}