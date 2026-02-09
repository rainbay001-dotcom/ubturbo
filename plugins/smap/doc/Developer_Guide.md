### ubturbo_smap_start

#### 函数定义

初始化SMAP，设置场景及迁移页面类型，虚拟化场景对应2M大页迁移，通算场景对应4K页迁移。

#### 实现方法

<pre class="screen"><p class="p" id="p294571314185">int ubturbo_smap_start(uint32_t pageType, Logfunc extlog);</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| pageType | uint32\_t | {0,1} | 入参 | 页面类型：0：4K页。1：2M页。 |
| extlog | typedef void (\*Logfunc)(int level, const char \*str, const char \*moduleName); | 与数据类型匹配 | 入参 | Logfunc日志函数。 |

#### 返回值

* 成功返回0。
* 同进程重复初始化返回-1。
* 其它进程已初始化返回-13。
* SMAP初始化异常返回-9 。
* 内存申请失败返回-12。
* SMAP内核驱动未安装返回-19。
* 参数错误返回-22。
* SMAP日志初始化失败返回-5。

#### 约束和注意事项

* 不能重复初始化。
* pageType需要和当前场景匹配。

### ubturbo_smap_stop

#### 约束和注意事项

初始化SMAP才能调用停止SMAP。

#### 函数定义

停止SMAP，释放资源（包含移除管理的pid迁出列表、enable状态位等）。

#### 实现方法

<pre class="screen"><p class="p" id="p199921138182">int ubturbo_smap_stop(void);</p></pre>

#### 参数说明

N/A

#### 返回值

* 成功返回0。
* 已停止返回-1。

### ubturbo_smap_migrate_out

#### 函数定义

配置虚拟机/进程的远端NUMA和迁出比例。

#### 实现方法

<pre class="screen"><p class="p" id="p987171415182">int ubturbo_smap_migrate_out(struct MigrateOutMsg *msg, int pidType);</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| msg | struct MigrateOutMsg\* | * 单次最多配置40个pid。SMAP可管理的最大虚机个数为100个，最大4K进程个数为300个。* 配置的虚机PID重复时参数错误。* 远端NUMA需要存在。* 迁出比例有效值为[0-100]。* 迁出内存量memSize单位KB，必须为2MB的倍数* PID对应进程名在黑名单中时参数错误。* 虚拟化内存水线场景下，迁出比例非精确的迁出比例，其含义为pid可迁出到远端NUMA的内存比例，SMAP会根据SetSmapRemoteNumaInfo接口传入的借用内存量进行调整；在多虚机场景也会根据虚机的冷热信息调整实际迁出比例。* 内存碎片场景下，migrateMode只能是按照memSize迁移。在保证迁出内存足够的情况下，迁出比例含义为pid总的实际使用内存的迁出比例，迁移大小为pid总的实际使用内存的迁出大小（KB）。 | 入参 | 详细如下 |
| pidType | int | {0,1} | 入参 | int配置类型：* 0-进程（4K）* 1-虚机（2M） |

```
#define MAX_NR_MIGOUT 40 

struct MigrateOutPayloadInner {
    int destNid;
    int ratio;
    uint64_t memSize;
    MigrateMode migrateMode;
};

struct MigrateOutPayload {
    int srcNid;
    pid_t pid;
    int count;
    struct MigrateOutPayloadInner inner[REMOTE_NUMA_NUM];
};

struct MigrateOutMsg {
    int count;
    struct MigrateOutPayload payload[MAX_NR_MIGOUT];
};
```

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 内存申请失败返回-12。
* 参数错误返回-22。

#### 约束和注意事项

* SMAP初始化后才能调用。
* pidType需要和当前场景匹配。
* HCCS代际远端NUMA最大值为17。
* UB代际远端NUMA最大值为17。
* 远端NUMA被禁用时无法配置迁出（调用SmapMigrateBack接口时会默认禁用远端NUMA）。
* 如果已配置某虚机的远端NUMA，后续配置不能改变虚机的远端NUMA，只能通过SmapMigratePidRemoteNuma接口改变远端NUMA。
* 配置pid内存迁出后，由SMAP线程异步迁移，在迁移周期到来时才会执行迁移操作。

### ubturbo_smap_remote_numa_info_set

#### 函数定义

通知SMAP，本地NUMA向远端NUMA借用的内存用量。

#### 实现方法

<pre class="screen"><p class="p" id="p41243146185">int ubturbo_smap_remote_numa_info_set(struct SetRemoteNumaInfoMsg *msg);</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| msg | struct SetRemoteNumaInfoMsg\* | * srcNid为本地NUMA id或者为-1，其它返回异常。* destNid为本节点远端NUMA id，非本节点远端NUMA id返回异常。* size为本地srcNid对应在远端NUMA上借用的内存量，单位MB，若srcNid为-1，则代表所有本地NUMA可共享这段内存。 | 入参 | 详细如下 |

```
struct SetRemoteNumaInfoMsg {
    int srcNid;
    int destNid;
    uint32_t size;
};
```

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 参数错误返回-22。

#### 约束和注意事项

* SMAP初始化后才能调用。
* HCCS代际远端NUMA最大值为17。
* UB代际远端NUMA最大值为17。
* 如果已配置某虚机的远端NUMA，后续配置不能改变虚机的远端NUMA，只能通过SmapMigratePidRemoteNuma接口改变远端NUMA。
* 当配置的pid可迁出的量大于借用内存量时，SMAP不会使用完所有的借用量，每个本地NUMA对应的远端借用都有MIN[借用量的5%，200MB]的量不会使用，这是为了迁移内存时能申请到新的内存页面。例如：numa0 numa1分别借用了2G和6G共8G，numa0借的2G的预留按5%来算是100M，numa1借的6G的预留是200M，合计一共300M。
* 此接口仅在水线场景中生效，且水线场景调用SmapMigrateOut接口前需通过此接口设置远端NUMA使用量才能迁出pid的内存。
* 如果未调用SetSmapRemoteNumaInfo接口，默认初始化size值为0。

### ubturbo_smap_migrate_back

#### 函数定义

将指定远端NUMA的内存迁移到同一远端NUMA的其他地址段。

#### 实现方法

<pre class="screen"><p class="p" id="p9161121421813">int SmapMigrateBack(struct MigrateBackMsg *msg);</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| msg | struct MigrateBackMsg\* | * 远端NUMA有效值范围为[0,9]，且是远端NUMA。* 远端NUMA和传入的地址段需要匹配。 | 入参 | 详细如下 |

```
#define MAX_NR_MIGBACK 50

struct MigrateBackPayload { 
    int srcNid; 
    int destNid; 
     uint64_t paStart; 
    uint64_t paEnd; 
};

struct MigrateBackMsg { 
    unsigned long long taskID; 
    int count; 
    struct MigrateBackPayload payload[MAX_NR_MIGBACK]; 
};
```

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 系统调用失败返回-9。
* 参数错误返回-22。
* 超时返回-11。

#### 约束和注意事项

* SMAP初始化后才能调用。
* 不支持并发调用此接口，否则会引起内存归还失败。
* 远端NUMA和传入的地址段需要匹配。
* 调用此接口后，SMAP默认禁止指定远端NUMA的冷热流动，只允许迁回任务中的迁移。
* 若远端NUMA的其他地址段的空闲页面不够，迁移任务会失败。
* 虚拟化水线场景下，请调用此接口前，调用SetSmapRemoteNumaInfo接口通知SMAP更新借用内存量，保证远端NUMA有足够的内存空间。
* 内存碎片场景下，调用此接口，需由调用方预留足够内存空间，否则会迁回失败。
* 迁回任务为异步执行，执行状态在/sys/kernel/debug/smap/mb\_[taskID]中进行查询。
* 同一个远端NUMA不支持并发调用，并发调用可能导致迁移数据无法迁移干净。

### ubturbo_smap_remove

#### 函数定义

移除指定的虚机/进程的远端numa，当远端numa全被移除时，整个进程被移除SMAP管理。

#### 实现方法

<pre class="screen"><p class="p" id="p3301101481814">int ubturbo_smap_remove(struct RemoveMsg *msg, int pidType);</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| msg | struct RemoveMsg\* | * 单次最多移除的虚机/进程数量为40。 | 入参 | 详细如下|
| pidType | int | {0,1} | 入参 | int配置类型：* 0-进程（4K）* 1-虚机（2M） |

```
#define MAX_NR_REMOVE 40

struct RemovePayload {
    pid_t pid;
    int count;
    int nid[REMOTE_NUMA_NUM];
};

struct RemoveMsg { 
    int count; 
    struct RemovePayload payload[MAX_NR_REMOVE]; 
};
```

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* SMAP处理异常返回-9。
* 参数错误返回-22。
* 申请内存失败返回-12。

#### 约束和注意事项

* SMAP初始化后才能调用。
* pidType需要和当前场景匹配。
* 当调用SmapMigrateBack接口迁回完所有地址后，需使用SmapRemove接口移除虚机管理，保证后续流程正常。

### ubturbo_smap_node_enable

#### 函数定义

启用NUMA迁入，允许其它NUMA的内存向该NUMA进行迁移。

#### 实现方法

<pre class="screen"><p class="p" id="p183398143187">int ubturbo_smap_node_enable(struct EnableNodeMsg *msg);</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| msg | struct EnableNodeMsg\* | 传入NUMA最大值为17，且为远端NUMA。 | 入参 | 详细如下 |

```
struct EnableNodeMsg { 
    int enable; 
    int nid;
 };
```

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 参数错误返回-22。

#### 约束和注意事项

* SMAP初始化后才能调用。
* 此接口与SmapMigrateBack接口配合使用，目的是在SmapMigrateBack接口调用后，恢复对应远端NUMA的冷热流动。

### ubturbo_smap_freq_query

#### 函数定义

查询进程冷热信息。

#### 实现方法

<pre class="screen"><p class="p" id="p1897593015511">int ubturbo_smap_freq_query(int pid, uint16_t *data, uint32_t lengthIn, uint32_t *lengthOut);</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- |--- | --- |--- |
| pid | int | 对应pid需要存在。 | 入参 | 进程pid号。 |
| data | uint16\_t\* | 非空。 | 入参 | 存放冷热信息的数组。 |
| lengthIn | uint32\_t | 大于0 | 入参 | 指示data数组的大小。 |
| lengthOut | uint32\_t\* | 非空。 | 入参 | 返回实际写入data数组的大小。 |

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 参数错误返回-22。

#### 约束和注意事项

* SMAP初始化后才能调用。
* dataSource为0表示先调用ubturbo_smap_migrate_out接口, 后续可获取到最近一个周期的冷热数据。

### ubturbo_smap_run_mode_set

#### 函数定义

设置SMAP运行模式。

#### 实现方法

<pre class="screen"><p class="p" id="p14201739155519">int ubturbo_smap_run_mode_set(int runMode);</p></pre>

#### 参数说明

**表1 ​**参数说明| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| - | - | - | - | - |
| ------------------------------------------------ |

| runMode | int | * 0：水线场景。* 1：内存碎片场景。* 其它值：返回错误。 | 入参 | 设置SMAP的运行模式，当前包括水线场景，内存碎片场景。 |
| - | - | - | - | - |

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 参数错误返回-22。
* 同步配置文件失败返回-9。

#### 约束和注意事项

* SMAP初始化后才能调用。
* 未设置的情况下，默认为水线场景。
* 如果是4K场景，不支持设置SMAP运行模式为内存碎片模式。

### ubturbo_smap_process_migrate_enable

#### 函数定义

启用/禁用PID对应虚机的冷热迁移和迁回。

#### 实现方法

<pre class="screen" id="screen159847188323"><p class="p" id="p18353828165616">int ubturbo_smap_process_migrate_enable(pid_t *pidArr, int len, int enable, int flags);</p></pre>

#### 参数说明

**表1 ​**参数说明| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| - | - | - | - | - |
| ------------------------------------------------ |

| pidArr | pid\_t \* | NA | 入参 | 虚机PID数组。 |
| - | - | - | - | - |
| len | int | 1-100的整数 | 入参 | 虚机PID数组长度。 |
| enable | int | 0或1 | 入参 | * 0-禁用。* 1-启用。 |
| flags | int | NA | 入参 | 保留字段。 |

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 参数错误返回-22。
* 超时返回-110。

#### 约束和注意事项

* flags为保留字段，暂未使用。

### ubturbo_smap_remote_numa_migrate

#### 函数定义

通知SMAP迁移远端NUMA的内存到另一个远端NUMA。

#### 实现方法

<pre class="screen" id="screen29172049124214"><p class="p" id="p522512464561">int ubturbo_smap_remote_numa_migrate(struct MigrateNumaMsg *msg);</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| msg | struct MigrateNumaMsg \* | NA | 入参 | 迁移远端NUMA的消息。 |
| msg.srcNid | int | 远端NUMA ID | 入参 | 源NUMA ID。 |
| msg.destNid | int | 远端NUMA ID | 入参 | 目的NUMA ID。 |
| msg.count | int | 1-50的整数 | 入参 | 源NUMA地址段数量。 |
| msg.payload | struct MigrateNumaPayload | 长度固定为50 | 入参 | 地址段信息。 |
| msg.payload[].paStart | uint64\_t | NA | 入参 | 地址段起始地址。 |
| msg.payload[].paEnd | uint64\_t | NA | 入参 | 地址段结束地址。 |

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 迁移成功但修改进程远端NUMA失败返回-9。
* 迁移失败返回-12。
* 参数错误返回-22。

#### 约束和注意事项

* 传入的地址段需要和源NUMA ID对应的地址段匹配。
* 调用该接口前必须调用SmapEnableProcessMigrate禁用pid迁移功能。

### ubturbo_smap_pid_remote_numa_migrate

#### 函数定义

通知SMAP按PID级迁移远端内存到其他远端内存。

#### 实现方法

<pre class="screen"><p class="p" id="p53951577571">int ubturbo_smap_pid_remote_numa_migrate(struct MigrateEscapeMsg *msg);</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| msg | struct MigrateEscapeMsg \* | NA | 入参 | 迁移PID远端NUMA的消息。|
| msg.count | int | 1-300的整数 | 入参 | 进程数量。|
| msg.payload | struct MigrateNumaPayload | 长度固定为300 | 入参 | 进程迁移配置 |
| msg.payload[].pid | pid\_t | NA | 入参 | 进程PID |
| msg.payload[].srcNid | int | NA | 入参 | PID源远端NUMA |
| msg.payload[].destNid | int| NA | 入参 | PID目的端远端NUMA |
| msg.payload[].ratio | int| NA | 入参 | 迁移比例 |
| msg.payload[].srcNid | memSize\_t | NA | 入参 | 迁移大小 |
| msg.payload[].migrateMode | int | NA | 入参 | 迁移模式 |

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 迁移成功但修改进程远端NUMA失败返回-9。
* srcNid不是PID的源远端NUMA，返回-6。
* 参数错误返回-22。
* 内存申请失败返回-12。

#### 约束和注意事项

* 目的NUMA ID内存余量充足。
* 不支持重复调用。
* 调用该接口前必须调用SmapEnableProcessMigrate禁用pid迁移功能。

### ubturbo_smap_process_tracking_add

#### 函数定义

通知SMAP添加进程扫描，并设置扫描周期参数。

#### 实现方法

<pre class="screen"><p class="p" id="p181653418315">int ubturbo_smap_process_tracking_add(pid_t *pidArr, uint32_t *scanTime, uint32_t *duration, int len, int scanType)</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| pidArr | pid\_t \* | NA | 入参 | PID数组。 |
| scanTime | uint32_t \* | 50毫秒的倍数，最小值50毫秒，最大值为200毫秒。 | 入参 | 扫描周期数组。 |
| duration | uint32_t \* | 与len长度相符的扫描周期数组，取值[1,300]，单位秒，在scanType=2时使用。 | 入参 | 访存数据统计时长。 |
| len | int | 1-40整数。 | 入参 | PID数组长度。 |
| scanType | int | {0,1,2} | 入参 | * 0：将进程设置为只扫描状态，此时调用[/proc/ {PID}_t/tracking_info](#proc-pid_tracking_info)获取扫描频次信息。* 1：将进程恢复为冷热扫描加迁移状态。* 2：表示进程设置为统计特定时长冷热信息状态。 |

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 参数错误返回-22。
* 内核态内存申请失败返回-9。
* 用户态内存申请失败返回-12。

#### 约束和注意事项

* scanType为0或1时只支持虚机场景
* 当进程未被SMAP纳管时，可以调用该接口，此时scanType不能传1。
* 当进程已经被SMAP纳管时，须先停止冷热迁移，然后才可以调用该接口，scanType可以传0/1/2。
* scanType传1的情况为进程已被smap纳管，需要从只扫描状态恢复到冷热扫描加迁移状态。
* 当进程未被SMAP纳管时，只允许进程使用本地numa。

### ubturbo_smap_process_tracking_remove

#### 函数定义

通知SMAP移除进程扫描。

#### 实现方法

<pre class="screen"><p class="p" id="p12683629436">int ubturbo_smap_process_tracking_remove(pid_t *pidArr, int len, int flags)</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| pidArr | pid\_t \* | NA | 入参 | PID数组。 |
| len | int | 1-100整数 | 入参 | PID数组长度。 |
| flags | int | NA | 入参 | 保留字段。 |

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 参数错误返回-22。

#### 约束和注意事项

* 只有通过SmapAddProcessTracking接口设置的flag为0的pid才能被这个接口移除。

<a id="proc-pid_tracking_info"></a>
### /proc/ {PID}\_t/tracking\_info

#### 函数定义

通过文件方式查询虚机的访问频次信息。pid为{PID}的虚机的访存频次文件位置为：/proc/{PID}\_t/tracking\_info

#### 实现方法

<pre class="screen"><p class="p" id="p863415451833">// 第一次调用读取数量</p><p class="p" id="p1663494510312">int fread(int *num, sizeof(int), int n, FILE *file)</p><p class="p" id="p156343451733">// 第二次调用读取物理地址和频次信息</p><p class="p" id="p1863420453318">int fread(struct FreqInfo *info, sizeof(FreqInfo), int n, FILE *file)</p></pre>

**表1 ​**参数说明
| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| - | - | - | - | - |
| FreqInfo | struct FreqInfo {u64 paddr;u16 freq;} | NA | 入参 | 存储频次信息结构体。 |

#### 返回值

* 读取成功返回频次个数以及频次信息。
* 读取失败返回errno。

#### 约束和注意事项

* 只支持虚拟化场景下，成功添加仅扫描模式的虚机到SMAP后调用。

### ubturbo_smap_migrate_out_sync

#### 函数定义

通知SMAP调用内存同步迁出接口。

#### 实现方法

<pre class="screen"><p class="p" id="p7119930419">int ubturbo_smap_migrate_out_sync(struct MigrateOutMsg *msg, int pidType, uint64_t maxWaitTime)</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| msg | struct MigrateOutMsg \* | NA | 入参 | 迁移信息。 |
| msg.count | int | 1-40的整数。 | 入参 | 迁移数量。 |
| msg.payload[].pid | pid\_t | NA | 入参 | 进程pid。 |
| msg.payload[].ratio | int | 0-100的整数。 | 入参 | 迁移比例。 |
| msg.payload[].destNid | int | 大于等于本地NUMA数量小于10。 | 入参 | 目的NUMA ID。 |
| msg.payload[].memSize | uint64\_t | 单位为KB，必须为2MB的整数倍 | 入参 | 内存迁移大小 |
| msg.payload[].migrateMode | MigrateMode | 枚举类型：枚举值为MIG\_RATIO\_MODE = 0， MIG\_MEMSIZE\_MODE = 1 | 入参 | MIG\_RATIO\_MODE表示按照比例迁移，MIG\_MEMSIZE\_MODE表示按照内存大小迁移 |
| pidType | int | 1 | 入参 | 进程pid类型，1表示虚机类型。 |
| maxWaitTime | uint64\_t | 10s-1min(单位ms) | 入参 | 一次调用最大等待时间。 |

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 参数错误（包含非法入参、非内存池化场景）返回-22。
* 等待超时返回-16。
* 内存申请失败返回-12。
* pid无效返回-3。

#### 约束和注意事项

* 只支持在虚拟化场景调用。
* 只支持内存池化场景。

### ubturbo_smap_process_config_query

#### 函数定义

查询SMAP进程配置的接口。

#### 实现方法

<pre class="screen" id="ZH-CN_TOPIC_0000002255419832__screen29172049124214"><p class="p" id="p12997132118410">int ubturbo_smap_process_config_query(int nid, struct ProcessPayload *result, int inLen, int *outLen)</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| nid | int | 有效的远端NUMA | 入参 | 远端NUMA NID |
| result | struct ProcessPayload | 非空数组 | 出参 | 保存结果的数组 |
| result[].pid | pid\_t | NA | 出参 | 进程pid |
| result[].ratio | uint8\_t | NA | 出参 | 进程内存本地比例 |
| result[].scanType | uint8\_t | NA | 出参 | 进程扫描类型 |
| result[].type | uint8\_t | NA | 出参 | 进程类型，0-PROCESS\_TYPE，1-VM\_TYPE |
| result[].state | uint8\_t | NA | 出参 | 进程状态，0-空闲，1-冷热迁移，2-迁回，3-远端搬迁 |
| result[].l1Node[4] | int16\_t | NA | 出参 | 进程L1 Node |
| result[].l2Node[4] | int16\_t | NA | 出参 | 进程L2 Node |
| result[].scanTime | uint32\_t | NA | 出参 | 进程扫描间隔 |
| inLen | int | 与数组长度一致， 虚机场景小于等于100，普通进程场景小于等于300 | 入参 | 数组长度 |
| outLen | int \* | 非空整型指针 | 出参 | 进程数量 |

#### 返回值

* 成功返回0。
* SMAP未初始化返回-1。
* 参数错误返回-22。

#### 约束和注意事项

* 切换场景时需要删除SMAP配置文件/dev/shm/smap\_config。

### ubturbo_smap_urgent_migrate_out

#### 函数定义

SMAP紧急迁移接口。

#### 实现方法

<pre class="screen"><p class="p" id="p921733917413">void ubturbo_smap_urgent_migrate_out(uint64_t size)</p></pre>

#### 参数说明

| 参数名 | 数据类型 | 有效性规格 | 参数类型 | 描述 |
| --- | --- | --- | --- | --- |
| size | uint64\_t | NA | 入参 | 内存迁移量，单位为字节 |

#### 返回值

* 无返回值。

#### 约束和注意事项

* 在OOM场景下由上层组件调用。
