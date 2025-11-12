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
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "securec.h"
#include "smap_env.h"
#include "smap_user_log.h"
#include "manage.h"
#include "access_ioctl.h"
#include "smap_ioctl.h"
#include "device.h"

typedef enum {
    PGSIZE_FOUR_KB = 0,
    PGSIZE_TWO_MB = 9,
} PageSize;

typedef enum {
    ACCESS_MODE_SUM = 5,
    MODE_MAX,
} ScanMode;

static int SendCmdToAllNodes(int fds[], unsigned long cmd, int arg)
{
    int i;
    int ret = 0;

    for (i = 0; i < MAX_NODES; i++) {
        if (fds[i] >= 0) {
            if (ioctl(fds[i], cmd, arg) < 0) {
                SMAP_LOGGER_ERROR("ioctl for node%d failed: %s, skipped.", i, strerror(errno));
                ret = -EBADF;
            }
        }
    }
    return ret;
}

static int FindFdByNode(int fds[], int fdsLength)
{
    int i;

    for (i = 0; i < fdsLength; i++) {
        if (fds[i] >= 0) {
            return fds[i];
        }
    }
    return -EINVAL;
}

int EnableTracking(struct ProcessManager *manager)
{
    return SendCmdToAllNodes(manager->fds.nodes, SMAP_IOCTL_TRACKING_CMD, 1);
}

int DisableTracking(struct ProcessManager *manager)
{
    return SendCmdToAllNodes(manager->fds.nodes, SMAP_IOCTL_TRACKING_CMD, 0);
}

int ReadTracking(struct ProcessManager *manager)
{
    int i;
    int ret = 0;
    clock_t start, end;
    double cpuTimeUsed;
    start = clock();
    for (i = 0; i < MAX_NODES; i++) {
        if (manager->fds.nodes[i] < 0 || !manager->nodeActcLen[i] || !manager->tracking.nodeActc[i]) {
            continue;
        }
        ret = memset_s(manager->tracking.nodeActc[i], sizeof(actc_t) * manager->nodeActcLen[i], 0,
                       sizeof(actc_t) * manager->nodeActcLen[i]);
        if (ret != EOK) {
            return ret;
        }
        ret = read(manager->fds.nodes[i], manager->tracking.nodeActc[i], sizeof(actc_t) * manager->nodeActcLen[i]);
        if (ret < 0) {
            SMAP_LOGGER_ERROR("read for node%d failed: %d.", i, ret);
            return ret;
        }
    }
    end = clock();
    cpuTimeUsed = (double)(end - start) / CLOCKS_PER_SEC * US_PER_SEC;
    SMAP_LOGGER_DEBUG("ReadTracking used %.2fus.", cpuTimeUsed);
    return 0;
}

// calculate 2M count by addr range
static uint64_t Calc2MCount(uint64_t range)
{
    return (range & ~TWO_MEGA_MASK) == 0 ? (uint64_t)(range >> TWO_MEGA_SHIFT) :
                                           (uint64_t)((range >> TWO_MEGA_SHIFT) + 1);
}

// calculate 4K count by addr range
static uint64_t Calc4KCount(uint64_t range)
{
    return (range & ~PAGE_MASK) == 0 ? (uint64_t)(range >> PAGE_SHIFT) : (uint64_t)((range >> PAGE_SHIFT) + 1);
}

int GetNodeActcLenIomem(int len, uint64_t *nodeActcLen, IomemMsg *msg, uint16_t nrLocalNuma)
{
    int i;

    if (len <= 0) {
        return -EINVAL;
    }

    for (i = nrLocalNuma; i < len; i++) {
        nodeActcLen[i] = 0;
    }

    for (i = 0; i < msg->cnt; i++) {
        if (msg->iomemSegArray[i].node >= len || msg->iomemSegArray[i].node < nrLocalNuma) {
            continue;
        }
        nodeActcLen[msg->iomemSegArray[i].node] +=
            IsHugeMode() ? Calc2MCount(msg->iomemSegArray[i].end - msg->iomemSegArray[i].start + 1) :
                           Calc4KCount(msg->iomemSegArray[i].end - msg->iomemSegArray[i].start + 1);
    }
    return 0;
}

static int GetNodeActcLenAcpi(int len, uint64_t *nodeActcLen, AcpiMsg *msg)
{
    int i;
    uint64_t range;
    uint64_t pageCnt;

    if (len == 0) {
        return -EINVAL;
    }

    for (i = 0; i < len; i++) {
        nodeActcLen[i] = 0;
    }

    for (i = 0; i < msg->cnt; i++) {
        if (msg->acpiSegArray[i].node >= len) {
            continue;
        }
        range = msg->acpiSegArray[i].end - msg->acpiSegArray[i].start + 1;
        if (IsHugeMode()) {
            pageCnt = Calc2MCount(range);
        } else {
            pageCnt = Calc4KCount(range);
        }
        nodeActcLen[msg->acpiSegArray[i].node] += pageCnt;
    }
    return 0;
}

static int GetNodeActcLen(struct ProcessManager *manager)
{
    int ret;

    ret = GetNodeActcLenIomem(MAX_NODES, manager->nodeActcLen, &manager->iomMsg, manager->nrLocalNuma);
    if (ret) {
        SMAP_LOGGER_ERROR("Get node actc len from iomem failed : %d.", ret);
        return ret;
    }

    ret = GetNodeActcLenAcpi(manager->nrLocalNuma, manager->nodeActcLen, &manager->acpiMsg);
    if (ret) {
        SMAP_LOGGER_ERROR("Get node actc len from acpi or numa failed : %d.", ret);
        return ret;
    }
    return 0;
}

void FreeNodeActcData(struct ProcessManager *manager)
{
    int nid;

    for (nid = 0; nid < MAX_NODES; nid++) {
        if (manager->tracking.nodeActc[nid]) {
            free(manager->tracking.nodeActc[nid]);
        }
        manager->tracking.nodeActc[nid] = NULL;
    }
}

static int InitNodeActcData(struct ProcessManager *manager)
{
    int nid;
    int ret;

    ret = GetNodeActcLen(manager);
    if (ret) {
        SMAP_LOGGER_ERROR("Error when get actc len : %d.", ret);
        return ret;
    }
    FreeNodeActcData(manager);
    for (nid = 0; nid < MAX_NODES; nid++) {
        if (!manager->nodeActcLen[nid]) {
            continue;
        }
        manager->tracking.nodeActc[nid] = malloc(sizeof(actc_t) * manager->nodeActcLen[nid]);
        if (!manager->tracking.nodeActc[nid]) {
            SMAP_LOGGER_ERROR("Alloc actc mem failed for node %d.", nid);
            FreeNodeActcData(manager);
            return -ENOMEM;
        }
    }
    return 0;
}

static int ConfigTrackingDev(int *trackingFds, uint32_t pageSize)
{
    int ret = 0;
    int arg;
    arg = ACCESS_MODE_SUM;
    ret |= SendCmdToAllNodes(trackingFds, SMAP_IOCTL_MODE_SET_CMD, arg);

    arg = pageSize == PAGESIZE_2M ? PGSIZE_TWO_MB : PGSIZE_FOUR_KB;
    ret |= SendCmdToAllNodes(trackingFds, SMAP_IOCTL_PAGE_SIZE_SET_CMD, arg);

    return ret;
}

void FreeAddrMemory(struct ProcessManager *manager)
{
    if (manager->acpiMsg.acpiSegArray) {
        free(manager->acpiMsg.acpiSegArray);
        manager->acpiMsg.acpiSegArray = NULL;
    }

    if (manager->iomMsg.iomemSegArray) {
        free(manager->iomMsg.iomemSegArray);
        manager->iomMsg.iomemSegArray = NULL;
    }
}

static void GetLocalNrNuma(struct ProcessManager *manager)
{
    int i = 0;
    manager->nrLocalNuma = 0;
    for (i = 0; i < manager->acpiMsg.cnt; i++) {
        if (manager->acpiMsg.acpiSegArray[i].node >= manager->nrLocalNuma) {
            manager->nrLocalNuma = manager->acpiMsg.acpiSegArray[i].node + 1;
        }
    }
    SMAP_LOGGER_INFO("get local nr numa: %lu.", manager->nrLocalNuma);
}

static int GetAcpiAddresses(struct ProcessManager *manager)
{
    int ret = 0;
    int fd;

    fd = FindFdByNode(manager->fds.nodes, MAX_NODES);
    if (fd < 0) {
        SMAP_LOGGER_ERROR("Can't find fd by node %d.", fd);
        return fd;
    }
    ret = ioctl(fd, SMAP_IOCTL_ACPI_LEN, &(manager->acpiMsg.cnt));
    if (ret) {
        SMAP_LOGGER_ERROR("ioctl get acpi len failed: %d.", ret);
        return ret;
    }
    manager->acpiMsg.acpiSegArray = malloc(sizeof(AcpiSeg) * manager->acpiMsg.cnt);
    if (!manager->acpiMsg.acpiSegArray) {
        SMAP_LOGGER_ERROR("Alloc acpi mem failed.");
        return -ENOMEM;
    }
    ret = ioctl(fd, SMAP_IOCTL_ACPI_ADDR, manager->acpiMsg.acpiSegArray);
    if (ret) {
        free(manager->acpiMsg.acpiSegArray);
        manager->acpiMsg.acpiSegArray = NULL;
        SMAP_LOGGER_ERROR("ioctl get acpi addr failed: %d.", ret);
        return ret;
    }
    GetLocalNrNuma(manager);
    return ret;
}

int GetIomemAddresses(struct ProcessManager *manager)
{
    int ret = 0;
    int fd;

    fd = FindFdByNode(manager->fds.nodes, MAX_NODES);
    if (fd < 0) {
        SMAP_LOGGER_ERROR("Can't find fd by node %d.", fd);
        return fd;
    }
    ret = ioctl(fd, SMAP_IOCTL_IOMEM_LEN, &(manager->iomMsg.cnt));
    if (ret) {
        SMAP_LOGGER_ERROR("manager->iomMsg.cnt failed  ret : %d.", ret);
        return ret;
    }
    if (manager->iomMsg.cnt == 0) {
        SMAP_LOGGER_WARNING("manager->iomMsg.cnt is 0.");
        free(manager->iomMsg.iomemSegArray);
        manager->iomMsg.iomemSegArray = NULL;
        return 0;
    }
    if (manager->iomMsg.iomemSegArray) {
        free(manager->iomMsg.iomemSegArray);
        manager->iomMsg.iomemSegArray = NULL;
    }
    manager->iomMsg.iomemSegArray = malloc(sizeof(IomemSeg) * manager->iomMsg.cnt);
    if (!manager->iomMsg.iomemSegArray) {
        SMAP_LOGGER_ERROR("Alloc iomem failed.");
        return -ENOMEM;
    }

    return ioctl(fd, SMAP_IOCTL_IOMEM_ADDR, manager->iomMsg.iomemSegArray);
}

static int GetTrackingAddr(struct ProcessManager *manager)
{
    int ret = 0;

    FreeAddrMemory(manager);
    ret = GetAcpiAddresses(manager);
    if (ret) {
        FreeAddrMemory(manager);
        SMAP_LOGGER_ERROR("GetAcpiAddresses failed. ret : %d.", ret);
        return ret;
    }
    ret |= GetIomemAddresses(manager);
    if (ret) {
        SMAP_LOGGER_ERROR("GetIomemAddresses failed. ret : %d.", ret);
        FreeAddrMemory(manager);
        return ret;
    }
    return ret;
}

int ConfigureTrackingDevices(struct ProcessManager *manager)
{
    int ret;

    if (!manager->nrNuma) {
        SMAP_LOGGER_ERROR("cannot find node* under /dev.");
        return -ENODEV;
    }

    ret = ConfigTrackingDev(manager->fds.nodes, manager->tracking.pageSize);
    if (ret) {
        SMAP_LOGGER_ERROR("Error when config tracking-node devices.");
        return ret;
    }

    ret = GetTrackingAddr(manager);
    if (ret) {
        SMAP_LOGGER_ERROR("Error when get tracking addresses.");
        return ret;
    }
    ret = InitNodeActcData(manager);
    if (ret) {
        FreeAddrMemory(manager);
        SMAP_LOGGER_ERROR("Error when update node segment.");
        return ret;
    }
    return 0;
}

static int OpenAndFlockFd(int *fd, const char *device)
{
    int tmp = open(device, O_RDWR);
    if (tmp < 0) {
        SMAP_LOGGER_ERROR("cannot find %s, skipped.", device);
        return -ENODEV;
    }

    if (flock(tmp, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            SMAP_LOGGER_ERROR("Device %s is already in use by another process.", device);
        } else {
            SMAP_LOGGER_ERROR("Flock %s failed.", device);
        }
        close(tmp);
        return -EACCES;
    }

    *fd = tmp;
    return 0;
}

int InitTrackingDev(struct ProcessManager *manager)
{
    int i;
    int ret = 0;
    int fd;
    char path[PATH_MAX];
    manager->nrNuma = 0;

    ret = OpenAndFlockFd(&fd, TIERING_PATH);
    if (ret) {
        return ret;
    }
    manager->fds.migrate = fd;
    fd = open(ACCESS_DEVICE, O_RDWR);
    if (fd < 0) {
        SMAP_LOGGER_ERROR("cannot find access dev under /dev.");
        return -ENODEV;
    }
    manager->fds.access = fd;
    for (i = 0; i < MAX_NODES; i++) {
        ret = snprintf_s(path, sizeof(path), sizeof(path), NODE_PATH, i);
        if (ret == -1) {
            SMAP_LOGGER_ERROR("Build tracking node path failed: %d.", ret);
            return -EINVAL;
        }
        if (access(path, F_OK) != 0) {
            if (errno == ENOENT) {
                continue;
            }
            SMAP_LOGGER_ERROR("%s exists, but cannot be accessed.", path);
            return -ENODEV;
        }
        fd = open(path, O_RDWR);
        if (fd < 0) {
            manager->fds.nodes[i] = DEFAULT_FD;
            SMAP_LOGGER_WARNING("Open tracking node failed: %d.", -errno);
            continue;
        }
        manager->fds.nodes[i] = fd;
        manager->nrNuma++;
        SMAP_LOGGER_INFO("%s is managed.", path);
    }
    ret = ConfigureTrackingDevices(manager);
    if (ret) {
        SMAP_LOGGER_ERROR("config tracking devices failed : %d.", ret);
    }
    return ret;
}

int GetRamIsChange(struct ProcessManager *manager, int *change)
{
    int fd;
    int ret;

    fd = FindFdByNode(manager->fds.nodes, MAX_NODES);
    if (fd < 0) {
        SMAP_LOGGER_ERROR("Can't find fd by node %d.", fd);
        return fd;
    }
    ret = ioctl(fd, SMAP_IOCTL_RAM_CHANGE, change);
    if (ret) {
        SMAP_LOGGER_ERROR("Error when change ram segment : %d.", ret);
        return ret;
    }
    return 0;
}

void DeinitTrackingDev(struct ProcessManager *manager)
{
    int i;

    DisableTracking(manager);
    FreeAddrMemory(manager);
    FreeNodeActcData(manager);
    for (i = 0; i < MAX_NODES; i++) {
        if (manager->fds.nodes[i] >= 0) {
            close(manager->fds.nodes[i]);
            manager->fds.nodes[i] = DEFAULT_FD;
        }
    }
    if (manager->fds.migrate >= 0) {
        close(manager->fds.migrate);
        manager->fds.migrate = DEFAULT_FD;
    }
    if (manager->fds.access >= 0) {
        close(manager->fds.access);
        manager->fds.access = DEFAULT_FD;
    }
    manager->nrNuma = 0;
}
