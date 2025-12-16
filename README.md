[TOC]
# UBTurbo
## 项目简介

UBTurbo是一款开源的节点内资源管理框架, 具备配置读取、插件加载、日志打印和IPC通信能力，集成SMAP能力提供基础的多级内存调度服务。

例如, 在虚拟化场景中, RMRS内存迁移工具基于UBTurbo框架开发，并运行在UBTurbo进程中，通过IPC与SMAP能力对外提供内存迁移决策与执行服务，外部进程使用UBTurbo客户端向RMRS发送指令与消息流。RMRS的配置项存储在配置文件中，可以通过UBTurbo的配置读取功能获得；并且基于UBTurbo框架的日志能力打印日志。

## 目录结构


```
UBTURBO/
├── 3rdparty                    // 源码三方库
├── build                       // 项目脚本
├── conf                        // 配置文件
├── doc                         // 文档 
├── include                     // 全局头文件
├── plugins                     // UBTurbo插件库 
├── src                         
│   ├── include                 // 头文件
│   ├── config                  // 配置模块
│   ├── ipc                     // 通信模块
│   ├── log                     // 日志模块
│   ├── main                    // UBTurbo main
│   ├── plugin                  // 插件模块
│   ├── smap                    // SMAP编/解码
│   ├── utils                   // 工具
└── test
    ├── 3rdparty                // 测试三方库
    └── testcase                // 测试用例
```

## 约束说明
- 用户使用UBTurbo进行内存借用时，需保证迁出内存地址和迁入内存地址的安全性一致。
- 用户管理的需要迁移的虚机或容器对应的用户权限应该和远端内存对应的用户权限保持一致。
- 客户使用UBTurbo，需要将用户添加到UBTurbo属组，被添加用户须拥有节点内存资源管理员的权限，才能使用UBTurbo内存迁移的能力，在内存迁移中PID由集群资源管理中心管理和下发，UBTurbo组件无法校验PID有效性，需要开发者在整体解决方案中，综合考虑pid、srcNid，destNid等参数传输和存储的安全。


## 项目架构

![UBTURBO_ARCHITECTURE.png](./doc/images/UBTURBO_ARCHITECTURE.png "UBTURBO_ARCHITECTURE")

**UBTurbo**组件包含以下服务：
- **UBTurboSDK**：UBTurbo服务提供的SDK端，作为一个独立SDK，对外通过接口给外层模块组件使用来使用UBTurbo能力。
- **Common**：公共组件，提供一些公共能力。
  - **Log**：提供日志功能模块。
  - **Config**：配置模块，解析UBTurbo服务的配置信息。
  - **Daemon**：UBTurbo的进程，提供进程服务。
- **MessageServer**：负责接收UBTurboSDK请求，UBTurboSDK通过UDS发送请求到UBTurboServer使能加速能力。

- **RMRS**：资源腾挪，调度模块，负责虚机、容器内存资源的调度。
- **SMAP**：分级内存使能模块，通过页面扫描和迁移使能分级内存能力。

主要包含以下关键技术和方案：
1. **配置加载**：从/opt/ubturbo/conf目录下读取ubturbo.conf、ubturbo_plugin_admission.conf以及每个插件的配置文件。
2. **插件加载**：从指定目录下查找so，通过dlopen加载插件，卸载时通过dlclose关闭动态库。
3. **进程通信**：通过unix domain socket机制，进行节点内进程间通信，提供面向连接的可靠数据传输功能。使用Reactor模式，server端启动线程监听指定socket文件，接受client端连接后创建一个新线程，调用指定回调函数，将结果发送给client端。
4. **日志管理**：
  - 1）**异步环形缓冲区**：使用异步环形缓冲区实现异步日志记录，避免阻塞主线程；
  - 2）**锁机制**：采用适当的锁机制确保多线程环境下的线程安全性；
  - 3）**时间戳处理**：利用系统时间函数获取时间戳信息；
  - 4）**文件操作**：使用文件操作相关的API实现日志文件的写入和管理。UBTurbo框架和各插件日志独立，各自单个日志最大200MB，绕接进行记录，各自最多存储10个文件。

# 快速入门

## 前置条件

- UBTurbo集成了SMAP能力，如需使用SMAP相关能力，需提前安装SMAP.

- /dev/shm/smap_config保存了NUMA和进程配置等信息，如果UBTurbo进程需要切换用户，则需要先删除该文件.

- /dev/shm/ubturbo_page_type.dat保存了SMAP的初始化类型信息，如果UBTurbo进程需要切换用户或者场景（例如虚拟化场景切换到大数据场景），则需要先删除该文件.

- UBTurbo默认不启用任何插件，请依据业务场景在ubturbo_plugin_admission.conf中打开插件（取消对应插件的注释）.

- 在ubturbo_plugin_admission.conf中打开插件前，需保证对应插件已安装且配置完成，否则UBTurbo及对应插件将启动异常.


## UBTurbo编译

在根目录下执行:

```bash
git submodule update --init --recursive
dos2unix build.sh
sh build.sh
```
编译产物：

- 在dist/release/bin下会有以下二进制文件: `ub_turbo_exec`

- 在dist/release/lib下会有以下库文件: `libubturbo_client.so`

- 在dist/release/conf下会有以配置文件: `ubturbo_plugin_admission.conf`、`ubturbo.conf`

## UBTurbo运行

- 配置ubturbo.conf，控制日志级别

| 序号 | 参数 | 说明 | 取值 | 配置节点 | 应用场景 |
|-----|-----|-----|-----|-----|-----|
| 1 | log.level | 日志等级 | 默认值：INFO，取值范围：DEBUG、INFO、WARN、ERROR、CRIT | 所有节点 | 决定主进程和插件的日志输出等级 |

- 在保持上述编译产物的相对位置的前提下，执行如下命令
```bash
chmod +x ub_turbo_exec
./ub_turbo_exec
```
