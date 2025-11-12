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

#include <fcntl.h>

#include "smap_user_log.h"
#include "strategy/migration.h"
#include "manage.h"
#include "securec.h"
#include "oom_migrate.h"

static void OomGetPaddr(uint64_t entry, struct MigList *mList, uint64_t *pageCount)
{
    int ret;
    if (entry & PRESENT) {
        uint64_t physPageNum = entry & PRN_SHIFT;
        uint64_t physAddr = physPageNum * PAGE_SIZE;
        if (IsHugeMode() && !IsHugeAligned(physAddr)) {
            return;
        }
        mList->nr -= 1;
        mList->addr[mList->nr] = physAddr;
        *pageCount = *pageCount - 1;
    }
    return;
}

static int GetPaddrFromMemRange(int pagemapFd, unsigned long start, unsigned long end, struct MigList *mList,
                                uint64_t *pageCount)
{
    int pageSize = PAGE_SIZE;
    for (unsigned long vaddr = start; vaddr < end; vaddr += pageSize) {
        off_t offset = (vaddr / pageSize) * PAGEMAP_ENTRY_SIZE;
        if (lseek(pagemapFd, offset, SEEK_SET) == (off_t)-1) {
            SMAP_LOGGER_ERROR("lseek pagemap failed.");
            return -EINVAL;
        }

        uint64_t entry;
        if (read(pagemapFd, &entry, PAGEMAP_ENTRY_SIZE) != PAGEMAP_ENTRY_SIZE) {
            SMAP_LOGGER_ERROR("read pagemap failed.");
            return -EINVAL;
        }
        OomGetPaddr(entry, mList, pageCount);
        if (mList->nr == 0 || *pageCount == 0) {
            return 0;
        }
    }
    return 0;
}

static int GetPaddrsFromPagemap(ProcessAttr *attr, int pagemapFd, uint64_t *pageCount, struct MigList *mList)
{
    int ret;
    int i;
    char mapsPath[PAGEMAP_LINE_LEN];

    ret = snprintf_s(mapsPath, MAPS_MAX_LEN, MAPS_MAX_LEN, "/proc/%d/maps", attr->pid);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("sprintf maps failed, pid %d.", attr->pid);
        return ret;
    }

    FILE *mapsFile = fopen(mapsPath, "r");
    if (mapsFile == NULL) {
        SMAP_LOGGER_ERROR("open pid %d maps failed.", attr->pid);
        return -ENODEV;
    }

    char line[PAGEMAP_LINE_LEN];
    while (fgets(line, sizeof(line), mapsFile) != NULL) {
        unsigned long start, end;
        if (sscanf_s(line, "%lx-%lx", &start, &end) == MAPS_LIN_LEN) {
            if (IsHugeMode() && !IsHugePageRange(line)) {
                continue;
            }
            ret = GetPaddrFromMemRange(pagemapFd, start, end, mList, pageCount);
            if (ret) {
                fclose(mapsFile);
                SMAP_LOGGER_ERROR("process pid %d memory range failed.", attr->pid);
                return ret;
            }

            if (mList->nr == 0 || *pageCount == 0) {
                break;
            }
        }
    }
    ret = fclose(mapsFile);
    if (ret) {
        SMAP_LOGGER_ERROR("close maps failed: %d.", ret);
        return ret;
    }
    return 0;
}

static int OpenPidPagemapFile(pid_t pid, int *pagemapFd)
{
    int ret;
    char pagemapPath[256];
    ret = snprintf_s(pagemapPath, MAPS_MAX_LEN, MAPS_MAX_LEN, "/proc/%d/pagemap", pid);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("sprintf pagemap failed, pid %d.", pid);
        return -EINVAL;
    }
    *pagemapFd = open(pagemapPath, O_RDONLY);
    if (*pagemapFd < 0) {
        SMAP_LOGGER_ERROR("open pid %d pagemap failed.", pid);
        return -ENODEV;
    }
    return 0;
}

static int InitMigList(struct MigList *mList, uint64_t *pageCount, ProcessAttr *attr)
{
    int node;
    int ret;
    struct ProcessManager *pm = GetProcessManager();

    ret = GetNumaNodesForPid(attr->pid, &node);
    if (ret) {
        SMAP_LOGGER_ERROR("get pid %d numa nodes failed.", attr->pid);
        return ret;
    }
    if (node < pm->nrLocalNuma && !IsNodeInvalid(node)) {
        mList->from = node;
    } else {
        SMAP_LOGGER_ERROR("pid %d numa %d is invalid.", attr->pid, node);
        return -EINVAL;
    }
    mList->pid = attr->pid;
    mList->to = GetAttrL2(attr);
    SMAP_LOGGER_INFO("mList->from =  %d, mList->to =  %d .", mList->from, mList->to);

    uint64_t sumPages = GetPidNrPages(attr->pid);
    if (attr->initLocalMemRatio == HUNDRED) {
        SMAP_LOGGER_INFO("Mig Ratio = 0, can not migrate.");
        mList->nr = 0;
        return 0;
    }
    uint64_t nrPages = sumPages * (HUNDRED - attr->initLocalMemRatio) / HUNDRED;
    nrPages = nrPages > *pageCount ? *pageCount : nrPages;

    SMAP_LOGGER_INFO("sumPages = %d , nrPages =  %llu, pageCount = %llu.", sumPages, nrPages, *pageCount);
    mList->nr = nrPages;
    if (nrPages == 0) {
        return 0;
    }
    mList->addr = malloc(sizeof(uint64_t) * nrPages);
    if (!mList->addr) {
        SMAP_LOGGER_ERROR("mList->addr malloc failed.");
        return -ENOMEM;
    }
    return 0;
}

static int InitOomMigrateMsg(struct MigrateMsg *mMsg, struct ProcessManager *manager)
{
    mMsg->cnt = 0;
    mMsg->mulMig.nrThread = manager->nrThread;
    mMsg->mulMig.isMulThread = manager->nrThread == 1 ? false : true;
    mMsg->mulMig.pageSize = manager->tracking.pageSize;

    mMsg->migList = malloc(sizeof(struct MigList) * manager->nrNuma);
    if (!mMsg->migList) {
        SMAP_LOGGER_ERROR("mMsg->migList malloc failed.");
        return -ENOMEM;
    }
    return 0;
}

static void FindEnoughPageToMigrate(uint64_t *pageCount, ProcessAttr *attr, struct MigrateMsg *mMsg)
{
    int ret, pagemapFd;
    struct MigList mList;

    ret = InitMigList(&mList, pageCount, attr);
    if (mList.nr == 0) {
        SMAP_LOGGER_INFO("mList.nr == 0, don't need to migrate.");
        return;
    }
    if (ret) {
        SMAP_LOGGER_ERROR("InitMigList failed, pid %d  ret:%d.", attr->pid, ret);
        return;
    }
    uint64_t migPages = mList.nr;
    ret = OpenPidPagemapFile(attr->pid, &pagemapFd);
    if (ret) {
        SMAP_LOGGER_ERROR("Open pid %d pagemap file error, ret:%d.", attr->pid, ret);
        free(mList.addr);
        return;
    }

    ret = GetPaddrsFromPagemap(attr, pagemapFd, pageCount, &mList);
    close(pagemapFd);
    if (ret < 0) {
        SMAP_LOGGER_ERROR("get paddrs from pagemap failed, pid %d, ret %d.", attr->pid, ret);
        free(mList.addr);
        return;
    }

    mList.nr = migPages;
    ret = AddMigList(mMsg, &mList);
    free(mList.addr);
    if (ret) {
        SMAP_LOGGER_ERROR("process :%d, AddMigList failed! ret:%d.", attr->pid, ret);
    }
    return;
}

void FindPidMigrateSize(uint64_t size)
{
    struct MigrateMsg mMsg;
    int ret;
    struct ProcessManager *manager = GetProcessManager();
    ret = InitOomMigrateMsg(&mMsg, manager);
    if (ret) {
        SMAP_LOGGER_ERROR("InitMigrateMsg failed! ret:%d.", ret);
        return;
    }

    EnvMutexLock(&manager->lock);
    uint64_t pageCount = size / manager->tracking.pageSize;
    SMAP_LOGGER_INFO("pageCount = %llu.", pageCount);
    ProcessAttr *current = manager->processes;
    while (current) {
        int l2Node = GetAttrL2(current);
        if (l2Node == NUMA_NO_NODE) {
            current = current->next;
            continue;
        }
        // 判断为空，对应pid还未进行过迁移，遍历
        if (current->state == PROC_IDLE && current->walkPage.nrPages[l2Node] == 0) {
            SMAP_LOGGER_INFO("in FindEnoughPageToMigrate.");
            FindEnoughPageToMigrate(&pageCount, current, &mMsg);
            if (pageCount == 0) {
                break;
            }
        }
        current = current->next;
    }
    SMAP_LOGGER_INFO("mMsg.cnt = %d , pageCount = %llu.", mMsg.cnt, pageCount);
    ret = DoMigration(&mMsg, manager);
    UpdateMigResult(&mMsg, manager);
    EnvMutexUnlock(&manager->lock);
    free(mMsg.migList);
    if (ret) {
        SMAP_LOGGER_ERROR("Do migration failed! ret:%d.", ret);
    }
    return;
}