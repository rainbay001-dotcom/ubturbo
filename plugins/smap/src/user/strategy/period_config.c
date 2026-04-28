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
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "smap_user_log.h"
#include "securec.h"
#include "period_config.h"

#define PERIOD_CONFIG_ENTRY 6
#define PERIOD_CONFIG_BUFFSIZE 500

#define RETURN_OK 0
#define RETURN_ERROR (-1)
#define PERIOD_CONFIG_READ_OVER 100

#define MAX_SCAN_PERIOD 20000
#define DEFAULT_SCAN_PERIOD 100
#define MIN_SCAN_PERIOD 50

#define MAX_MIGRATE_PERIOD 20000
#define DEFAULT_MIGRATE_PERIOD 1000
#define MIN_MIGRATE_PERIOD 50

#define MAX_SLOW_THRESHOLD (MAX_MIGRATE_PERIOD / MIN_SCAN_PERIOD)
#define DEFAULT_SLOW_THRESHOLD 2
#define MIN_SLOW_THRESHOLD 0

#define MAX_REMOTE_FREQ_PERCENTILE 100
#define DEFAULT_REMOTE_FREQ_PERCENTILE 99
#define MIN_REMOTE_FREQ_PERCENTILE 1

#define MAX_FREQ_WT 65535
#define DEFAULT_FREQ_WT 0
#define MIN_FREQ_WT 0

#define SCAN_MULTIPLE 5UL

#define RADIX_10 10UL

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

static PeriodConfig g_tmpPeriodConfig;

static PeriodConfig g_periodConfig;

typedef int32_t (*PeriodConfigReadFunc)(char *substr, char *value);

typedef struct {
    char *substr;
    PeriodConfigReadFunc func;
    uint32_t needCfg;
    uint32_t realCfg;
} PeriodConfigReadElem;

uint32_t GetScanPeriodConfig(void)
{
    return g_periodConfig.scanPeriod;
}

uint32_t GetMigratePeriodConfig(void)
{
    return g_periodConfig.migratePeriod;
}

uint32_t GetRemoteFreqPercentileConfig(void)
{
    return g_periodConfig.remoteFreqPercentile;
}

uint32_t GetSlowThresholdConfig(void)
{
    return g_periodConfig.slowThreshold;
}

uint64_t GetFreqWtConfig(void)
{
    return g_periodConfig.freqWt;
}

bool GetFileConfSwitchConfig(void)
{
    return g_periodConfig.fileConfSwitch;
}

bool GetScanPeriodChanged(void)
{
    return g_periodConfig.scanPeriodChanged;
}

bool GetMigratePeriodChanged(void)
{
    return g_periodConfig.migratePeriodChanged;
}

void SetScanPeriodChanged(bool val)
{
    g_periodConfig.scanPeriodChanged = val;
}

void SetMigratePeriodChanged(bool val)
{
    g_periodConfig.migratePeriodChanged = val;
}

static int32_t ConfigReadValueToInt(char *pvalue, uint32_t *resultvalue)
{
    uint64_t result = 0;
    while (*pvalue != '\0') {
        if (*pvalue >= '0' && *pvalue <= '9') {
            int digit = *pvalue - '0';
            result = RADIX_10 * result + digit;
            if (result > UINT32_MAX) {
                return RETURN_ERROR;
            }
            pvalue++;
        } else {
            return RETURN_ERROR;
        }
    }
    *resultvalue = result;
    return RETURN_OK;
}

static int32_t ConfigScanPeriod(char *substr, char *value)
{
    SMAP_LOGGER_DEBUG("Read config key:%s, value:%s.", substr, value);
    int32_t ret = ConfigReadValueToInt(value, &g_tmpPeriodConfig.scanPeriod);
    if (ret != RETURN_OK) {
        SMAP_LOGGER_ERROR("Config scan period read failed, key:%s.", substr);
        return ret;
    }
    if (g_tmpPeriodConfig.scanPeriod < MIN_SCAN_PERIOD || g_tmpPeriodConfig.scanPeriod > MAX_SCAN_PERIOD) {
        SMAP_LOGGER_ERROR("Config scan period(%d) invalid, range(%d-%d), key:%s.", g_tmpPeriodConfig.scanPeriod,
                          MIN_SCAN_PERIOD, MAX_SCAN_PERIOD, substr);
        return RETURN_ERROR;
    }
    if (g_tmpPeriodConfig.scanPeriod % SCAN_MULTIPLE != 0) {
        SMAP_LOGGER_ERROR("Scan period(%d) must be a multiple of %d, key:%s.", g_tmpPeriodConfig.scanPeriod,
                          SCAN_MULTIPLE, substr);
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static int32_t ConfigMigratePeriod(char *substr, char *value)
{
    SMAP_LOGGER_DEBUG("Read config key:%s, value:%s.", substr, value);
    int32_t ret = ConfigReadValueToInt(value, &g_tmpPeriodConfig.migratePeriod);
    if (ret != RETURN_OK) {
        SMAP_LOGGER_ERROR("Config migrate period read failed, key:%s.", substr);
        return ret;
    }
    if (g_tmpPeriodConfig.migratePeriod < MIN_MIGRATE_PERIOD || g_tmpPeriodConfig.migratePeriod > MAX_MIGRATE_PERIOD) {
        SMAP_LOGGER_ERROR("Config migrate period(%d) invalid, range(%d-%d), key:%s.", g_tmpPeriodConfig.migratePeriod,
                          MIN_MIGRATE_PERIOD, MAX_MIGRATE_PERIOD, substr);
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static int32_t ConfigRemoteFreqPercentile(char *substr, char *value)
{
    SMAP_LOGGER_DEBUG("Read config key:%s, value:%s.", substr, value);
    int32_t ret = ConfigReadValueToInt(value, &g_tmpPeriodConfig.remoteFreqPercentile);
    if (ret != RETURN_OK) {
        SMAP_LOGGER_ERROR("Config remote freq percentile read failed, key:%s.", substr);
        return ret;
    }
    if (g_tmpPeriodConfig.remoteFreqPercentile < MIN_REMOTE_FREQ_PERCENTILE ||
        g_tmpPeriodConfig.remoteFreqPercentile > MAX_REMOTE_FREQ_PERCENTILE) {
        SMAP_LOGGER_ERROR("Config remote freq percentile(%d) invalid, range(%d-%d), key:%s.",
                          g_tmpPeriodConfig.remoteFreqPercentile, MIN_REMOTE_FREQ_PERCENTILE,
                          MAX_REMOTE_FREQ_PERCENTILE, substr);
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static int32_t ConfigSlowThreshold(char *substr, char *value)
{
    SMAP_LOGGER_DEBUG("Read config key:%s, value:%s.", substr, value);
    int32_t ret = ConfigReadValueToInt(value, &g_tmpPeriodConfig.slowThreshold);
    if (ret != RETURN_OK) {
        SMAP_LOGGER_ERROR("Config slow threshold read failed, key:%s.", substr);
        return ret;
    }
    if (g_tmpPeriodConfig.slowThreshold < MIN_SLOW_THRESHOLD || g_tmpPeriodConfig.slowThreshold > MAX_SLOW_THRESHOLD) {
        SMAP_LOGGER_ERROR("Config slow threshold(%d) invalid, range(%d-%d), key:%s.", g_tmpPeriodConfig.slowThreshold,
                          MIN_SLOW_THRESHOLD, MAX_SLOW_THRESHOLD, substr);
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static int32_t ConfigFreqWt(char *substr, char *value)
{
    SMAP_LOGGER_DEBUG("Read config key:%s, value:%s.", substr, value);
    uint32_t tempFreqWt = 0;
    int32_t ret = ConfigReadValueToInt(value, &tempFreqWt);
    if (ret != RETURN_OK) {
        SMAP_LOGGER_ERROR("Config freq wt read failed, key:%s.", substr);
        return ret;
    }
    g_tmpPeriodConfig.freqWt = tempFreqWt;
    bool isTooLow = (MIN_FREQ_WT > 0 && g_tmpPeriodConfig.freqWt < MIN_FREQ_WT);
    if (isTooLow || g_tmpPeriodConfig.freqWt > MAX_FREQ_WT) {
        SMAP_LOGGER_ERROR("Config freq wt(%d) invalid, range(%d-%d), key:%s.", g_tmpPeriodConfig.freqWt, MIN_FREQ_WT,
                          MAX_FREQ_WT, substr);
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static int32_t ConfigFileConfSwitch(char *substr, char *value)
{
    SMAP_LOGGER_DEBUG("Read config key:%s, value:%s.", substr, value);
    if (strcmp(value, "true") == 0) {
        g_tmpPeriodConfig.fileConfSwitch = true;
    } else if (strcmp(value, "false") == 0) {
        g_tmpPeriodConfig.fileConfSwitch = false;
    } else {
        SMAP_LOGGER_ERROR("Config file conf switch(%s) failed, need config true or false, key:%s.", value, substr);
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static PeriodConfigReadElem g_periodConfigRead[] = {
    {
        "smap.scan.period",
        ConfigScanPeriod,
        1UL,
        0UL,
    },
    {
        "smap.migrate.period",
        ConfigMigratePeriod,
        1UL,
        0UL,
    },
    {
        "smap.remote.freq.percentile",
        ConfigRemoteFreqPercentile,
        1UL,
        0UL,
    },
    {
        "smap.slow.threshold",
        ConfigSlowThreshold,
        1UL,
        0UL,
    },
    {
        "smap.freq.wt",
        ConfigFreqWt,
        1UL,
        0UL,
    },
    {
        "smap.period.file.config.switch",
        ConfigFileConfSwitch,
        1UL,
        0UL,
    },
};

static void ConfigReadTrim(char *str)
{
    size_t len = strlen(str);

    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[len - 1] = '\0';
        len--;
    }
}

static char *ConfigReadSearchSubString(const char *buf, const char *substr)
{
    char *ret = NULL;
    bool isMatch = false;

    if (!(buf && substr)) {
        return NULL;
    }

    ret = strstr((char *)buf, substr);
    if (ret != buf) {
        return NULL;
    }

    ret += strlen(substr);
    while (*ret == ' ' || *ret == '=' || *ret == '\n' || *ret == '\r') {
        isMatch = true;
        ret++;
    }
    if (!isMatch || strlen(ret) == 0) {
        return NULL;
    }
    ConfigReadTrim(ret);
    return ret;
}

static int PeriodConfigAnalyze(const char *str)
{
    size_t len = sizeof(g_periodConfigRead) / sizeof(PeriodConfigReadElem);

    for (size_t index = 0; index < len; index++) {
        char *config = g_periodConfigRead[index].substr;
        char *substr = ConfigReadSearchSubString(str, config);
        if (substr != NULL) {
            g_periodConfigRead[index].realCfg = 1;
            return g_periodConfigRead[index].func(config, substr);
        }
    }
    return 0;
}

static int32_t ConfigReadByLine(FILE *fp, char *buf, int32_t n)
{
    char *ret = (char *)fgets(buf, n, fp);

    if (ret) {
        return RETURN_OK;
    }
    if (ferror(fp)) {
        return RETURN_ERROR;
    }
    return PERIOD_CONFIG_READ_OVER;
}

static int ConfigReadReadFile(const char *filepath)
{
    FILE *fp = NULL;
    char buf[PERIOD_CONFIG_BUFFSIZE] = { 0 };
    int ret = RETURN_OK;
    char path[PATH_MAX + 1] = { 0 };

    if (strlen(filepath) > PATH_MAX || realpath(filepath, path) == NULL) {
        SMAP_LOGGER_ERROR("Read config path failed.");
        return RETURN_ERROR;
    }
    fp = fopen(path, "r");
    if (!fp) {
        SMAP_LOGGER_ERROR("Failed to open config file: %s.", path);
        return RETURN_ERROR;
    }

    while (true) {
        ret = memset_s(buf, PERIOD_CONFIG_BUFFSIZE, 0, PERIOD_CONFIG_BUFFSIZE);
        if (ret != 0) {
            break;
        }
        ret = ConfigReadByLine(fp, buf, PERIOD_CONFIG_BUFFSIZE);
        if (ret == PERIOD_CONFIG_READ_OVER) {
            SMAP_LOGGER_INFO("Read config over.");
            ret = RETURN_OK;
            break;
        } else if (ret == RETURN_ERROR) {
            SMAP_LOGGER_ERROR("Read config failed.");
            break;
        }
        ret = PeriodConfigAnalyze(buf);
        if (ret != 0) {
            SMAP_LOGGER_ERROR("Analyze config failed(%s), str(%s).", filepath, (char *)buf);
            break;
        }
    }

    (void)fclose(fp);
    return ret;
}

static int32_t PeriodConfigReview(void)
{
    size_t num = sizeof(g_periodConfigRead) / sizeof(PeriodConfigReadElem);
    size_t index;

    for (index = 0; index < num; index++) {
        if (g_periodConfigRead[index].needCfg == 1UL && g_periodConfigRead[index].realCfg == 0UL) {
            SMAP_LOGGER_ERROR("Read config key:%s, failed.", g_periodConfigRead[index].substr);
            return RETURN_ERROR;
        }
    }
    if (g_tmpPeriodConfig.scanPeriod > g_tmpPeriodConfig.migratePeriod) {
        SMAP_LOGGER_ERROR("Scan period(%d) must be less than migrate period(%d).", g_tmpPeriodConfig.scanPeriod,
                          g_tmpPeriodConfig.migratePeriod);
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static void PeriodConifgReset(void)
{
    size_t num = sizeof(g_periodConfigRead) / sizeof(PeriodConfigReadElem);
    size_t index;

    for (index = 0; index < num; index++) {
        g_periodConfigRead[index].realCfg = 0;
    }
}

static void InitPeriodConfig(void)
{
    g_periodConfig.scanPeriod = DEFAULT_SCAN_PERIOD;
    g_periodConfig.migratePeriod = DEFAULT_MIGRATE_PERIOD;
    g_periodConfig.remoteFreqPercentile = DEFAULT_REMOTE_FREQ_PERCENTILE;
    g_periodConfig.slowThreshold = DEFAULT_SLOW_THRESHOLD;
    g_periodConfig.freqWt = DEFAULT_FREQ_WT;
    g_periodConfig.fileConfSwitch = false;
    g_periodConfig.scanPeriodChanged = false;
    g_periodConfig.migratePeriodChanged = false;
}

static int32_t EnsureDirectoryExists(const char *dirPath)
{
    struct stat st;
    if (stat(dirPath, &st) == 0) {
        // 目录已存在
        if (S_ISDIR(st.st_mode)) {
            return RETURN_OK; // 是一个目录，返回成功
        } else {
            SMAP_LOGGER_ERROR("Error: %s exists but is not a directory.", dirPath);
            return RETURN_ERROR; // 路径存在但不是目录
        }
    }

    if (mkdir(dirPath, 0755) != 0) { // 0755 是权限设置
        SMAP_LOGGER_ERROR("Error creating directory.");
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

static int32_t InitPeriodConfigFileBuffer(char periodDefaultConfig[PERIOD_CONFIG_ENTRY][PERIOD_CONFIG_BUFFSIZE])
{
    int32_t ret;

    // 定义配置项的结构体数组
    struct ConfigEntry {
        const char *format;
        int value;
    } configs[] = {
        { "smap.scan.period = %d\n", DEFAULT_SCAN_PERIOD },
        { "smap.migrate.period = %d\n", DEFAULT_MIGRATE_PERIOD },
        { "smap.remote.freq.percentile = %d\n", DEFAULT_REMOTE_FREQ_PERCENTILE },
        { "smap.slow.threshold = %d\n", DEFAULT_SLOW_THRESHOLD },
        { "smap.freq.wt = %d\n", DEFAULT_FREQ_WT },
    };
    size_t numConfigs = sizeof(configs) / sizeof(configs[0]);

    // 使用循环处理snprintf_s部分
    for (size_t i = 0; i < numConfigs; i++) {
        ret = snprintf_s(periodDefaultConfig[i], PERIOD_CONFIG_BUFFSIZE, PERIOD_CONFIG_BUFFSIZE - 1, configs[i].format,
                         configs[i].value);
        if (ret < 0) {
            SMAP_LOGGER_ERROR("Snprintf failed for config index %zu.", i);
            return RETURN_ERROR;
        }
    }
    const char *switchConfigStr = "smap.period.file.config.switch = false\n";
    size_t configStrLen = strlen(switchConfigStr);
    errno_t res = strncpy_s(periodDefaultConfig[numConfigs], PERIOD_CONFIG_BUFFSIZE, switchConfigStr, configStrLen);
    if (res != EOK) {
        SMAP_LOGGER_ERROR("Strncpy smap period switch failed.");
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

int32_t GeneratePeriodConfigFile(const char *configFile)
{
    InitPeriodConfig();
    char periodDefaultConfig[PERIOD_CONFIG_ENTRY][PERIOD_CONFIG_BUFFSIZE];
    int32_t res = InitPeriodConfigFileBuffer(periodDefaultConfig);
    if (res != RETURN_OK) {
        return res;
    }

    if (strlen(configFile) > PATH_MAX) {
        SMAP_LOGGER_ERROR("Config period file path too long.");
        return RETURN_ERROR;
    }

    // 提取目录路径
    char dirPath[PATH_MAX + 1] = { 0 };
    errno_t ret = strncpy_s(dirPath, sizeof(dirPath), configFile, strlen(configFile));
    if (ret != EOK) {
        SMAP_LOGGER_ERROR("strncpy_s failed, ret(%d).", ret);
        return RETURN_ERROR;
    }
    char *lastSlash = strrchr(dirPath, '/');
    if (lastSlash == NULL) {
        SMAP_LOGGER_ERROR("Invalid config file path.");
        return RETURN_ERROR;
    }
    *lastSlash = '\0';

    if (EnsureDirectoryExists(dirPath) != RETURN_OK) { // 确保目录存在
        SMAP_LOGGER_ERROR("Failed to ensure directory exists: %s.", dirPath);
        return RETURN_ERROR;
    }

    if (access(configFile, F_OK) == 0) {
        return RETURN_OK;
    }

    int fd = open(configFile, (O_WRONLY | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR | S_IRGRP));
    if (fd < 0) {
        SMAP_LOGGER_ERROR("Open file %s error: %s.", configFile, strerror(errno));
        return RETURN_ERROR;
    }

    size_t arraySize = sizeof(periodDefaultConfig) / sizeof(periodDefaultConfig[0]);
    for (size_t i = 0; i < arraySize; i++) {
        ret = dprintf(fd, periodDefaultConfig[i]);
        if (ret < 0) {
            SMAP_LOGGER_ERROR("Dprintf file %s error: %s.", configFile, strerror(errno));
            (void)close(fd);
            return RETURN_ERROR;
        }
    }

    (void)close(fd);
    return RETURN_OK;
}

static bool UpdatePeriodConfigChanged(void)
{
    uint32_t oldScanPeriod, oldMigratePeriod, scanPeriod, migratePeriod;
    uint32_t oldRemoteFreqPercentile, oldSlowThreshold, remoteFreqPercentile, slowThreshold;
    uint64_t oldFreqWt, freqWt;

    if (!g_tmpPeriodConfig.fileConfSwitch) {
        return false;
    }

    oldScanPeriod = g_periodConfig.scanPeriod;
    oldMigratePeriod = g_periodConfig.migratePeriod;
    oldRemoteFreqPercentile = g_periodConfig.remoteFreqPercentile;
    oldSlowThreshold = g_periodConfig.slowThreshold;
    oldFreqWt = g_periodConfig.freqWt;

    scanPeriod = g_tmpPeriodConfig.scanPeriod;
    migratePeriod = g_tmpPeriodConfig.migratePeriod;
    remoteFreqPercentile = g_tmpPeriodConfig.remoteFreqPercentile;
    slowThreshold = g_tmpPeriodConfig.slowThreshold;
    freqWt = g_tmpPeriodConfig.freqWt;

    if (oldScanPeriod == scanPeriod && oldMigratePeriod == migratePeriod &&
        oldRemoteFreqPercentile == remoteFreqPercentile && oldSlowThreshold == slowThreshold && oldFreqWt == freqWt) {
        return false;
    }

    if (oldScanPeriod != scanPeriod) {
        g_tmpPeriodConfig.scanPeriodChanged = true;
    }

    if (oldMigratePeriod != migratePeriod) {
        g_tmpPeriodConfig.migratePeriodChanged = true;
    }

    if (oldRemoteFreqPercentile != remoteFreqPercentile) {
        SMAP_LOGGER_INFO("Start update remote freq percentile from config to %u.", remoteFreqPercentile);
    }

    if (oldSlowThreshold != slowThreshold) {
        SMAP_LOGGER_INFO("Start update slow threshold from config to %u.", slowThreshold);
    }

    if (oldFreqWt != freqWt) {
        SMAP_LOGGER_INFO("Start update freq wt from config to %lu.", freqWt);
    }

    return true;
}

void PeriodConfigRead(const char *configFile)
{
    int ret = ConfigReadReadFile(configFile);
    if (ret != RETURN_OK) {
        PeriodConifgReset();
        SMAP_LOGGER_ERROR("Read period config failed, ret(%d).", ret);
        return;
    }

    ret = PeriodConfigReview();
    if (ret != RETURN_OK) {
        PeriodConifgReset();
        SMAP_LOGGER_ERROR("Review period config failed, ret(%d).", ret);
        return;
    }
    if (UpdatePeriodConfigChanged()) {
        g_periodConfig = g_tmpPeriodConfig;
    }
    PeriodConifgReset();
    SMAP_LOGGER_DEBUG("Scan: %d, Migrate: %d, FileConfSwitch: %d.", g_periodConfig.scanPeriod,
                      g_periodConfig.migratePeriod, g_periodConfig.fileConfSwitch);
    return;
}
