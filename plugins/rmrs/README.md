# RMRS
## 项目简介

RMRS是一款开源的内存迁移工具, 是ubturbo框架的插件，其搭配OBMM、底层调用SMAP使用，可以决策及执行将虚拟机的内存迁出到远端以及决策、执行将内存迁回.

例如, 在虚拟化场景中, 创建虚拟机时本地Numa上内存均不足，会借用远端内存，将虚拟机的本地内存置换到远端.
此时RMRS提供能力, 决策将本地虚拟机迁出多少到远端内存上，并调用SMAP进行迁移执行；当新虚拟机创建失败等情况下需要回滚迁出行为时，RMRS也提供了内存迁回回滚能力；当虚拟机销毁等情况下，业务侧可能会调用内存归还，RMRS提供了内存归还迁回决策、迁回执行能力，决策远端Numa上的虚拟机是否都可以迁回到本地，如果是则执行迁回。

迁移行为底层调用的都是SMAP，SMAP会周期性统计内存冷热信息, 保证虚机内应用在使用远端内存时, 性能可控。
## 目录结构

```
├── doc                             # 文档
├── src
│   ├── include                     # 全局头文件
│   └── ubturbo_plugin
│       ├── common                  # 公共函数
│       ├── conf                    # 配置文件
│       ├── export                  # 采集模块
│       ├── include                 # 头文件
│       ├── intranode_strategy      # 迁移模块
│       ├── message                 # 消息模块
│       ├── serialization           # 序列化模块
│       └── ucache                  # ucache模块
```

## 项目架构

**RMRS分层架构**：

**UBS RMRS**部署在UBS engine内，各计算节点Agent端仅负责数据采集和消息转发等，主节点侧作为功能入口，提供碎片内存的借用、迁出，内存归还和回滚接口，负责节点间的碎片内存管理。
**RMRS**部署在各计算节点的UBTurbo内，基于自身的资源采集，提供碎片场景虚机的迁出/迁回策略，利用SMAP迁移能力执行虚机的迁出和迁回。


# 快速入门

## 前置条件

- 安装并启动libvirt服务.

  ```bash
  yum install -y qemu*
  yum install -y libvirt*
  systemctl start libvirtd
  ```

- 安装并启动UBTurbo服务.

- RMRS依赖obmm生成远端numa, 安装RMRS前需要进行obmm配置;

- RMRS依赖SMAP的内存迁移和判热能力，安装SMAP.

## RMRS编译

RMRS插件默认集成在UBTurbo中，安装、部署及使用方式参考[UBTurbo Doc](../../../doc/)。

## RMRS启用
检查`/opt/ubturbo/conf/ubturbo_plugin_admission.conf`文件以下选项打开：
```bash
# code 必须大于200
rmrs=777
```

