/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
* Description: SMAP3.0 bus测试代码
*/
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/idr.h>
#include <linux/capability.h>
#include <linux/kstrtox.h>
#include "tracking_private.h"
#include "bus.h"

#include "stub_bus.h"
#include "stub_core.h"

using namespace std;

static struct device *ldev;
static struct device_driver *drv;
static struct tracking_driver *trk_drv;
static struct tracking_dev *trk_dev;
static struct bus_type stub_tracking_bus_type;
static struct tracking_operations stub_tracking_ops;

class GlobalManager {
public:
    static GlobalManager &getInstance()
    {
        static GlobalManager instance;
        return instance;
    }
    void init_bus()
    {
        ldev = (struct device*)kzalloc(sizeof(struct device), GFP_KERNEL);
        ASSERT_NE(nullptr, ldev);
        drv = (struct device_driver*)kzalloc(sizeof(struct device_driver), GFP_KERNEL);
        ASSERT_NE(nullptr, drv);
        trk_drv = (struct tracking_driver*)kzalloc(sizeof(struct tracking_driver), GFP_KERNEL);
        ASSERT_NE(nullptr, trk_drv);
        trk_dev = (struct tracking_dev*)kzalloc(sizeof(struct tracking_dev), GFP_KERNEL);
        ASSERT_NE(nullptr, trk_dev);
        ldev->driver = drv;
    }
    void exit_bus()
    {
        kfree(ldev);
        ldev = NULL;
        kfree(drv);
        drv = NULL;
        kfree(trk_drv);
        trk_drv = NULL;
        kfree(trk_dev);
        trk_dev = NULL;
    }
};

class DriversBusTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        GlobalManager::getInstance().init_bus();
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalManager::getInstance().exit_bus();
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

extern "C" struct tracking_driver *to_tracking_drv(struct device_driver *drv);
TEST_F(DriversBusTest, ToTrackingDrv)
{
    struct tracking_driver *trk_drv_temp;
    drv->name = "ToTrackingDrv";
    trk_drv_temp = to_tracking_drv(drv);
    EXPECT_STREQ("ToTrackingDrv", trk_drv_temp->drv.name);
}

extern "C" struct tracking_dev *to_tracking_dev(struct device *dev);
TEST_F(DriversBusTest, ToTrackingDev)
{
    struct tracking_dev *trk_dev_temp;
    ldev->numa_node = 1;
    trk_dev_temp = to_tracking_dev(ldev);
    EXPECT_EQ(1, trk_dev_temp->tdev.numa_node);
}

extern "C" int tracking_bus_match(struct device *dev, struct device_driver *drv);
TEST_F(DriversBusTest, TrackingBusMatch)
{
    trk_drv->match_always = 1;
    MOCKER(to_tracking_drv).stubs().will(returnValue(trk_drv));
    int ret = tracking_bus_match(ldev, drv);
    EXPECT_EQ(1, ret);

    trk_drv->match_always = 0;
    ret = tracking_bus_match(ldev, drv);
    EXPECT_EQ(0, ret);
}

extern "C" int tracking_bus_probe(struct device *dev);
TEST_F(DriversBusTest, TrackingBusProbe)
{
    int ret;
    trk_drv->probe = stub_tracking_bus_probe;
    MOCKER(to_tracking_drv).stubs().will(returnValue(trk_drv));
    MOCKER(to_tracking_dev).stubs().will(returnValue(trk_dev));
    ret = tracking_bus_probe(ldev);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, trk_dev->target_node);
}

extern "C" int tracking_bus_remove(struct device *dev);
TEST_F(DriversBusTest, TrackingBusRemove)
{
    int ret;
    trk_drv->remove = stub_tracking_device_remove;
    MOCKER(to_tracking_drv).stubs().will(returnValue(trk_drv));
    MOCKER(to_tracking_dev).stubs().will(returnValue(trk_dev));
#if LINUX_VERSION_CODE == KERNEL_VERSION(6, 6, 0)
    tracking_bus_remove(ldev);
#else
    ret = tracking_bus_remove(ldev);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, trk_dev->target_node);
#endif
}

extern "C" int match_always_count;
extern "C" int inner_tracking_driver_register(struct tracking_driver *tracking_drv,
                                              struct module *module, const char *mod_name);
TEST_F(DriversBusTest, TrackingDriverRegister)
{
    int ret;
    struct module *module;
    char *mod_name;
    module = (struct module*)kzalloc(sizeof(struct module), GFP_KERNEL);
    ASSERT_NE(nullptr, module);
    mod_name = (char*)kzalloc(sizeof(char), GFP_KERNEL);
    ASSERT_NE(nullptr, mod_name);
    ret = inner_tracking_driver_register(trk_drv, module, mod_name);
    EXPECT_EQ(0, ret);

    trk_drv->match_always = 2;
    ret = inner_tracking_driver_register(trk_drv, module, mod_name);
    EXPECT_EQ(-EINVAL, ret);
    trk_drv->match_always = 1;
    kfree(module);
    kfree(mod_name);
}

extern "C" void tracking_driver_unregister(struct tracking_driver *tracking_drv);
TEST_F(DriversBusTest, TrackingDriverUnregister)
{
    int pre = match_always_count;
    tracking_driver_unregister(trk_drv);
    EXPECT_EQ(match_always_count, pre - trk_drv->match_always);
}

extern "C" struct tracking_dev *tracking_dev_add(struct device *ldev,
                                                 const struct tracking_operations *ops,
                                                 u8 target_node);
extern "C" void tracking_dev_remove(struct tracking_dev *trk_dev);
TEST_F(DriversBusTest, TrackingDevAdd)
{
    int target_node = 1;
    struct tracking_operations *ops;
    struct tracking_dev *trk_dev_temp;

    trk_dev_temp = tracking_dev_add(ldev, ops, target_node);
    EXPECT_EQ(target_node, trk_dev_temp->target_node);
    tracking_dev_remove(trk_dev_temp);

    trk_dev_temp = tracking_dev_add(NULL, ops, target_node);
    EXPECT_EQ(NULL, trk_dev_temp);

    MOCKER(device_add).stubs().will(returnValue(-1));
    trk_dev_temp = tracking_dev_add(ldev, ops, target_node);
    EXPECT_EQ(NULL, trk_dev_temp);

    MOCKER(kzalloc).stubs().will(returnValue((void*)NULL));
    trk_dev_temp = tracking_dev_add(ldev, ops, target_node);
    EXPECT_EQ(NULL, trk_dev_temp);
}

extern "C" int tracking_bus_init(void);
TEST_F(DriversBusTest, TrackingBusInit)
{
    int ret = tracking_bus_init();
    EXPECT_EQ(0, ret);
    MOCKER(bus_register).stubs().will(returnValue(1));
    ret = tracking_bus_init();
    EXPECT_EQ(1, ret);
}

extern "C" void tracking_bus_exit(void);
TEST_F(DriversBusTest, TrackingBusExit)
{
    tracking_bus_exit;
}