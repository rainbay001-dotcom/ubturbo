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
#include <errno.h>
#include <dlfcn.h>
#include <libvirt/libvirt.h>

#include "securec.h"
#include "manage.h"
#include "smap_user_log.h"
#include "smap_env.h"

#define DOMAIN_KEYWORD "domain"
#define HYP_ID_DASH_CNT 1
#define HYP_ID_LEN 11
#define CHAR_DASH '-'
#define CHAR_SLASH '/'
#define QEMU_SYSTEM_URI "qemu:///system"
#define LIB_PATH "libvirt.so.0"

void *g_virtHandler = NULL;
virConnectPtr (*g_virConnectOpen)(const char *name);
int (*g_virConnectClose)(virConnectPtr conn);
int (*g_virConnectIsAlive)(virConnectPtr conn);
virDomainPtr (*g_virDomainLookupByID)(virConnectPtr conn, int id);
int (*g_virDomainGetInfo)(virDomainPtr domain, virDomainInfoPtr info);
int (*g_virDomainFree)(virDomainPtr domain);
int (*g_virDomainGetVcpus)(virDomainPtr domain, virVcpuInfoPtr info, int maxinfo, unsigned char *cpumaps, int maplen);
char *(*g_virDomainGetXMLDesc)(virDomainPtr domain, unsigned int flags);
int OpenVirConn(virConnectPtr *virConn);

/*
 * Extract VM domain ID from cmdline
 *
 * @cmdline The content of /proc/[vm_pid]/cmdline
 * @id      The id to store domain ID
 *
 * e.g. extract 3 from string "domain-3-vm_4u8g_qcow2"
 */
static int ExtractIdFromCmdline(char *cmdline, int len, int *id)
{
    int i, j;
    char domainId[HYP_ID_LEN] = { 0 };
    char *tmp = cmdline;

    if (!tmp || !id) {
        return -EINVAL;
    }
    if (len > CMDLINE_LEN) {
        return -EINVAL;
    }
    while (!strstr(tmp, DOMAIN_KEYWORD)) {
        tmp = strchr(tmp, '\0');
        tmp++;
    }
    tmp = strstr(tmp, DOMAIN_KEYWORD);
    tmp += strlen(DOMAIN_KEYWORD) + HYP_ID_DASH_CNT;
    for (i = 0, j = 0; i < strlen(tmp) && tmp[i] != CHAR_DASH; i++) {
        domainId[j++] = tmp[i];
        if (j >= HYP_ID_LEN) {
            return -ERANGE;
        }
    }
    domainId[j] = '\0';
    if (j == 0) {
        return -ENODATA;
    }
    return (sscanf_s(domainId, "%d", id) <= 0) ? -EINVAL : 0;
}

int ReadDomainIdByPid(pid_t pid, int *id)
{
    char cmdline[CMDLINE_LEN] = { 0 };
    char *tmp;
    char cmdBuf[BUFFER_SIZE];
    int ret = snprintf_s(cmdBuf, sizeof(cmdBuf), sizeof(cmdBuf) - 1, "%s %d cmdline %s", CAT_SCRIPT_CAT_PATH, pid,
                         CAT_SCRIPT_TAIL);
    if (ret == -1) {
        SMAP_LOGGER_ERROR("Make pid %d cmdline error.");
        return ret;
    }
    SMAP_LOGGER_INFO("Before open cmdline file");
    FILE *file = popen(cmdBuf, "r");
    if (!file) {
        SMAP_LOGGER_ERROR("Open pid %d cmdline error: %d.", errno);
        return -errno;
    }
    if (fgets(cmdline, sizeof(cmdline), file)) {
        SMAP_LOGGER_DEBUG("Skip the first line of fileContent.");
    }
    if (fgets(cmdline, sizeof(cmdline), file)) {
        ret = ExtractIdFromCmdline(cmdline, sizeof(cmdline), id);
    }
    SMAP_LOGGER_INFO("After fgets cmdline file, ret=%d", ret);
    int err = pclose(file);
    if (err) {
        SMAP_LOGGER_WARNING("fclose error: %d.", errno);
    }
    return ret;
}

int CheckVirConn(virConnectPtr *virConn)
{
    int ret;
    if (*virConn) {
        ret = g_virConnectIsAlive(*virConn);
        // conn alive
        if (ret == 1) {
            SMAP_LOGGER_DEBUG("vir alive.");
            return 0;
        }
        // conn dead
        if (ret == 0) {
            SMAP_LOGGER_WARNING("vir dead and reconnect...");
            g_virConnectClose(*virConn);
            *virConn = NULL;
            ret = OpenVirConn(virConn);
            if (ret != 0) {
                SMAP_LOGGER_WARNING("vir dead and reconnect error.");
                return -ENOTCONN;
            }
            return 0;
        }
        // conn error
        return -ENOTCONN;
    }
    SMAP_LOGGER_WARNING("vir reconnect...");
    ret = OpenVirConn(virConn);
    if (ret != 0) {
        SMAP_LOGGER_WARNING("vir reconnect error.");
        return -ENOTCONN;
    }
    return 0;
}

void CloseVirConn(virConnectPtr *virConn)
{
    if (*virConn) {
        g_virConnectClose(*virConn);
        SMAP_LOGGER_DEBUG("virsh connection destroyed.");
        *virConn = NULL;
    }
}

int OpenVirConn(virConnectPtr *virConn)
{
    if (*virConn) {
        return 0;
    }
    *virConn = g_virConnectOpen(QEMU_SYSTEM_URI);
    if (!*virConn) {
        SMAP_LOGGER_ERROR("establist virsh connection error.");
        return -ENOTCONN;
    }
    SMAP_LOGGER_DEBUG("virsh connection established.");
    return 0;
}

void CloseVirHandler(void)
{
    if (g_virtHandler) {
        dlclose(g_virtHandler);
        g_virtHandler = NULL;
        SMAP_LOGGER_DEBUG("CloseVirHandler.");
    }
}

int OpenVirHandler(void)
{
    if (g_virtHandler) {
        SMAP_LOGGER_INFO("g_virtHandler already exists.");
        return 0;
    }
    SMAP_LOGGER_DEBUG("OpenVirHandler.");
    g_virtHandler = dlopen(LIB_PATH, RTLD_LAZY);
    if (!g_virtHandler) {
        SMAP_LOGGER_ERROR("dlopen libvirt.so error %s.", dlerror());
        return -ENOENT;
    }
    g_virConnectOpen = dlsym(g_virtHandler, "virConnectOpen");
    if (!g_virConnectOpen) {
        SMAP_LOGGER_ERROR("get symbol virConnectOpen error.");
        return -ENOENT;
    }
    g_virConnectClose = dlsym(g_virtHandler, "virConnectClose");
    if (!g_virConnectClose) {
        SMAP_LOGGER_ERROR("get symbol virConnectClose error.");
        return -ENOENT;
    }
    g_virConnectIsAlive = dlsym(g_virtHandler, "virConnectIsAlive");
    if (!g_virConnectIsAlive) {
        SMAP_LOGGER_ERROR("get symbol virConnectIsAlive error.");
        return -ENOENT;
    }
    g_virDomainLookupByID = dlsym(g_virtHandler, "virDomainLookupByID");
    if (!g_virDomainLookupByID) {
        SMAP_LOGGER_ERROR("get symbol virDomainLookupByID error.");
        return -ENOENT;
    }
    g_virDomainGetInfo = dlsym(g_virtHandler, "virDomainGetInfo");
    if (!g_virDomainGetInfo) {
        SMAP_LOGGER_ERROR("get symbol virDomainGetInfo error.");
        return -ENOENT;
    }
    g_virDomainFree = dlsym(g_virtHandler, "virDomainFree");
    if (!g_virDomainFree) {
        SMAP_LOGGER_ERROR("get symbol virDomainFree error.");
        return -ENOENT;
    }
    g_virDomainGetVcpus = dlsym(g_virtHandler, "virDomainGetVcpus");
    if (!g_virDomainGetVcpus) {
        SMAP_LOGGER_ERROR("get symbol virDomainGetVcpus error.");
        return -ENOENT;
    }
    g_virDomainGetXMLDesc = dlsym(g_virtHandler, "virDomainGetXMLDesc");
    if (!g_virDomainGetXMLDesc) {
        SMAP_LOGGER_ERROR("get symbol virDomainGetXMLDesc error.");
        return -ENOENT;
    }
    return 0;
}

char *GetXMLByDomainId(int domainId)
{
    virDomainPtr domainPtr;
    virConnectPtr virConn = NULL;
    char *xml;

    if (OpenVirConn(&virConn)) {
        return NULL;
    }
    SMAP_LOGGER_INFO("establish virsh connection success.");
    domainPtr = g_virDomainLookupByID(virConn, domainId);
    if (!domainPtr) {
        SMAP_LOGGER_ERROR("get domain %d by id error.", domainId);
        CloseVirConn(&virConn);
        return NULL;
    }

    xml = g_virDomainGetXMLDesc(domainPtr, 0);
    g_virDomainFree(domainPtr);
    CloseVirConn(&virConn);
    if (!xml) {
        SMAP_LOGGER_ERROR("get domain %d xml description failed.", domainId);
        return NULL;
    }

    return xml;
}