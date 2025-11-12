/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: smap5.0 user migration ut code
 */

#include <cstdlib>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "smap_user_log.h"
#include "securec.h"
#include "strategy/period_config.h"

using namespace std;

class PeriodConfigTest : public ::testing::Test {
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

constexpr int RETURN_ERROR = -1;

using PeriodConfigReadFunc = int32_t (*)(char*, char*);

typedef struct {
    uint32_t scanPeriod;
    uint32_t migratePeriod;
    uint32_t remoteFreqPercentile;
    uint32_t slowThreshold;
    uint64_t freqWt;
    bool fileConfSwitch;
    bool scanPeriodChanged;
    bool migratePeriodChanged;
} PeriodConfig;

typedef struct {
    char *substr;
    PeriodConfigReadFunc func;
    uint32_t needCfg;
    uint32_t realCfg;
} PeriodConfigReadElem;

extern "C" PeriodConfig g_tmpPeriodConfig;
extern "C" PeriodConfig g_periodConfig;
extern "C" PeriodConfigReadElem g_periodConfigRead[];

extern "C" uint32_t GetScanPeriodConfig(void);
TEST_F(PeriodConfigTest, GetScanPeriodConfigTest)
{
    g_periodConfig.scanPeriod = 1;
    uint32_t ret = GetScanPeriodConfig();
    EXPECT_EQ(1, ret);
}

extern "C" uint32_t GetMigratePeriodConfig(void);
TEST_F(PeriodConfigTest, GetMigratePeriodConfigTest)
{
    g_periodConfig.migratePeriod = 2;
    uint32_t ret = GetMigratePeriodConfig();
    EXPECT_EQ(2, ret);
}

extern "C" bool GetFileConfSwitchConfig(void);
TEST_F(PeriodConfigTest, GetFileConfSwitchConfigTest)
{
    g_periodConfig.fileConfSwitch = true;
    bool ret = GetFileConfSwitchConfig();
    EXPECT_EQ(true, ret);
}

extern "C" bool GetScanPeriodChanged(void);
TEST_F(PeriodConfigTest, GetScanPeriodChangedTest)
{
    g_periodConfig.scanPeriodChanged = true;
    bool ret = GetScanPeriodChanged();
    EXPECT_EQ(true, ret);
}

extern "C" bool GetMigratePeriodChanged(void);
TEST_F(PeriodConfigTest, GetMigratePeriodChangedTest)
{
    g_periodConfig.migratePeriodChanged = true;
    bool ret = GetMigratePeriodChanged();
    EXPECT_EQ(true, ret);
}

extern "C" void SetScanPeriodChanged(bool val);
TEST_F(PeriodConfigTest, SetScanPeriodChangedTest)
{
    g_periodConfig.scanPeriodChanged = true;
    SetScanPeriodChanged(false);
    EXPECT_EQ(false, g_periodConfig.scanPeriodChanged);
}

extern "C" void SetMigratePeriodChanged(bool val);
TEST_F(PeriodConfigTest, SetMigratePeriodChangedTest)
{
    g_periodConfig.migratePeriodChanged = false;
    SetMigratePeriodChanged(true);
    EXPECT_EQ(true, g_periodConfig.migratePeriodChanged);
}

extern "C" int32_t ConfigReadValueToInt(char *pvalue, uint32_t *resultvalue);
TEST_F(PeriodConfigTest, ConfigReadValueToIntTest)
{
    char* pvalue = "123";
    uint32_t resultvalue = 0;
    int32_t ret = ConfigReadValueToInt(pvalue, &resultvalue);
    EXPECT_EQ(123, resultvalue);
    EXPECT_EQ(0, ret);

    char* pvalue2 = "abc";
    uint32_t resultvalue2 = 0;
    ret = ConfigReadValueToInt(pvalue2, &resultvalue2);
    EXPECT_EQ(RETURN_ERROR, ret);
}

extern "C" int32_t ConfigScanPeriod(char *substr, char *value);
TEST_F(PeriodConfigTest, ConfigScanPeriodTest)
{
    char* substr = "scanPeriod";
    char* value = "123";
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(-1));
    int32_t ret = ConfigScanPeriod(substr, value);
    EXPECT_EQ(-1, ret);

    char* substr2 = "scanPeriod";
    char* value2 = "123";
    g_tmpPeriodConfig.scanPeriod = 0;
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(0));
    ret = ConfigScanPeriod(substr2, value2);
    EXPECT_EQ(RETURN_ERROR, ret);

    g_tmpPeriodConfig.scanPeriod = 0;
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(0));
    ret = ConfigScanPeriod(substr2, value2);
    EXPECT_EQ(RETURN_ERROR, ret);
}

TEST_F(PeriodConfigTest, ConfigScanPeriodTestSuccess)
{
    int ret;
    char* substr2 = "scanPeriod";
    char* value2 = "123";
    g_tmpPeriodConfig.scanPeriod = 100UL;
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(0));
    ret = ConfigScanPeriod(substr2, value2);
    EXPECT_EQ(0, ret);
}

extern "C" int32_t ConfigMigratePeriod(char *substr, char *value);
TEST_F(PeriodConfigTest, ConfigMigratePeriodTest)
{
    char* substr = "migratePeriod";
    char* value = "123";
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(-1));
    int32_t ret = ConfigMigratePeriod(substr, value);
    EXPECT_EQ(-1, ret);

    g_tmpPeriodConfig.migratePeriod = 0;
    GlobalMockObject::verify();
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(0));
    ret = ConfigMigratePeriod(substr, value);
    EXPECT_EQ(RETURN_ERROR, ret);

    g_tmpPeriodConfig.migratePeriod = 1000;
    GlobalMockObject::verify();
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(0));
    ret = ConfigMigratePeriod(substr, value);
    EXPECT_EQ(0, ret);
}

extern "C" int32_t ConfigRemoteFreqPercentile(char *substr, char *value);
TEST_F(PeriodConfigTest, ConfigRemoteFreqPercentileTest)
{
    char *substr = "smap.remote.freq.percentile";
    char *value = "101";
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(-1));
    int32_t ret = ConfigRemoteFreqPercentile(substr, value);
    EXPECT_EQ(-1, ret);

    g_tmpPeriodConfig.remoteFreqPercentile = 0;
    GlobalMockObject::verify();
    ret = ConfigRemoteFreqPercentile(substr, value);
    EXPECT_EQ(RETURN_ERROR, ret);
    EXPECT_EQ(101, g_tmpPeriodConfig.remoteFreqPercentile);

    GlobalMockObject::verify();
    char *value1 = "100";
    ret = ConfigRemoteFreqPercentile(substr, value1);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(100, g_tmpPeriodConfig.remoteFreqPercentile);
}

extern "C" int32_t ConfigSlowThreshold(char *substr, char *value);
TEST_F(PeriodConfigTest, ConfigSlowThresholdTest)
{
    char *substr = "smap.slow.threshold";
    char *value = "41";
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(-1));
    int32_t ret = ConfigSlowThreshold(substr, value);
    EXPECT_EQ(-1, ret);

    g_tmpPeriodConfig.slowThreshold = 0;
    GlobalMockObject::verify();
    ret = ConfigSlowThreshold(substr, value);
    EXPECT_EQ(RETURN_ERROR, ret);
    EXPECT_EQ(41, g_tmpPeriodConfig.slowThreshold);

    GlobalMockObject::verify();
    char *value1 = "40";
    ret = ConfigSlowThreshold(substr, value1);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(40, g_tmpPeriodConfig.slowThreshold);
}

extern "C" int32_t ConfigFreqWt(char *substr, char *value);
TEST_F(PeriodConfigTest, ConfigFreqWtTest)
{
    char *substr = "smap.freq.wt";
    char *value = "65536";
    MOCKER(ConfigReadValueToInt).stubs().will(returnValue(-1));
    int32_t ret = ConfigFreqWt(substr, value);
    EXPECT_EQ(-1, ret);

    g_tmpPeriodConfig.freqWt = 0;
    GlobalMockObject::verify();
    ret = ConfigFreqWt(substr, value);
    EXPECT_EQ(RETURN_ERROR, ret);
    EXPECT_EQ(65536, g_tmpPeriodConfig.freqWt);

    GlobalMockObject::verify();
    char *value1 = "40";
    ret = ConfigFreqWt(substr, value1);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(40, g_tmpPeriodConfig.freqWt);
}

extern "C" int32_t ConfigFileConfSwitch(char *substr, char *value);
TEST_F(PeriodConfigTest, ConfigFileConfSwitchTest)
{
    char* substr = "test";
    char* value1 = "true";
    g_tmpPeriodConfig.fileConfSwitch = false;
    int32_t ret = ConfigFileConfSwitch(substr, value1);
    EXPECT_EQ(true, g_tmpPeriodConfig.fileConfSwitch);

    char* value2 = "false";
    g_tmpPeriodConfig.fileConfSwitch = true;
    ret = ConfigFileConfSwitch(substr, value2);
    EXPECT_EQ(false, g_tmpPeriodConfig.fileConfSwitch);

    char* value3 = "test";
    ret = ConfigFileConfSwitch(substr, value3);
    EXPECT_EQ(-1, ret);
}

extern "C" void ConfigReadTrim(char *str);
TEST_F(PeriodConfigTest, ConfigReadTrimTest)
{
    char str1[] = "123test\n";
    ConfigReadTrim(str1);
    EXPECT_EQ('1', str1[0]);
    EXPECT_EQ('3', str1[2]);
}

extern "C" char *ConfigReadSearchSubString(const char *buf, const char *substr);
TEST_F(PeriodConfigTest, ConfigReadSearchSubStringTest)
{
    char buf[20] = "smap = 2\n";
    const char* substr = "smap";
    char* res = nullptr;
    res = ConfigReadSearchSubString(buf, nullptr);
    EXPECT_EQ(nullptr, res);

    res = ConfigReadSearchSubString(buf, substr);
    EXPECT_EQ('2', res[0]);
}

extern "C" int PeriodConfigAnalyze(const char *str);
TEST_F(PeriodConfigTest, PeriodConfigAnalyzeTest)
{
    char *str = "smap";
    MOCKER(ConfigReadSearchSubString).stubs().will(returnValue(static_cast<char *>(nullptr)));
    uint32_t ret = PeriodConfigAnalyze(str);
    EXPECT_EQ(0, ret);
}

extern "C" int32_t ConfigReadByLine(FILE *fptemp, char *buf, int32_t readLine);
TEST_F(PeriodConfigTest, ConfigReadByLineTest)
{
    FILE *fptemp = (FILE *)malloc(sizeof(FILE));
    char *buf = (char *)malloc(1024);
    int32_t readLine = 1;

    MOCKER(fgets).stubs().will(returnValue(buf));
    int32_t ret = ConfigReadByLine(fptemp, buf, readLine);
    EXPECT_EQ(0, ret);

    free(buf);
    free(fptemp);
}

extern "C" int ConfigReadReadFile(const char *configFile);
extern "C" int32_t PeriodConfigReview(void);
TEST_F(PeriodConfigTest, PeriodConfigReviewTest)
{
    g_periodConfigRead[0].needCfg = 1UL;
    g_periodConfigRead[0].realCfg = 0UL;
    int32_t ret = PeriodConfigReview();
    EXPECT_EQ(-1, ret);

    uint32_t num = 6;
    for (int i = 0; i < num; i++) {
        g_periodConfigRead[i].needCfg = 2UL;
        g_periodConfigRead[i].realCfg = 2UL;
    }
    g_tmpPeriodConfig.scanPeriod = 1000;
    g_tmpPeriodConfig.migratePeriod = 500;
    ret = PeriodConfigReview();
    EXPECT_EQ(-1, ret);

    g_tmpPeriodConfig.scanPeriod = 500;
    g_tmpPeriodConfig.migratePeriod = 1000;
    ret = PeriodConfigReview();
    EXPECT_EQ(0, ret);
}

extern "C" void PeriodConifgReset(void);
TEST_F(PeriodConfigTest, PeriodConifgResetTest)
{
    for (int i = 0; i < 3; i++) {
        g_periodConfigRead[i].realCfg = 1;
    }
    PeriodConifgReset();
    EXPECT_EQ(0, g_periodConfigRead[0].realCfg);
}

extern "C" void InitPeriodConfig(void);
TEST_F(PeriodConfigTest, InitPeriodConfigTest)
{
    g_periodConfig.scanPeriod = 10;
    g_periodConfig.migratePeriod = 10;
    g_periodConfig.fileConfSwitch = true;
    g_periodConfig.scanPeriodChanged = true;
    g_periodConfig.migratePeriodChanged = true;

    InitPeriodConfig();
    EXPECT_EQ(100, g_periodConfig.scanPeriod);
    EXPECT_EQ(1000, g_periodConfig.migratePeriod);
    EXPECT_EQ(false, g_periodConfig.fileConfSwitch);
    EXPECT_EQ(false, g_periodConfig.scanPeriodChanged);
    EXPECT_EQ(false, g_periodConfig.migratePeriodChanged);
}

extern "C" int32_t EnsureDirectoryExists(const char *dirPath);
TEST_F(PeriodConfigTest, TestEnsureDirectoryExists)
{
    const char* dir = "/home/test";
    MOCKER(stat).stubs().will(returnValue(1));
    MOCKER(mkdir).stubs().will(returnValue(1));
    int32_t ret = EnsureDirectoryExists(dir);

    EXPECT_EQ(-1, ret);
}

extern "C" bool UpdatePeriodConfigChanged(void);
TEST_F(PeriodConfigTest, TestUpdatePeriodConfigChanged)
{
    g_tmpPeriodConfig.fileConfSwitch = true;
    g_periodConfig.scanPeriod = 500;
    g_periodConfig.migratePeriod = 1000;
    g_tmpPeriodConfig.scanPeriod = 1500;
    g_tmpPeriodConfig.migratePeriod = 2000;

    bool ret = UpdatePeriodConfigChanged();
    EXPECT_EQ(true, ret);
}

extern "C" void PeriodConfigRead(const char *configFile);
TEST_F(PeriodConfigTest, PeriodConfigReadTestReadFileFailed)
{
    const char *configFile = "/test";
    MOCKER(ConfigReadReadFile).stubs().will(returnValue(-1));
    MOCKER(PeriodConifgReset).stubs().will(ignoreReturnValue());

    PeriodConfigRead(configFile);
}

TEST_F(PeriodConfigTest, PeriodConfigReadTestConfigReviewFailed)
{
    const char *configFile = "/test";
    MOCKER(ConfigReadReadFile).stubs().will(returnValue(0));
    MOCKER(PeriodConfigReview).stubs().will(returnValue(-1));
    MOCKER(PeriodConifgReset).stubs().will(ignoreReturnValue());

    PeriodConfigRead(configFile);
}

TEST_F(PeriodConfigTest, PeriodConfigReadTestSuccess)
{
    const char *configFile = "/test";
    MOCKER(ConfigReadReadFile).stubs().will(returnValue(0));
    MOCKER(PeriodConfigReview).stubs().will(returnValue(0));
    MOCKER(UpdatePeriodConfigChanged).stubs().will(returnValue(false));
    MOCKER(PeriodConifgReset).stubs().will(ignoreReturnValue());

    PeriodConfigRead(configFile);
}
