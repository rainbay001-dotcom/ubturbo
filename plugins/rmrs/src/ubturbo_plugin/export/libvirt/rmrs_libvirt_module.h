/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef RMRS_LIBVIRT_MODULE_H
#define RMRS_LIBVIRT_MODULE_H

#include <cstdint>
#include "rmrs_error.h"

namespace rmrs::libvirt {
using VirConnectPtr = void *;

struct VirDomainInfo {
    unsigned char state;        /* the running state, one of virDomainState */
    unsigned long maxMem;       /* the maximum memory in KBytes allowed */
    unsigned long memory;       /* the memory in KBytes used by the domain */
    unsigned short nrVirtCpu;   /* the number of virtual CPUs for the domain */
    unsigned long long cpuTime; /* the CPU time used in nanoseconds */
};

struct VirVcpuInfo {
    unsigned int number;        /* virtual CPU number */
    int state;                  /* value from virVcpuState */
    unsigned long long cpuTime; /* CPU time used, in nanoseconds */
    int cpu;                    /* real CPU number, or one of the values from virVcpuHostCpuState */
};

enum class VirConnectListAllDomainsFlags : unsigned int {
    VM_LIST_DOMAINS_ACTIVE = 1 << 0,   /* (Since: 0.9.13) */
    VM_LIST_DOMAINS_INACTIVE = 1 << 1, /* (Since: 0.9.13) */

    VM_LIST_DOMAINS_PERSISTENT = 1 << 2, /* (Since: 0.9.13) */
    VM_LIST_DOMAINS_TRANSIENT = 1 << 3,  /* (Since: 0.9.13) */

    VM_LIST_DOMAINS_RUNNING = 1 << 4, /* (Since: 0.9.13) */
    VM_LIST_DOMAINS_PAUSED = 1 << 5,  /* (Since: 0.9.13) */
    VM_LIST_DOMAINS_SHUTOFF = 1 << 6, /* (Since: 0.9.13) */
    VM_LIST_DOMAINS_OTHER = 1 << 7,   /* (Since: 0.9.13) */

    VM_LIST_DOMAINS_MANAGEDSAVE = 1 << 8,    /* (Since: 0.9.13) */
    VM_LIST_DOMAINS_NO_MANAGEDSAVE = 1 << 9, /* (Since: 0.9.13) */

    VM_LIST_DOMAINS_AUTOSTART = 1 << 10,    /* (Since: 0.9.13) */
    VM_LIST_DOMAINS_NO_AUTOSTART = 1 << 11, /* (Since: 0.9.13) */

    VM_LIST_DOMAINS_HAS_SNAPSHOT = 1 << 12, /* (Since: 0.9.13) */
    VM_LIST_DOMAINS_NO_SNAPSHOT = 1 << 13,  /* (Since: 0.9.13) */

    VM_LIST_DOMAINS_HAS_CHECKPOINT = 1 << 14, /* (Since: 5.6.0) */
    VM_LIST_DOMAINS_NO_CHECKPOINT = 1 << 15,  /* (Since: 5.6.0) */
};


enum class VirDomainEventDefinedDetailType : uint8_t {
    DEFINED_ADDED = 0,         /* Newly created config file */
    DEFINED_UPDATED = 1,       /* Changed config file */
    DEFINED_RENAMED = 2,       /* Domain was renamed */
    DEFINED_FROM_SNAPSHOT = 3, /* Config was restored from a snapshot */

#ifdef VIR_ENUM_SENTINELS
    DEFINED_LAST
#endif
};

enum class VirDomainEventType : uint8_t {
    DEFINED = 0,
    UNDEFINED = 1,
    STARTED = 2, // **
    SUSPENDED = 3,
    RESUMED = 4,
    STOPPED = 5, // **
    SHUTDOWN = 6,
    PMSUSPENDED = 7,
    CRASHED = 8,

#ifdef VIR_ENUM_SENTINELS
    LAST
#endif
};

using VirDomainEventDetailType = uint8_t;

enum class VirDomainEventUndefinedDetailType : VirDomainEventDetailType {
    UNDEFINED_REMOVED = 0, /* Deleted the config file */
    UNDEFINED_RENAMED = 1, /* Domain was renamed */

#ifdef VIR_ENUM_SENTINELS
    UNDEFINED_LAST
#endif
};

enum class VirDomainEventStartedDetailType : VirDomainEventDetailType {
    STARTED_BOOTED = 0,        /* Normal startup from boot */
    STARTED_MIGRATED = 1,      /* Incoming migration from another host */
    STARTED_RESTORED = 2,      /* Restored from a state file */
    STARTED_FROM_SNAPSHOT = 3, /* Restored from snapshot */
    STARTED_WAKEUP = 4,        /* Started due to wakeup event */

#ifdef VIR_ENUM_SENTINELS
    STARTED_LAST
#endif
};

enum class VirDomainEventSuspendedDetailType : VirDomainEventDetailType {
    SUSPENDED_PAUSED = 0,          /* Normal suspend due to admin pause */
    SUSPENDED_MIGRATED = 1,        /* Suspended for offline migration */
    SUSPENDED_IOERROR = 2,         /* Suspended due to a disk I/O error */
    SUSPENDED_WATCHDOG = 3,        /* Suspended due to a watchdog firing */
    SUSPENDED_RESTORED = 4,        /* Restored from paused state file */
    SUSPENDED_FROM_SNAPSHOT = 5,   /* Restored from paused snapshot */
    SUSPENDED_API_ERROR = 6,       /* suspended after failure during libvirt API call */
    SUSPENDED_POSTCOPY = 7,        /* suspended for post-copy migration */
    SUSPENDED_POSTCOPY_FAILED = 8, /* suspended after failed post-copy */

#ifdef VIR_ENUM_SENTINELS
    SUSPENDED_LAST
#endif
};

enum class VirDomainEventResumedDetailType : VirDomainEventDetailType {
    RESUMED_UNPAUSED = 0,      /* Normal resume due to admin unpause */
    RESUMED_MIGRATED = 1,      /* Resumed for completion of migration */
    RESUMED_FROM_SNAPSHOT = 2, /* Resumed from snapshot */
    RESUMED_POSTCOPY = 3,      /* Resumed, but migration is still
                                                running in post-copy mode */

#ifdef VIR_ENUM_SENTINELS
    RESUMED_LAST
#endif
};

enum class VirDomainEventStoppedDetailType : VirDomainEventDetailType {
    STOPPED_SHUTDOWN = 0,      /* Normal shutdown */
    STOPPED_DESTROYED = 1,     /* Forced poweroff from host */
    STOPPED_CRASHED = 2,       /* Guest crashed */
    STOPPED_MIGRATED = 3,      /* Migrated off to another host */
    STOPPED_SAVED = 4,         /* Saved to a state file */
    STOPPED_FAILED = 5,        /* Host emulator/mgmt failed */
    STOPPED_FROM_SNAPSHOT = 6, /* offline snapshot loaded */

#ifdef VIR_ENUM_SENTINELS
    STOPPED_LAST
#endif
};

enum class VirDomainEventShutdownDetailType : VirDomainEventDetailType {
    SHUTDOWN_FINISHED = 0, /* Guest finished shutdown sequence */
    SHUTDOWN_GUEST = 1,    /* Domain finished shutting down after request from the guest itself */
    SHUTDOWN_HOST = 2,     /* Domain finished shutting down after request from the host */

#ifdef VIR_ENUM_SENTINELS
    SHUTDOWN_LAST
#endif
};

enum class VirDomainEventCrashedDetailType : VirDomainEventDetailType {
    CRASHED_PANICKED = 0, /* Guest was panicked */
    CRASHED_CRASHLOADED = 1, /* Guest was crashloaded */

# ifdef VIR_ENUM_SENTINELS
    EVENT_CRASHED_LAST
# endif
};

using VirConnectOpenFunc = void *(*)(const char *);
using VirConnectCloseFunc = int (*)(void *);
using VirConnectListAllDomainsFunc = int (*)(void *, void ***, VirConnectListAllDomainsFlags);
using VirDomainGetNameFunc = const char *(*)(void *);
using VirDomainGetIDFunc = unsigned int (*)(void *);
using VirDomainGetUUIDStringFunc = int (*)(void *, char *);
using VirDomainGetInfoFunc = int (*)(void *, void *);
using VirDomainGetVcpusFunc = int (*)(void *, void *, int, unsigned char *, int);
using VirConnectGetHostnameFunc = char *(*)(void *);
using VirDomainFreeFunc = int (*)(void *);
using VirConnectIsAliveFunc = int (*)(void *);
using VirConnectDomainEventCallbackFunc = int (*)(void *, void *, int, int, void *);
using VirEventRegisterDefaultImplFunc = int (*)();
using VirEventRunDefaultImplFunc = int (*)();
using VirConnectDomainEventRegisterFunc = int (*)(void *, VirConnectDomainEventCallbackFunc, void *, void *);
using VirConnectDomainEventDeRegisterFunc = int (*)(void *, VirConnectDomainEventCallbackFunc);

class LibvirtModule {
public:
    static RmrsResult Init();

    static VirConnectOpenFunc VirConnectOpen();

    static VirConnectCloseFunc VirConnectClose();

    static VirConnectListAllDomainsFunc VirConnectListAllDomains();

    static VirDomainGetNameFunc VirDomainGetName();

    static VirDomainGetIDFunc VirDomainGetID();

    static VirDomainGetUUIDStringFunc VirDomainGetUUIDString();

    static VirDomainGetInfoFunc VirDomainGetInfo();

    static VirDomainGetVcpusFunc VirDomainGetVcpus();

    static VirConnectGetHostnameFunc VirConnectGetHostname();

    static VirDomainFreeFunc VirDomainFree();

    static VirConnectIsAliveFunc VirConnectIsAlive();

    static VirEventRegisterDefaultImplFunc VirEventRegisterDefaultImpl();

    static VirEventRunDefaultImplFunc VirEventRunDefaultImpl();

    static VirConnectDomainEventRegisterFunc VirConnectDomainEventRegister();

    static VirConnectDomainEventDeRegisterFunc VirConnectDomainEventDeRegister();

private:
    static void *libvirtHandle;
    static VirConnectOpenFunc virConnectOpenFunc;
    static VirConnectCloseFunc virConnectCloseFunc;
    static VirConnectListAllDomainsFunc virConnectListAllDomainsFunc;
    static VirDomainGetNameFunc virDomainGetNameFunc;
    static VirDomainGetIDFunc virDomainGetIDFunc;
    static VirDomainGetUUIDStringFunc virDomainGetUUIDStringFunc;
    static VirDomainGetInfoFunc virDomainGetInfoFunc;
    static VirDomainGetVcpusFunc virDomainGetVcpusFunc;
    static VirConnectGetHostnameFunc virConnectGetHostnameFunc;
    static VirDomainFreeFunc virDomainFreeFunc;
    static VirConnectIsAliveFunc virConnectIsAliveFunc;
    static VirEventRegisterDefaultImplFunc virEventRegisterDefaultImplFunc;
    static VirEventRunDefaultImplFunc virEventRunDefaultImplFunc;
    static VirConnectDomainEventRegisterFunc virConnectDomainEventRegisterFunc;
    static VirConnectDomainEventDeRegisterFunc virConnectDomainEventDeRegisterFunc;
};
} // mempooling::libvirt

#endif // RMRS_LIBVIRT_MODULE_H
