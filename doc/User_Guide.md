# 1 UBTurbo 简介

UBTurbo是一款开源的节点内资源管理框架, 具备配置读取、插件加载、日志打印和IPC通信能力，集成SMAP能力提供基础的多级内存调度服务。

例如, 在虚拟化场景中, RMRS内存迁移工具基于UBTurbo框架开发，并运行在UBTurbo进程中，通过IPC与SMAP能力对外提供内存迁移决策与执行服务，外部进程使用UBTurbo客户端向RMRS发送指令与消息流。RMRS的配置项存储在配置文件中，可以通过UBTurbo的配置读取功能获得；并且基于UBTurbo框架的日志能力打印日志。

# 2 UBTurbo 安装&部署

## 2.1 须知

- 需切换为root用户执行安装与启动。
- 初始安装由安装者保证环境纯净，如环境不纯净则会使用环境中的配置文件，可能会导致业务功能运行异常。
- ubturbo依赖smap，两者的安装部署顺序概述为：1. 创建ubturbo用户   2. 安装smap（包括插ko）  3. 安装ubturbo 4. 启动ubturbo；详细流程见本章节后续内容。

## 2.2 创建ubturbo用户和组

使用如下指令在终端创建ubturbo用户和组：

```bash
SYSTEM_USER="ubturbo"
SYSTEM_GROUP="ubturbo"

# 一行初始化：先定义日志函数，再创建组和用户
(
  log_message() { echo "[$(date +'%Y-%m-%d %H:%M:%S')] $1: $2"; }
  handle_error() { log_message "ERROR" "$1"; exit 1; }

  # 创建系统组（-r 表示系统组）
  if ! getent group "$SYSTEM_GROUP" > /dev/null; then
    groupadd -r "$SYSTEM_GROUP" || handle_error "Failed to create group $SYSTEM_GROUP"
    log_message "INFO" "Group $SYSTEM_GROUP created"
  else
    log_message "INFO" "Group $SYSTEM_GROUP already exists"
  fi

  # 创建系统用户（-r 系统用户，-g 指定组，-s 指定 shell）
  if ! getent passwd "$SYSTEM_USER" > /dev/null; then
    useradd -r -g "$SYSTEM_GROUP" -s /sbin/nologin "$SYSTEM_USER" || handle_error "Failed to create user $SYSTEM_USER"
    log_message "INFO" "User $SYSTEM_USER created"
  else
    log_message "INFO" "User $SYSTEM_USER already exists"
  fi
)
```

## 2.3 检查前置依赖SMAP

**说明：**

- /dev/shm/smap_config保存了NUMA和进程配置等信息，如果UBTurbo进程需要切换用户，则需要先删除该文件.
- /dev/shm/ubturbo_page_type.dat保存了SMAP的初始化类型信息，如果UBTurbo进程需要切换用户或者场景（例如虚拟化场景切换到大数据场景），则需要先删除该文件.
- 该步骤中只安装smap的包，不插入smap驱动.

执行以下命令查询SMAP包是否安装。

```bash
rpm -qa | grep ubturbo-smap
```

若返回如下信息，表示安装成功。

```bash
[root@controller ~]# rpm -qa | grep ubturbo-smap
ubturbo-smap-*.aarch64
```

如未安装SMAP，需先安装SMAP。

## 2.4 安装ubturbo

```bash
rpm -ivh ubturbo-rmrs-1.1.1-1.oe2203sp1.aarch64 --force
```

**目录结构：**

| 目录 | 用途说明 | 权限 | 所属用户组 |
| - | - | - | - |
| /opt/os\_turbo                                     | 程序根目录 | 750 | ubturbo:ubturbo |
| /opt/os\_turbo/bin                                 | 可执行文件目录 | 500 | ubturbo:ubturbo |
| /opt/os\_turbo/conf                                | 配置文件目录 | 700 | ubturbo:ubturbo |
| /opt/os\_turbo/lib                                 | 动态库目录 | 500 | ubturbo:ubturbo |
| /var/log/os\_turbo                                 | 日志目录 | 700 | ubturbo:ubturbo |

## 2.5 检查UBTurbo是否安装成功

```bash
rpm -qa | grep ubturbo-rmrs
```

返回如下信息即表示安装成功：

```bash
[root@controller ~]# rpm -qa | grep ubturbo-rmrs
ubturbo-rmrs-*.aarch64
```
## 2.6 按实际需求修改配置文件

**说明：**

- UBTurbo默认不启用任何插件，请依据业务场景在ubturbo_plugin_admission.conf中打开插件（取消对应插件的注释）。
- 在ubturbo_plugin_admission.conf中打开插件前，需保证插件的动态库和配置文件已分别放置在/opt/ubturbo/lib和/opt/ubturbo/conf下，否则UBTurbo及对应插件将启动异常.

### 2.6.1 ubturbo.conf

| 序号 | 参数 | 说明 | 取值 | 配置节点 | 应用场景 |
| - | - | - | - | - | - |
| 1 | log.level | 日志等级 | 默认值：INFO<br>取值范围：DEBUG、INFO、WARN、ERROR、CRIT | 所有节点 | 决定主进程和插件的日志输出等级。 |

### 2.6.2 os\_turbo\_plugin\_admission.conf

| 序号 | 参数 | 说明 | 取值 | 配置节点 | 应用场景 |
| - | - | - | - | - | - |
| 1 | rmrs | 内存碎片插件 | 默认值：777 | 所有节点 | 通算虚拟化场景 |

**说明：**

* 默认不启用任何插件，请依据业务场景在os\_turbo\_plugin\_admission.conf中打开插件（取消对应插件的注释）。
* 在os\_turbo\_plugin\_admission.conf中打开插件前，需保证对应的插件已执行其自身的安装步骤，否则UBTurbo及对应插件将启动异常。
* 插件准入配置文件os\_turbo\_plugin\_admission.conf的说明如下：
  * 只有在该准入配置中配置的插件才会被加载并初始化，未在该文件中配置的插件将不予加载。
  * 配置中key对应插件的插件名称(需要与插件自身配置文件中的名称相对应)，value为初始化函数中需要使用的moduleCode。
  * moduleCode是唯一值。
* 进程启动成功后，通过执行以下命令来查询哪些插件已加载成功:
  ```bash
  cat /var/log/ubturbo/ubturbo.log | grep "loaded successfully"
  ```

## 2.7 启动ubturbo服务

### 2.7.1 启动服务

```bash
systemctl start ubturbo
```

### 2.7.2 查询服务状态

- 查询服务当前状态，状态为active即表示服务已经启动：
  ```bash
  systemctl status ubturbo
  ```
- 查询服务对应进程，观察进程是否存在：
  
  ```bash
  ps -ef | grep ubturbo
  ```
### 2.7.3 查看启动日志

观察进程是否正常启动。启动成功的标志：TurboMain::Run end.

```bash
journalctl -u ubturbo
```

# 3 常用命令

## 3.1 服务管理

- 启动服务
  
  ```bash
  systemctl start ubturbo
  ```
- 服务状态查询
  
  ```bash
  systemctl status ubturbo
  ```
- 停止服务
  
  ```bash
  systemctl stop ubturbo
  ```
- 重启服务
  
  ```bash
  systemctl restart ubturbo
  ```

## 3.2 软件包管理

- 升级
  
  ```bash
  rpm -Uvh ubturbo-rmrs-*.aarch64.rpm
  ```
- 回退
  
  ```bash
  rpm -Uvh --oldpackage ubturbo-rmrs-*.aarch64.rpm
  ```

**说明：**

* 配置文件在新的rpm包里相对之前的rpm有变化，且在本地没有被修改过。 此时执行**rpm -Uvh *xxxx***时，新rpm包里的配置文件会替换覆盖旧的文件。
* 配置文件在新的rpm包里相对之前的rpm有变化，且在本地被修改过。 此时执行**rpm -Uvh xxxx**时，旧文件保持不变，新rpm包里的配置文件并重命名为xx.rpmnew，例如“/opt/os\_turbo/conf/os\_turbo.conf.rpmnew”。
* 配置文件在新的rpm包里相对之前的rpm没有变化，且在本地没有被修改过。 此时执行**rpm -Uvh xxxx**时，新rpm包里的配置文件会替换旧的文件。（旧文件被删除）
* 配置文件在新的rpm包里相对之前的 rpm 没有变化，且在本地被修改过。 此时执行**rpm -Uvh xxxx**时，新rpm包里的配置文件不会覆盖旧的文件，旧文件保持不变。
* 软件卸载，被修改过的配置文件会以.rpmsave后缀备份。备份的配置文件不删除，如以.rpmnew或者.rpmsave后缀的备份的配置文件。
