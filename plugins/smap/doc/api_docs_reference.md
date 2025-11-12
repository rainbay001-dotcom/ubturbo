# ubturbo_smap_start: 初始化SMAP

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_start(uint32_t pageType, Logfunc extlog);
```

## 描述 DESCRIPTION

初始化SMAP，设置场景及迁移页面类型，虚拟化场景对应2M大页迁移，通算场景对应4K页迁移。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| pageType | IN | 页面类型：0：4K页。1：2M页。 |
| extlog | IN | Logfunc日志函数。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | 同进程重复初始化 |
| -EIO | 日志初始化失败 |
| -EBADF | 初始化异常 |
| -ENOMEM | 内存申请失败 |
| -EACCES | 其它进程已初始化 |
| -ENODEV | 内核驱动未安装 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* 不能重复初始化。
* pageType需要和当前场景匹配。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成SMAP的初始化。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret = SmapInit(1, NULL);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_stop: 停止SMAP

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_stop(void);
```

## 描述 DESCRIPTION

停止SMAP，释放资源（包含移除管理的pid迁出列表、enable状态位等）。

## 参数 Parameters

不涉及。

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | 已停止 |

## 约束 CONSTRAINTS

不涉及。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成SMAP的停止。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret = ubturbo_smap_stop();
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_migrate_out: 配置进程迁出

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_migrate_out(struct MigrateOutMsg *msg, int pidType);
```

## 描述 DESCRIPTION

配置虚拟机/进程的远端NUMA和迁出比例。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| msg | IN | 配置参数。 |
| msg.count | IN | 配置数量。 |
| msg.payload | IN | 配置数组。 |
| msg.payload.destNid | IN | 远端NUMA。 |
| msg.payload.pid | IN | 进程PID。 |
| msg.payload.ratio | IN | 迁出比例。 |
| msg.payload.memSize | IN | 迁出大小，单位KB。 |
| msg.payload.migrateMode | IN | 迁移模式，0：按比例， 1：按大小。 |
| pidType | IN | 进程页面类型，0：4K，1：2M。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -ESRCH | 部分进程不存在但其余进程配置成功 |
| -ENOMEM | 内存申请失败 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* pidType需要和当前场景匹配。
* HCCS代际远端NUMA最大值为21。
* UB代际远端NUMA最大值为21。
* 远端NUMA被禁用时无法配置迁出（调用SmapMigrateBack接口时会默认禁用远端NUMA）。
* 如果已配置某虚机的远端NUMA，后续配置不能改变虚机的远端NUMA，只能通过SmapMigratePidRemoteNuma接口改变远端NUMA。
* 配置pid内存迁出后，由SMAP线程异步迁移，在迁移周期到来时才会执行迁移操作。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序配置PID为10253的虚机进程迁出到NUMA4上，迁出比例为25%。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    struct MigrateOutMsg msg = {
        .count = 1,
        .payload = {
            {
                .destNid = 4,
                .pid = 10253,
                .ratio = 25,
                .memSize = 0,
                .migrateMode = 0,
            }
        };
    };

    ret = ubturbo_smap_migrate_out(&msg, 1);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_remote_numa_info_set: 设置远端NUMA内存可用量

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_remote_numa_info_set(struct SetRemoteNumaInfoMsg *msg);
```

## 描述 DESCRIPTION

设置本地NUMA对于远端NUMA的内存可用量。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| msg | IN | 配置参数。 |
| msg.srcNid | IN | 本地NUMA，-1代表所有本地NUMA共享。 |
| msg.destNid | IN | 远端NUMA。 |
| msg.size | IN | 内存可用量，单位MB。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* HCCS代际远端NUMA最大值为21。
* UB代际远端NUMA最大值为21。
* 如果已配置某虚机的远端NUMA，后续配置不能改变虚机的远端NUMA，只能通过SmapMigratePidRemoteNuma接口改变远端NUMA。
* 当配置的pid可迁出的量大于借用内存量时，SMAP不会使用完所有的借用量，每个本地NUMA对应的远端借用都有MIN[借用量的5%，200MB]的量不会使用，这是为了迁移内存时能申请到新的内存页面。例如：numa0 numa1分别借用了2G和6G共8G，numa0借的2G的预留按5%来算是100M，numa1借的6G的预留是200M，合计一共300M。
* 此接口仅在水线场景中生效，且水线场景调用SmapMigrateOut接口前需通过此接口设置远端NUMA使用量才能迁出pid的内存。
* 如果未调用SetSmapRemoteNumaInfo接口，默认初始化size值为0。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序设置NUMA0可以使用NUMA4的内存量为1024MB。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    struct SetRemoteNumaInfoMsg msg = {
        .srcNid = 0,
        .destNid = 4,
        .size = 1024,
    };

    ret = ubturbo_smap_remote_numa_info_set(&msg);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_migrate_back: 迁回远端内存

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_migrate_back(struct MigrateBackMsg *msg);
```

## 描述 DESCRIPTION

将指定远端NUMA的内存迁移到同一远端NUMA的其他地址段或迁回本地NUMA。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| msg | IN | 配置参数。 |
| msg.taskID | IN | 任务ID。 |
| msg.count | IN | 子任务数量。 |
| msg.payload | IN | 子任务配置。 |
| msg.payload.srcNid | IN | 源NUMA，远端NUMA。 |
| msg.payload.destNid | IN | 目的NUMA，本地NUMA，-1代表由SMAP决定目的NUMA。 |
| msg.payload.memid | IN | 迁回地址段的memid。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EBADF | 系统调用失败 |
| -EAGAIN | 超时 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 不支持并发调用此接口，否则会引起内存归还失败。
* 远端NUMA和传入的地址段需要匹配。
* 调用此接口后，SMAP默认禁止指定远端NUMA的冷热流动，只允许迁回任务中的迁移。
* 若远端NUMA的其他地址段的空闲页面不够，迁移任务会失败。
* 虚拟化水线场景下，请调用此接口前，调用SetSmapRemoteNumaInfo接口通知SMAP更新借用内存量，保证远端NUMA有足够的内存空间。
* 内存碎片场景下，调用此接口，需由调用方预留足够内存空间，否则会迁回失败。
* 迁回任务为异步执行，执行状态在/sys/kernel/debug/smap/mb\_[taskID]中进行查询。
* 同一个远端NUMA不支持并发调用，并发调用可能导致迁移数据无法迁移干净。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序将memid为1的借用内存从NUMA4迁回，目的NUMA由SMAP决定。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    struct MigrateBackMsg msg = {
        .taskId = 1,
        .count = 1,
        .payload = {
            .srcNid = 4,
            .destNid = -1,
            .memid = 1,
        }
    };

    ret = ubturbo_smap_migrate_back(&msg);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_remove: 移除SMAP对进程的管理

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_remove(struct RemoveMsg *msg, int pidType);
```

## 描述 DESCRIPTION

从SMAP管理的虚机/进程中移除指定的虚机/进程。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| msg | IN | 配置参数。 |
| msg.count | IN | 进程数量。 |
| msg.payload | IN | 进程信息。 |
| msg.payload.pid | IN | 进程PID。 |
| pidType | IN | 进程页面类型，0：4K，1：2M。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EBADF | 处理异常 |
| -ENOMEM | 申请内存失败 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* pidType需要和当前场景匹配。
* 当调用SmapMigrateBack接口迁回完所有地址后，需使用SmapRemove接口移除虚机管理，保证后续流程正常。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序将PID为10253的借虚机进程从SMAP管理中移除。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    struct RemoveMsg msg = {
        .count = 1,
        .payload = {
            .pid = 10253,
        }
    };

    ret = ubturbo_smap_remove(&msg, 1);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_node_enable: 启用NUMA迁移

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_node_enable(struct EnableNodeMsg *msg);
```

## 描述 DESCRIPTION

启用NUMA迁入，允许其它NUMA的内存向该NUMA进行迁移。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| msg | IN | 配置参数。 |
| msg.enable | IN | 0：停用，1：启用。 |
| msg.nid | IN | 远端NUMA。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 此接口与SmapMigrateBack接口配合使用，目的是在SmapMigrateBack接口调用后，恢复对应远端NUMA的冷热流动。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序禁用了NUMA4上内存相关的迁移。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    struct EnableNodeMsg msg = {
        .enable = 0,
        .nid = 4,
    };

    ret = ubturbo_smap_node_enable(&msg);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_vm_freq_query: 查询虚机冷热信息

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_vm_freq_query(int pid, uint16_t *data, uint16_t lengthIn, uint16_t *lengthOut, int dataSource);
```

## 描述 DESCRIPTION

查询虚机冷热信息。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| pid | IN | 进程PID。 |
| data | OUT | 访存数组。 |
| lengthIn | IN | 数组长度。 |
| lengthOut | OUT | 实际数组长度。 |
| dataSource | IN | 数据来源，0代表数据来自周期性冷热迁移时的统计，1代表数据来自独立的统计。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 调用前使用SmapMigrateOut接口设置迁移比例为0，后续在多个迁移周期后获取到冷热数据。
* 此接口为内存池化碎片场景使用，不在虚拟化场景使用。使用前请调用SetSmapRunMode接口设置内存池化场景。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序查询了PID为10253，虚机内存规格为2GB的虚机进程的访存数据。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    int pid = 10253;
    uint16_t data[1024] = { 0 };
    uint16_t lengthIn = 1024;
    uint16_t lengthOut;

    ret = ubturbo_smap_vm_freq_query(pid, data, lengthIn, &lengthOut, 1);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_run_mode_set: 设置SMAP运行模式

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_run_mode_set(int runMode);
```

## 描述 DESCRIPTION

设置SMAP运行模式。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| runMode | IN | 运行模式，0：水线场景，1：内存碎片场景。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EBADF | 同步配置文件失败 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 未设置的情况下，默认为水线场景。
* 如果是4K场景，不支持设置SMAP运行模式为内存碎片模式。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序设置了SMAP运行模式为水线场景。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;

    ret = ubturbo_smap_run_mode_set(0);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_process_migrate_enable: 启用/禁用进程的内存迁移

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_process_migrate_enable(pid_t *pidArr, int len, int enable, int flags);
```

## 描述 DESCRIPTION

启用/禁用PID对应虚机的冷热迁移和迁回。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| pidArr | IN | 进程PID数组。 |
| len | IN | 数组长度。 |
| enable | IN | 0：禁用，1：启用。 |
| flags | IN | 保留字段。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EINVAL | 参数错误 |
| -ETIMEDOUT | 超时 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* flags为保留字段，暂未使用。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序禁用了PID为10253的进程的内存迁移。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    pid_t pidArr[1] = { 10253 };

    ret = ubturbo_smap_process_migrate_enable(pidArr, 1, 0, 0);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_remote_numa_migrate: 迁移远端NUMA到另一远端NUMA

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_remote_numa_migrate(struct MigrateNumaMsg *msg);
```

## 描述 DESCRIPTION

迁移远端NUMA到另一远端NUMA。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| msg | IN | 配置参数。 |
| msg.srcNid | IN | 源NUMA。 |
| msg.destNid | IN | 目的NUMA。 |
| msg.count | IN | memid数量。 |
| msg.memids | IN | memid数组。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EBADF | 迁移成功但修改进程远端NUMA失败 |
| -ENOMEM | 迁移失败 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 传入的地址段需要和源NUMA ID对应的地址段匹配。
* 调用该接口前必须调用SmapEnableProcessMigrate禁用pid迁移功能。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序将NUMA4上memid为1的内存迁移到NUMA5上。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    struct MigrateNumaMsg msg = {
        .srcNid = 4,
        .destNid = 5,
        .count = 1,
        .memids = { 1 },
    };

    ret = ubturbo_smap_remote_numa_migrate(&msg);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_pid_remote_numa_migrate: 迁移进程远端NUMA到另一远端NUMA

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_pid_remote_numa_migrate(pid_t *pidArr, int len, int srcNid, int destNid);
```

## 描述 DESCRIPTION

迁移进程远端NUMA到另一远端NUMA。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| pidArr | IN | 进程PID数组。 |
| len | IN | 数组长度。 |
| srcNid | IN | 源NUMA。 |
| destNid | IN | 目的NUMA。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -ENXIO | PID的远端NUMA不全为srcNid |
| -EBADF | 迁移成功但修改进程远端NUMA失败 |
| -ENOMEM | 内存申请失败 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 目的NUMA ID内存余量充足。
* 不支持重复调用。
* 调用该接口前必须调用SmapEnableProcessMigrate禁用pid迁移功能。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序将PID为10253的进程的NUMA4上的内存迁移到NUMA5上。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    pid_t pidArr[1] = { 10253 };

    ret = ubturbo_smap_pid_remote_numa_migrate(pidArr, 1, 4, 5);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_process_tracking_add: 添加进程扫描

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_process_tracking_add(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType);
```

## 描述 DESCRIPTION

通知SMAP添加进程扫描，并设置扫描周期参数。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| pidArr | IN | 进程PID数组。 |
| scanTime | IN | 扫描间隔，单位ms，必须为50的倍数，最大200。 |
| duration | IN | 扫描持续时长，scanType为2时有效。 |
| len | IN | 数组长度。 |
| scanType | IN | 0：将进程设置为只扫描状态，1：将进程恢复为冷热扫描加迁移状态，2：表示进程设置为统计特定时长冷热信息状态。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EBADF | 内核调用失败 |
| -ENOMEM | 内存申请失败 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 只支持虚机场景
* 当进程未被SMAP纳管时，可以调用该接口，此时scanType不能传1。
* 当进程已经被SMAP纳管时，须先停止冷热迁移，然后才可以调用该接口，scanType可以传0/1/2。
* scanType传1的情况为进程已被smap纳管，需要从只扫描状态恢复到冷热扫描加迁移状态。
* 当进程未被SMAP纳管时，只允许进程使用本地numa。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序将PID为10253的进程添加到扫描中，每50ms扫描一次，持续3s。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    pid_t pidArr[1] = { 10253 };
    uint32_t scanTime[1] = { 50 };
    uint32_t duration[1] = { 3 };

    ret = ubturbo_smap_process_tracking_add(pidArr, scanTime, duration, 1, 2);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_process_tracking_remove: 移除进程扫描

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_process_tracking_remove(pid_t *pidArr, int len, int flags);
```

## 描述 DESCRIPTION

通知SMAP移除进程扫描。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| pidArr | IN | 进程PID数组。 |
| len | IN | 数组长度。 |
| flags | IN | 保留字段。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 只有通过SmapAddProcessTracking接口设置的scanType为0或2的pid才能被这个接口移除。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序将PID为10253的进程从扫描中移除。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    pid_t pidArr[1] = { 10253 };

    ret = ubturbo_smap_process_tracking_remove(pidArr, 1, 0);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_migrate_out_sync: 配置进程迁出（同步）

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_migrate_out_sync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime);
```

## 描述 DESCRIPTION

通知SMAP调用内存同步迁出接口。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| msg | IN | 配置参数。 |
| msg.count | IN | 配置数量。 |
| msg.payload | IN | 配置数组。 |
| msg.payload.destNid | IN | 远端NUMA。 |
| msg.payload.pid | IN | 进程PID。 |
| msg.payload.ratio | IN | 迁出比例。 |
| msg.payload.memSize | IN | 迁出大小，单位KB。 |
| msg.payload.migrateMode | IN | 迁移模式，0：按比例， 1：按大小。 |
| pidType | IN | 进程页面类型，0：4K，1：2M。 |
| maxWaitTime | IN | 最大等待时间，单位ms。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EBUSY | 超时 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 只支持在虚拟化场景调用。
* 只支持内存池化场景。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序配置PID为10253的虚机进程迁出到NUMA4上，迁出比例为25%，最多等待60s。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    struct MigrateOutMsg msg = {
        .count = 1,
        .payload = {
            {
                .destNid = 4,
                .pid = 10253,
                .ratio = 25,
                .memSize = 0,
                .migrateMode = 0,
            }
        };
    };

    ret = ubturbo_smap_migrate_out_sync(&msg, 1, 60000);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_process_config_query: 根据远端NUMA查询进程配置

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
int ubturbo_smap_process_config_query(int nid, struct ProcessPayload *result, int inLen, int *outLen);
```

## 描述 DESCRIPTION

根据远端NUMA查询进程配置。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| nid | IN | 远端NUMA。 |
| result | OUT | 保存结果的数组。 |
| inLen | IN | 数组长度。 |
| outLen | OUT | 实际长度。 |

## 返回值 RETURN VALUE

返回 `0` 表示成功，返回其他值表示失败，请见`错误 ERRORS`

## 错误 ERRORS

| Error | Description |
| --- | --- |
| -EPERM | SMAP未初始化 |
| -EINVAL | 参数错误 |

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 切换场景时需要删除SMAP配置文件`/dev/shm/smap_config`。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序查询配置了远端NUMA为4的进程配置。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;
    struct ProcessPayload result[100] = { 0 };

    ret = ubturbo_smap_process_config_query(4, &result, 100, &len);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_urgent_migrate_out: 紧急迁移

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
void ubturbo_smap_urgent_migrate_out(uint64_t size);
```

## 描述 DESCRIPTION

紧急迁移。

## 参数 Parameters

| name | IN/OUT | description |
| --- | --- | --- |
| size | IN | 内存迁移量，单位为字节。 |

## 返回值 RETURN VALUE

不涉及。

## 错误 ERRORS

不涉及。

## 约束 CONSTRAINTS

* SMAP初始化后才能调用。
* 在OOM场景下由上层组件调用。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序紧急迁出1MB内存到远端。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    int ret;

    ret = ubturbo_smap_urgent_migrate_out(1048576);
    if (ret != 0) {
        return ret;
    }

    return 0;
}
```

# ubturbo_smap_is_running: 查询SMAP的运行状态

## 库 LIBRARY

SMAP库 (libsmap.so)

## 摘要 SYNOPSIS

```c
#include "smap_interface.h"
bool ubturbo_smap_is_running(void);
```

## 描述 DESCRIPTION

查询SMAP是否正在运行。

## 参数 Parameters

不涉及。

## 返回值 RETURN VALUE

返回 `true` 正在运行，返回`false`表示未运行。

## 错误 ERRORS

不涉及。

## 约束 CONSTRAINTS

不涉及。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序查询了SMAP的运行状态。

```c
#include <stdio.h>
#include "smap_interface.h"

int main(void)
{
    bool ret;

    ret = ubturbo_smap_is_running();

    return 0;
}
```
