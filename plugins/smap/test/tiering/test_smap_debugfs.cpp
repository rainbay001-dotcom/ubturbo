/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: SMAP Debugfs Test
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <asm/page-def.h>
#include <linux/slab.h>

#include "smap_debugfs.h"
#include "tracking_manage.h"

using namespace std;

class TestSmapDebugfs : public ::testing::Test {
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

extern "C" {
int smap_debugfs_migrate_init(void);
void smap_debugfs_mod_exit(void);
};

TEST_F(TestSmapDebugfs, SmapDebugfsMigrateInitDirCreateFailed)
{
    int ret = 0;
    int expectRet = 0;

    ret = smap_debugfs_migrate_init();
    EXPECT_EQ(expectRet, ret);
}

TEST_F(TestSmapDebugfs, SmapDebugfsModExitSuccess)
{
    smap_debugfs_mod_exit();
}
