# Tutorial
 
## Best Practices

在灵衢超节点架构中，UBTurbo作为节点内自研开源的资源管理框架，通过封装SMAP冷热页面调度能力并提供标准IPC通信接口，为上层内存管理工具提供统一服务接入点。RMRS节点内内存调度工具基于UBTurbo插件化框架开发，通过框架内置的配置读取、插件管理、日志管理和IPC通信能力，实现与外部组件的指令交互与消息传递。当节点需要进行内存资源调度时，外部进程通过UBTurbo客户端发起请求，RMRS进行决策并调用SMAP的内存迁移相关接口，动态调整进程内存布局。
 
## Demo
 
使用UBTurbo的一个典型场景是：虚拟化场景下，某节点借用到了其它节点的内存，借用内存形成了一个新的NUMA，并分配了足够的大页；该节点的本地NUMA上启动了一个2M页虚拟机。首先，用户打开RMRS插件并启动UBTurbo进程，通过SDK访问UBTurbo为虚拟机制定迁移策略。基于迁移策略结果，再次向UBTurbo发送内存迁移执行命令，将虚拟机的部分本地内存到远端NUMA上。下面的demo依次完成了以下动作：
 
1. 设置迁出策略的入参信息；
2. 执行内存迁移策略，获取策略结果；
3. 基于策略结果将虚机本地内存迁移到远端NUMA上；

Demo使用说明：
```bash
# 环境要求：已安装ubturbo-rmrs、ubturbo-devel、ubturbo-smap、libboundscheck

# 编译
g++ -lboundscheck -lubturbo_client demo.cpp -o demo

# 运行
# 参数说明
# pid: 虚拟机对应pid
# borrowRemoteNuma: 借用过来的内存呈现的远端numa
./demo <pid> <borrowRemoteNuma>
```

Demo源码：
```C++
// demo.cpp
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "ubturbo/turbo_rmrs_interface.h"

using namespace std;

void PrintStrategyResult(turbo::rmrs::MigrateStrategyResult migrateStrategyResult)
{
     std::cout << "vmInfoList size is " << migrateStrategyResult.vmInfoList.size() << std::endl;
     if (migrateStrategyResult.vmInfoList.size() != 0) {
          turbo::rmrs::VMMigrateOutParam it = migrateStrategyResult.vmInfoList[0];
          std::cout << "pid: " << it.pid << std::endl;
          std::cout << "memSize: " << it.memSize << std::endl;
          std::cout << "desNumaId: " << it.desNumaId << std::endl;
     }
     std::cout << "Estimated waiting time (unit: ms)：" << migrateStrategyResult.waitingTime << std::endl;
     return;
};

int main(int argc, char* argv[])
{
    // ---- Prepare input parameters for migration strategy ---
    if (argc != 3) {
        std::cout << "Invalid number of input parameters." << std::endl;
    }

    turbo::rmrs::MigrateStrategyParamRMRS migrateStrategyParam = {
    // vmInfoList
    .vmInfoList = {
        {
            .pid = static_cast<pid_t>(std::stol(argv[1])),
            .ratio = 50
        }
    },

    // borrowSize
    .borrowSize = 131072,

    // pidRemoteNumaMap    
    .pidRemoteNumaMap = {
        {
            static_cast<pid_t>(std::stol(argv[1])),
            {
                static_cast<uint16_t>(std::stol(argv[2]))
            }
        }
    },

    // timeOutNumas
    .timeOutNumas = {
            3
        }
    };

    // --- 2. Run migration strategy. ---
    turbo::rmrs::MigrateStrategyResult migrateStrategyResult;

    uint32_t ret = turbo::rmrs::UBTurboRMRSAgentMigrateStrategy(migrateStrategyParam, migrateStrategyResult);
    if (ret != 0) {
        std::cout << "Migrate strategy failed : " << ret << std::endl;
        return 0;
    }

    std::cout << "Strategy content is: " << std::endl;
    PrintStrategyResult(migrateStrategyResult);

    // --- 2. Run migration execution. ---
    ret = turbo::rmrs::UBTurboRMRSAgentMigrateExecute(migrateStrategyResult);
    if (ret != 0) {
        std::cout << "Migrate execute failed: " << ret;
        exit(EXIT_FAILURE);
    }

    return 0;
}

```
 
## Getting Started: Compilation Guide
 
在项目根目录执行:

```bash
git submodule update --init
dos2unix build.sh
sh build.sh
```
主要编译产物：

- 在dist/release/bin下会生成以下二进制文件: `ub_turbo_exec`

- 在dist/release/lib下会生成以下库文件: `libubturbo_client.so`、`librmrs_ubturbo_plugin.so`

- 在dist/release/conf下会生成以下配置文件: `ubturbo_plugin_admission.conf`、`ubturbo.conf`、`plugin_rmrs.conf`

构建rpm包：

```bash
cd dist/release
cpack
```
执行上述命令之后，rpm包（例如ubturbo-rmrs-1.1.1-1.oe2403sp1.aarch64.rpm）位于`dist/release/output`中。

## Getting Started: Installation Guide

```bash
rpm -ivh ubturbo-rmrs-1.1.1-1.oe2403sp1.aarch64.rpm
```
检查ubturbo是否安装成功：
```bash
rpm -qa | grep ubturbo-rmrs
```
返回如下信息即表示安装成功：
```bash
[root@controller ~]# rpm -qa | grep ubturbo-rmrs
ubturbo-rmrs*
```

安装后的目录结构：

| 目录 | 用途说明 | 权限 | 所属用户组 |
| - | - | - | - |
| /opt/ubturbo | 程序根目录 | 750 | ubturbo:ubturbo |
| /opt/ubturbo/bin                                 | 可执行文件目录 | 500 | ubturbo:ubturbo |
| /opt/ubturbo/conf                                | 配置文件目录 | 700 | ubturbo:ubturbo |
| /opt/ubturbo/lib                                 | 动态库目录 | 500 | ubturbo:ubturbo |
| /var/log/ubturbo                                 | 日志目录 | 700 | ubturbo:ubturbo |

按实际需求将ubturbo用户添加到libvirt组：ubturbo进程框架本身不依赖libvirt，当前UBTurbo进程默认无法访问libvirt服务。如果插件依赖libvirt，则需要将ubturbo用户添加到libvirt组，命令如下：
```bash
usermod -aG libvirt ubturbo
```
检查是否添加成功，输出内容包含libvirt则表示添加成功：
```bash
groups ubturbo
# 输出内容示例：ubturbo : ubturbo libvirt
```

配置免密sudo: 创建文件/etc/sudoers.d/ubturbo，依次执行：
```bash
# 1. 打开
visudo -f /etc/sudoers.d/ubturbo
# 2. 新增
ubturbo ALL=(root) NOPASSWD:/usr/local/bin/cat.sh
```

为需要与ubturbo交互的用户添加权限，例如ubse:
```bash
usermod -aG ubturbo ubse
```

启动ubturbo服务：

```bash
systemctl start ubturbo
```

查询服务状态：
```bash
systemctl status ubturbo
```

## Getting Started: Test Guide
 
测试代码在项目根目录的test目录下，如果你想针对某个函数进行测试开发，只需要在对应目录下编写新的UT代码即可，该目录下主要文件如下所示：
 
```plaintext
test/
├── 3rdparty/                # 单元测试三方依赖
├── testcase/                # 测试用例
│   ├── config/              # 配置读取用例
│   ├── ipc/                 # ipc通信用例
│   ├── log/                 # 日志模块用例
│   ├── plugin/              # 插件加载用例
│   ├── rmrs/                # rmrs插件用例
│   ├── smap/                # smap模块用例
│   └── utils/               # 公共工具用例
├── CMakeLists.txt           # CMakeLists
├── run_ut.sh                # 单元测试执行脚本
```
## Getting Started: Contribution Guide
 
我们非常欢迎新贡献者加入到项目中来，也非常高兴能为新加入贡献者提供指导和帮助，如果您有任何的意见、建议、问题，可以创建issue。