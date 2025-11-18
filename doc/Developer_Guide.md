# 1 进程外使用：调用UBTurbo集成的SMAP能力

- 使用方加载`libubturbo_client.so`动态库，例如dlopen机制；
- 调用动态库中的SMAP接口，执行冷热信息查询、多级内存调度等功能；

# 2 进程内使用：运行现有插件

**说明：**

- 默认不启用任何插件，请依据业务场景在ubturbo_plugin_admission.conf中打开插件（取消对应插件的注释）.
- 在ubturbo_plugin_admission.conf中打开插件前，需保证插件的动态库和配置文件已分别放置在/opt/ubturbo/lib和/opt/ubturbo/conf下，否则UBTurbo及对应插件将启动异常.
- 插件准入配置文件ubturbo_plugin_admission.conf的说明如下：
  
  - 只有在该准入配置中配置的插件才会被加载并初始化，未在该文件中配置的插件将不予加载
  - 配置项中key对应插件的插件名称(需要与插件自身配置文件中的名称相对应)，value为初始化函数中需要使用的moduleCode
  - moduleCode是唯一值
  - 进程启动成功后，通过执行以下命令来查询哪些插件已加载:
    `cat /var/log/ubturbo/turbo.log | grep "loaded successfully"`
**rmrs插件示例：**

| 序号 | 参数 | 说明 | 取值 | 配置节点 | 应用场景 |
|-----|-----|-----|-----|-----|-----|
| 1 | rmrs | 内存碎片插件 | 默认值：777 | 所有节点 | 通算虚拟化场景 |

# 3 进程内使用：基于UBTurbo框架及其IPC通信能力，开发新插件，并对外提供服务

## 3.1 实现`TurboPluginInit`

UBTurbo启动时调用该函数对插件进行初始化。

```c
uint32_t TurboPluginInit(const uint16_t modCode)
```

#### 参数说明

modCode：插件的唯一编码

#### 返回值

* 成功返回0，否则返回非0

#### 约束和注意事项

该函数由插件自己实现，完成自己的初始化工作。

## 3.2 实现`TurboPluginDeInit`

UBTurbo停止时调用该函数对插件进行反初始化。

```c
void TurboPluginDeInit()
```

#### 参数说明

NULL

#### 返回值

NULL

#### 约束和注意事项

该函数由插件自己实现，完成自己的反初始化工作。

## 3.3 调用接口`UBTurboRegIpcService`注册IPC回调函数

| 列1       | 列2       |
|-----------|-----------|
| 接口名称     | UBTurboRegIpcService     |
| 接口描述     | 服务端注册回调函数     |
| Input要求/参数     | 1. 函数名称 const std::string &name<br>2. 回调函数 IpcHandlerFunc function  |
| Output要求/参数     | 执行结果，成功时返回0，失败时返回非0错误码|
| 约束和注意事项     |  同名函数不能重复注册    |
| 接口使用样例     | RetCode UBTurboRegIpcService(const std::string &name, IpcHandlerFunc function);|
| 参数有效性规格     |入参说明<br>参数1：name<br>说明：服务端回调函数的名称，长度不超过128且无空格<br>参数2：function<br>说明：服务端用于处理客户端请求的回调函数, 类型为std::function<uint32_t(const TurboByteBuffer &inputBuffer, TurboByteBuffer &outputBuffer)>, 不能为空。其中inputBuffer为客户端发给服务端的数据，outputBuffer为服务端返回给客户端的数据，类型为<br>struct TurboByteBuffer {<br>uint8_t *data=nullptr;<br>size_t len=0;<br>TurboByteBufferFreeFunc freeFunc;<br>}<br>data为数据指针，len为数据长度，freeFunc为释放内存的回调函数<br>|

## 3.4 调用接口`UBTurboUnRegIpcService`解注册IPC回调函数

| 列1       | 列2       |
|-----------|-----------|
| 接口名称     | UBTurboUnRegIpcService |
| 接口描述     | 服务端解注册回调函数  |
| Input要求/参数     | 1. 函数名称 std::string name  |
| Output要求/参数     | 执行结果，成功时返回0，失败时返回非0错误码 |
| 约束和注意事项     |  解注册的函数需已注册过 |
| 接口使用样例     | RetCode UBTurboUnRegIpcService(const std::string &name); |
| 参数有效性规格     | 入参说明<br>参数1：函数名称<br>说明：服务端回调函数的名称，长度不超过128且无空格<br> |

## 3.5 外部通过接口`UBTurboFunctionCaller`调用插件提供的能力

| 列1       | 列2       |
|-----------|-----------|
| 接口名称     | UBTurboFunctionCaller     |
| 接口描述     | 客户端通过进程间通信调用服务端函数     |
| Input要求/参数     | 1. 函数名称 const std::string &function<br>2. 发送数据 const TurboByteBuffer ¶ms<br>3. 接受数据 TurboByteBuffer &result  |
| Output要求/参数     | UBTurboFunctionCaller返回值只代表ipc通信流程是否成功，业务逻辑的返回值应封装在报文中。返回值为0代表函数调用成功，返回的数据有效，由调用者负责内存释放。否则无效，返回的数据为空指针。<br>执行结果<br>0：执行成功<br>1：通用错误码<br>2：socket失效<br>3：连接断开<br>4：服务端无对应函数<br>5：服务端对应函数执行错误<br>6：服务端对应函数出参范围异常<br>|
| 约束和注意事项     |  调用的函数应已在服务端进行注册    |
| 接口使用样例     | uint32_t UBTurboFunctionCaller(const std::string &function, const TurboByteBuffer ¶ms, TurboByteBuffer &result);|
| 参数有效性规格     |入参说明<br>参数1：function<br>说明：客户端调用服务端函数的名称，长度不超过128且无空格<br>参数2：params<br>说明：客户端发送给服务端的数据，由调用方进行序列化，数据类型为<br>struct TurboByteBuffer {<br>uint8_t *data=nullptr;<br>size_t len=0;<br>TurboByteBufferFreeFunc freeFunc;<br>}<br>data为数据指针，len为数据长度，freeFunc为释放内存的回调函数。len应大于等于0，小于等于1048576。若data为空，len必须同时为0。data需要由调用者在调用完成后手动释放。<br>参数3：返回数据<br>说明：服务端返回给客户端的数据，由调用方进行反序列化，数据类型为struct TurboByteBuffer。len应大于等于0，小于等于1048576。若返回值为0，则需要调用方手动释放data。<br>|

## 3.6 日志打印接口

| 列1       | 列2       |
|-----------|-----------|
| 接口名称     | UBTURBO_LOG_CRIT |
| 接口描述     | 创建单条CRIT级别日志，写到终端或写入文件     |
| Input要求/参数     | 1. 模块名 const char * gModuleName<br>2. 模块id int gModuleId <br>3. 打印的信息 可变参数args<br>  |
| Output要求/参数     | 执行结果，成功时返回true，失败时返回false，打印的内容输出到日志文件中，格式为<br>[时间][CRIT][进程号][线程号][代码所在文件及行数]日志信息<br> |
| 约束和注意事项     |  无    |
| 接口使用样例     | UBTURBO_LOG_CRIT(moduleName, moduleId) << args; |
| 参数有效性规格     |入参说明<br>参数1：moduleName<br>说明：打印日志的模块名称， 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 <br>参数2：moduleId<br>说明：打印日志的模块id，moduleid作为保留字段，当前无意义 <br>参数3：args<br>说明：需要打印的信息，通过重载operator <<输出流实现打印<br> |

| 列1       | 列2       |
|-----------|-----------|
| 接口名称     | UBTURBO_LOG_ERROR |
| 接口描述     | 创建单条ERROR级别日志，写到终端或写入文件     |
| Input要求/参数     | 1. 模块名 const char * gModuleName<br>2. 模块id int gModuleId <br>3. 打印的信息 可变参数args<br>  |
| Output要求/参数     | 执行结果，成功时返回true，失败时返回false，打印的内容输出到日志文件中，格式为<br>[时间][ERROR][进程号][线程号][代码所在文件及行数]日志信息<br> |
| 约束和注意事项     |  无    |
| 接口使用样例     | UBTURBO_LOG_ERROR(moduleName, moduleId) << args; |
| 参数有效性规格     |入参说明<br>参数1：moduleName<br>说明：打印日志的模块名称， 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 <br>参数2：moduleId<br>说明：打印日志的模块id， moduleid作为保留字段，当前无意义 <br>参数3：args<br>说明：需要打印的信息，通过重载operator <<输出流实现打印<br> |

| 列1       | 列2       |
|-----------|-----------|
| 接口名称     | UBTURBO_LOG_WARN|
| 接口描述     | 创建单条WARN级别日志，写到终端或写入文件     |
| Input要求/参数     | 1. 模块名 const char * gModuleName<br>2. 模块id int gModuleId <br>3. 打印的信息 可变参数args<br>  |
| Output要求/参数     | 执行结果，成功时返回true，失败时返回false，打印的内容输出到日志文件中，格式为<br>[时间][WARN][进程号][线程号][代码所在文件及行数]日志信息<br> |
| 约束和注意事项     |  无    |
| 接口使用样例     | UBTURBO_LOG_WARN(moduleName, moduleId) << args; |
| 参数有效性规格     |入参说明<br>参数1：moduleName<br>说明：打印日志的模块名称， 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 <br>参数2：moduleId<br>说明：打印日志的模块id<br>参数3：args<br>说明：需要打印的信息，通过重载operator <<输出流实现打印<br> |

| 列1       | 列2       |
|-----------|-----------|
| 接口名称     | UBTURBO_LOG_INFO |
| 接口描述     | 创建单条INFO级别日志，写到终端或写入文件     |
| Input要求/参数     | 1. 模块名 const char * gModuleName<br>2. 模块id int gModuleId <br>3. 打印的信息 可变参数args<br>  |
| Output要求/参数     | 执行结果，成功时返回true，失败时返回false，打印的内容输出到日志文件中，格式为<br>[时间][INFO][进程号][线程号][代码所在文件及行数]日志信息<br> |
| 约束和注意事项     |  无    |
| 接口使用样例     | UBTURBO_LOG_INFO(moduleName, moduleId) << args; |
| 参数有效性规格     |入参说明<br>参数1：moduleName<br>说明：打印日志的模块名称， 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 <br>参数2：moduleId<br>说明：打印日志的模块id<br>参数3：args<br>说明：需要打印的信息，通过重载operator <<输出流实现打印<br> |

| 列1       | 列2       |
|-----------|-----------|
| 接口名称     | UBTURBO_LOG_DEBUG |
| 接口描述     | 创建单条DEBUG级别日志，写到终端或写入文件     |
| Input要求/参数     | 1. 模块名 const char * gModuleName<br>2. 模块id int gModuleId <br>3. 打印的信息 可变参数args<br>  |
| Output要求/参数     | 执行结果，成功时返回true，失败时返回false，打印的内容输出到日志文件中，格式为<br>[时间][DEBUG][进程号][线程号][代码所在文件及行数]日志信息<br> |
| 约束和注意事项     |  无    |
| 接口使用样例     | UBTURBO_LOG_DEBUG(moduleName, moduleId) << args; |
| 参数有效性规格     |入参说明<br>参数1：moduleName<br>说明：打印日志的模块名称， 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 <br>参数2：moduleId<br>说明：打印日志的模块id<br>参数3：args<br>说明：需要打印的信息，通过重载operator <<输出流实现打印<br> |
