# UBTurboGetUInt32: 获取指定的uint32类型配置项

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_conf.h"

uint32_t UBTurboGetUInt32(const std::string &section, const std::string &configKey, uint32_t &configValue);
```

## 描述 DESCRIPTION

获取指定的uint32类型配置项。

## 参数 Parameters

| name        | IN/OUT | description  |
| ----------- | ------ | ------------ |
| section     | IN     | 指定具体插件。要求格式为“plugin_<plugin_name>"，其中<plugin_name>必须是在ubturbo_plugin_admission.conf中指定的，例如“plugin_rmrs”。不符合要求的输入会导致获取配置项失败，返回错误码1。|
| configKey      | IN     | 配置项名称。必须是在plugin_<plugin_name>.conf中存在的配置项名称。否则返回错误1。配置文件中，1<= configKey长度 <= 256。 |
| configValue | OUT    | 配置项的值。执行成功时，configValue即为获取到的配置项的值。执行失败时，configValue不具有任何意义。配置文件中，1<= configValue长度 <= 256。 |

## 返回值 RETURN VALUE

返回值0：表示成功。

返回非0错误码：表示失败。

## 约束 CONSTRAINTS

- 配置项名称的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项的值的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项名称只能包含字母、数字、.、_、-
- 配置项的值只能包含字母、数字、.、_、-、:、,、/、;
- 执行成功时，configValue即为获取到的配置项的值
- 同一个配置文件中存在重复配置项则报错，且服务启动失败

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成获取指定的uint32类型配置项。

```cpp
#include "turbo_conf.h"

int main(void)
{
    const std::string section = "section";
    const std::string configKey = "configKey";
    uint32_t configVal;
    uint32_t ret = UBTurboGetUInt32(section, configKey, configVal);
    return 0;
}
```





# UBTurboGetFloat: 获取指定的float类型配置项

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_conf.h"

uint32_t UBTurboGetFloat(const std::string &section, const std::string &configKey, float &configValue);
```

## 描述 DESCRIPTION

获取指定的float类型配置项

## 参数 Parameters

| name        | IN/OUT | description  |
| ----------- | ------ | ------------ |
| section     | IN     | 指定具体插件。要求格式为“plugin_<plugin_name>"，其中<plugin_name>必须是在ubturbo_plugin_admission.conf中指定的，例如“plugin_rmrs”。不符合要求的输入会导致获取配置项失败，返回错误码1。|
| configKey      | IN     | 配置项名称。必须是在plugin_<plugin_name>.conf中存在的配置项名称。否则返回错误1。配置文件中，1<= configKey长度 <= 256。 |
| configValue | OUT    | 配置项的值。执行成功时，configValue即为获取到的配置项的值。执行失败时，configValue不具有任何意义。配置文件中，1<= configValue长度 <= 256。 |

## 返回值 RETURN VALUE

返回值0：表示成功。

返回非0错误码：表示失败。

## 约束 CONSTRAINTS

- 支持指数形式，例如-1.2e3、1E-4。
- 配置项名称的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项的值的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项名称只能包含字母、数字、.、_、-
- 配置项的值只能包含字母、数字、.、_、-、:、,、/、;
- 执行成功时，configValue即为获取到的配置项的值
- 同一个配置文件中存在重复配置项则报错，且服务启动失败

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成获取指定的float类型配置项。

```cpp
#include "turbo_conf.h"

int main(void)
{
    const std::string section = "section";
    const std::string configKey = "configKey";
    float configValue;
    uint32_t ret = UBTurboGetFloat(section, configKey, configValue);
    return 0;
}
```



# UBTurboGetStr: 获取指定的string类型配置项

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_conf.h"

uint32_t UBTurboGetStr(const std::string &section, const std::string &configKey, std::string &configValue);
```

## 描述 DESCRIPTION

获取指定的string类型配置项

## 参数 Parameters

| name        | IN/OUT | description  |
| ----------- | ------ | ------------ |
| section     | IN     | 指定具体插件。要求格式为“plugin_<plugin_name>"，其中<plugin_name>必须是在ubturbo_plugin_admission.conf中指定的，例如“plugin_rmrs”。不符合要求的输入会导致获取配置项失败，返回错误码1。|
| configKey      | IN     | 配置项名称。必须是在plugin_<plugin_name>.conf中存在的配置项名称。否则返回错误1。配置文件中，1<= configKey长度 <= 256。 |
| configValue | OUT    | 配置项的值。执行成功时，configValue即为获取到的配置项的值。执行失败时，configValue不具有任何意义。配置文件中，1<= configValue长度 <= 256。 |

## 返回值 RETURN VALUE

返回值0：表示成功。

返回非0错误码：表示失败。

## 约束 CONSTRAINTS

- 配置项名称的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项的值的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项名称只能包含字母、数字、.、_、-
- 配置项的值只能包含字母、数字、.、_、-、:、,、/、;
- 执行成功时，configValue即为获取到的配置项的值
- 同一个配置文件中存在重复配置项则报错，且服务启动失败
- 配置项的值前导、后导空白字符均会被删除

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成获取指定的string类型配置项。

```cpp
#include "turbo_conf.h"

int main(void)
{
    const std::string section = "section";
    const std::string configKey = "configKey";
    std::string configValue;
    uint32_t ret = UBTurboGetStr(section, configKey, configValue);
    return 0;
}
```



# UBTurboGetBool: 获取指定的bool类型配置项

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_conf.h"

uint32_t UBTurboGetBool(const std::string &section, const std::string &configKey, bool &configValue);
```

## 描述 DESCRIPTION

获取指定的bool类型配置项

## 参数 Parameters

| name        | IN/OUT | description  |
| ----------- | ------ | ------------ |
| section     | IN     | 指定具体插件。要求格式为“plugin_<plugin_name>"，其中<plugin_name>必须是在ubturbo_plugin_admission.conf中指定的，例如“plugin_rmrs”。不符合要求的输入会导致获取配置项失败，返回错误码1。|
| configKey      | IN     | 配置项名称。必须是在plugin_<plugin_name>.conf中存在的配置项名称。否则返回错误1。配置文件中，1<= configKey长度 <= 256。 |
| configValue | OUT    | 配置项的值。执行成功时，configValue即为获取到的配置项的值。执行失败时，configValue不具有任何意义。配置文件中，1<= configValue长度 <= 256。 |

## 返回值 RETURN VALUE

返回值0：表示成功。

返回非0错误码：表示失败。

## 约束 CONSTRAINTS

- 配置项的值支持true/false、0/1、yse/no，对大小写不敏感
- 配置项名称的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项的值的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项名称只能包含字母、数字、.、_、-
- 配置项的值只能包含字母、数字、.、_、-、:、,、/、;
- 执行成功时，configValue即为获取到的配置项的值
- 同一个配置文件中存在重复配置项则报错，且服务启动失败

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成获取指定的bool类型配置项。

```cpp
#include "turbo_conf.h"

int main(void)
{
    const std::string section = "section";
    const std::string configKey = "configKey";
    bool configValue;
    uint32_t ret = UBTurboGetBool(section, configKey, configValue);
    return 0;
}
```



# UBTurboGetUInt64: 获取指定的uint64类型配置项

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_conf.h"

uint32_t UBTurboGetUInt64(const std::string &section, const std::string &configKey, uint64_t &configValue);
```

## 描述 DESCRIPTION

获取指定的uint64类型配置项

## 参数 Parameters

| name        | IN/OUT | description  |
| ----------- | ------ | ------------ |
| section     | IN     | 指定具体插件。要求格式为“plugin_<plugin_name>"，其中<plugin_name>必须是在ubturbo_plugin_admission.conf中指定的，例如“plugin_rmrs”。不符合要求的输入会导致获取配置项失败，返回错误码1。|
| configKey      | IN     | 配置项名称。必须是在plugin_<plugin_name>.conf中存在的配置项名称。否则返回错误1。配置文件中，1<= configKey长度 <= 256。 |
| configValue | OUT    | 配置项的值。执行成功时，configValue即为获取到的配置项的值。执行失败时，configValue不具有任何意义。配置文件中，1<= configValue长度 <= 256。 |

## 返回值 RETURN VALUE

返回值0：表示成功。

返回非0错误码：表示失败。

## 约束 CONSTRAINTS

- 配置项名称的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项的值的长度区间为[1, 256]，否则报错，进程启动失败
- 配置项名称只能包含字母、数字、.、_、-
- 配置项的值只能包含字母、数字、.、_、-、:、,、/、;
- 执行成功时，configValue即为获取到的配置项的值
- 同一个配置文件中存在重复配置项则报错，且服务启动失败

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成获取指定的uint64_t类型配置项。

```cpp
#include "turbo_conf.h"

int main(void)
{
    const std::string section = "section";
    const std::string configKey = "configKey";
    uint64_t configValue;
    uint32_t ret = UBTurboGetUInt64(section, configKey, configValue);
    return 0;
}
```



# UBTurboFunctionCaller: 客户端通过进程间通信调用服务端函数

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_ipc_client.h"

uint32_t UBTurboFunctionCaller(const std::string &function, const TurboByteBuffer &params, TurboByteBuffer &result);
```

## 描述 DESCRIPTION

客户端通过进程间通信调用服务端函数。

## 参数 Parameters

| name     | IN/OUT | description                                                  |
| -------- | ------ | ------------------------------------------------------------ |
| function | IN     | 函数名。                                                     |
| params   | IN     | 客户端发送给服务端的数据，由调用方进行序列化。               |
| result   | OUT    | 服务端返回给客户端的数据，由调用方进行反序列化。result.data的内存通过new进行分配，应该由调用方通过delete进行释放。 |

## 返回值 RETURN VALUE

返回值0：表示成功。
返回值1：通用错误码。
返回值2：socket失效。
返回值3：连接断开。
返回值4：服务端无对应函数。
返回值5：服务端对应函数执行错误。
返回值6：服务端对应函数出参范围异常。

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成客户端通过进程间通信调用服务端函数。

```cpp
#include "turbo_ipc_client.h"

int main(void)
{
    TurboByteBuffer params;
    TurboByteBuffer result;
    params.data = new uint8_t[5];
    params.len = 5;
    params.data[0] = 'h';
    params.data[1] = 'e';
    params.data[2] = 'l';
    params.data[3] = 'l';
    params.data[4] = 'o';
    uint32_t ret = UBTurboFunctionCaller("function", params, result);
    return 0;
}
```



# UBTurboRegIpcService: 服务端注册回调函数

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_ipc_server.h"

uint32_t UBTurboRegIpcService(const std::string &name, IpcHandlerFunc function);
```

## 描述 DESCRIPTION

服务端注册回调函数。

## 参数 Parameters

| name     | IN/OUT | description                          |
| -------- | ------ | ------------------------------------ |
| name     | IN     | 服务端回调函数的名称。非空字符串，长度区间[1, 128]且无空格。|
| function | IN     | 服务端用于处理客户端请求的回调函数。 |

## 返回值 RETURN VALUE

返回值0：表示成功。
返回非0错误码：表示失败。

## 约束 CONSTRAINTS

暂无

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成服务端注册回调函数。

```cpp
#include "turbo_ipc_server.h"

uint32_t MemBorrowRollbackRecvHandler(const TurboByteBuffer &req, TurboByteBuffer &resp)
{
	/*
	对应实现
	*/
    return 0;
}

int main(void)
{
	uint32_t ret = UBTurboRegIpcService("MemBorrowRollback", MemBorrowRollbackRecvHandler);
    return 0;
}
```



# UBTurboUnRegIpcService: 服务端解注册回调函数

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_ipc_server.h"

UBTURBO_LOG_CRIT(moduleName, moduleId) << args;
```

## 描述 DESCRIPTION

服务端解注册回调函数。

## 参数 Parameters

| name | IN/OUT | description              |
| ---- | ------ | ------------------------ |
| name | IN     | 已注册的函数名，字符串。 |

## 返回值 RETURN VALUE

返回值0：表示成功。
返回非0错误码：表示失败。

## 约束 CONSTRAINTS

- 已注册的服务解注册成功后，再次解注册，返回失败

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成服务端解注册。

```cpp
#include "turbo_ipc_server.h"


int main(void)
{
	uint32_t ret = UBTurboUnRegIpcService("MemBorrowRollback");
    return 0;
}
```



# UBTURBO_LOG_CRIT: 创建单条CRIT级别日志，写入终端或文件

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_logger.h"

UBTURBO_LOG_CRIT(moduleName, moduleId) << args;
```

## 描述 DESCRIPTION

创建单条CRIT级别日志，写入终端或文件。

## 参数 Parameters

| name       | IN/OUT | description        |
| ---------- | ------ | ------------------ |
| moduleName | IN     | 打印日志的模块名。；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 |
| moduleId   | IN     | 打印日志的模块id。moduleid作为保留字段，当前无意义。 |
| args       | IN     | 需要打印的信息。 可打印的类型，支持通过operator <<打印。  |

## 返回值 RETURN VALUE

返回值true：表示成功。

返回值false：表示失败。

## 约束 CONSTRAINTS

- 打印内容输出格式为：[时间][CRIT][进程号][线程号][代码所在文件及行数]日志信息。
- 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成创建单条CRIT级别日志，写入终端或文件。

```cpp
#include "turbo_logger.h"


int main(void)
{
	UBTURBO_LOG_CRIT(moduleName, moduleId) << “This is CRIT log”;
    return 0;
}
```



# UBTURBO_LOG_ERROR: 创建单条ERROR级别日志，写入终端或文件

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_logger.h"

UBTURBO_LOG_ERROR(moduleName, moduleId) << args;
```

## 描述 DESCRIPTION

创建单条ERROR级别日志，写入终端或文件。

## 参数 Parameters

| name       | IN/OUT | description        |
| ---------- | ------ | ------------------ |
| moduleName | IN     | 打印日志的模块名。；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 |
| moduleId   | IN     | 打印日志的模块id。moduleid作为保留字段，当前无意义。 |
| args       | IN     | 需要打印的信息。 可打印的类型，支持通过operator <<打印。  |

## 返回值 RETURN VALUE

返回值true：表示成功。

返回值false：表示失败。

## 约束 CONSTRAINTS

- 打印内容输出格式为：[时间][CRIT][进程号][线程号][代码所在文件及行数]日志信息。
- 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成创建单条ERROR级别日志，写入终端或文件。

```cpp
#include "turbo_logger.h"


int main(void)
{
	UBTURBO_LOG_ERROR(moduleName, moduleId) << “This is ERROR log”;
    return 0;
}
```



# UBTURBO_LOG_WARN: 创建单条WARN级别日志，写入终端或文件

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_logger.h"

UBTURBO_LOG_WARN(moduleName, moduleId) << args;
```

## 描述 DESCRIPTION

创建单条WARN级别日志，写入终端或文件。

## 参数 Parameters

| name       | IN/OUT | description        |
| ---------- | ------ | ------------------ |
| moduleName | IN     | 打印日志的模块名。；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 |
| moduleId   | IN     | 打印日志的模块id。moduleid作为保留字段，当前无意义。 |
| args       | IN     | 需要打印的信息。 可打印的类型，支持通过operator <<打印。  |

## 返回值 RETURN VALUE

返回值true：表示成功。

返回值false：表示失败。

## 约束 CONSTRAINTS

- 打印内容输出格式为：[时间][CRIT][进程号][线程号][代码所在文件及行数]日志信息。
- 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成创建单条WARN级别日志，写入终端或文件。

```cpp
#include "turbo_logger.h"


int main(void)
{
	UBTURBO_LOG_WARN(moduleName, moduleId) << “This is WARN log”;
    return 0;
}
```



# UBTURBO_LOG_INFO: 创建单条INFO级别日志，写入终端或文件

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_logger.h"

UBTURBO_LOG_INFO(moduleName, moduleId) << args;
```

## 描述 DESCRIPTION

创建单条INFO级别日志，写入终端或文件。

## 参数 Parameters

| name       | IN/OUT | description        |
| ---------- | ------ | ------------------ |
| moduleName | IN     | 打印日志的模块名。；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 |
| moduleId   | IN     | 打印日志的模块id。moduleid作为保留字段，当前无意义。 |
| args       | IN     | 需要打印的信息。 可打印的类型，支持通过operator <<打印。  |

## 返回值 RETURN VALUE

返回值true：表示成功。

返回值false：表示失败。

## 约束 CONSTRAINTS

- 打印内容输出格式为：[时间][CRIT][进程号][线程号][代码所在文件及行数]日志信息。
- 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成创建单条INFO级别日志，写入终端或文件。

```cpp
#include "turbo_logger.h"


int main(void)
{
	UBTURBO_LOG_INFO(moduleName, moduleId) << “This is INFO log”;
    return 0;
}
```



# UBTURBO_LOG_DEBUG: 创建单条DEBUG级别日志，写入终端或文件

## 框架 FRAMEWORK

UBTurbo框架

## 摘要 SYNOPSIS

```cpp
#include "turbo_logger.h"

UBTURBO_LOG_DEBUG(moduleName, moduleId) << args;
```

## 描述 DESCRIPTION

创建单条DEBUG级别日志，写入终端或文件。

## 参数 Parameters

| name       | IN/OUT | description        |
| ---------- | ------ | ------------------ |
| moduleName | IN     | 打印日志的模块名。；非空，以'\0'结尾。传入nullptr时，使用默认值'\0'，此时日志打印在.log文件中。 |
| moduleId   | IN     | 打印日志的模块id。moduleid作为保留字段，当前无意义。 |
| args       | IN     | 需要打印的信息。 可打印的类型，支持通过operator <<打印。  |

## 返回值 RETURN VALUE

返回值true：表示成功。
返回值false：表示失败。

## 约束 CONSTRAINTS

- 打印内容输出格式为：[时间][CRIT][进程号][线程号][代码所在文件及行数]日志信息。
- 插件/模块可以通过modulename指定日志的输出文件，modulename作为日志文件的文件名。

## 附注 NOTES

暂无

## 样例 EXAMPLES

以下程序完成创建单条DEBUG级别日志，写入终端或文件。

```cpp
#include "turbo_logger.h"


int main(void)
{
	UBTURBO_LOG_DEBUG(moduleName, moduleId) << “This is DEBUG log”;
    return 0;
}
```



