## SMAP简介

SMAP是一款Huawei计算产品线自研, 开源的内存迁移工具, 搭配OBMM使用, 可以将进程的热页留在本地, 冷页迁往远端, 达到扩容的目的。

例如, 在虚拟化场景中, 随着虚机内使用内存增加, NUMA内的内存使用率随之增加, 当达到一定水线时, 虚机的部分内存需要逃生到借用内存。
此时SMAP提供能力, 将虚机冷数据按照可控比例迁出至借用内存。
在之后的时间内, SMAP周期性统计内存冷热信息, 保证虚机内应用在使用远端内存时, 性能可控。
当NUMA内的内存使用率下降到一定水线时, SMAP支持按照借用的内存迁回虚机数据。

## SMAP安装&部署

#### 前提条件

* 当前部署环境需开启ACPI，并提前安装好obmm、URMA、libvirt库和numactl库。执行以下命令关闭numa\_balancing和透明大页。
  
  <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen580884719598"><p class="p" id="p1395174862716">echo 0 > /proc/sys/kernel/numa_balancing</p><p class="p" id="p179511848102716">echo 0 > /proc/sys/vm/compaction_proactiveness</p><p class="p" id="p1095194882719">echo never > /sys/kernel/mm/transparent_hugepage/defrag</p><p class="p" id="p17951104832720">echo never > /sys/kernel/mm/transparent_hugepage/enabled</p></pre>
* 安装libvirt，并配置，然后启动服务。
  
  <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen1743004145"><p class="p" id="p13953174812279">yum install -y qemu*</p><p class="p" id="p295334842710">yum install -y libvirt*</p><p class="p" id="p795354842714">//进行libvirt配置</p><p class="p" id="p139531948192713">systemctl start libvirtd</p></pre>
* 由于在HCCS环境下，SMAP依赖obmm生成远端numa，安装SMAP前需要进行obmm配置，配置方式参见安装obmm。可通过**numastat -cvm**命令查看。
* 由于SMAP与HCOM对obmm均有依赖，若先安装smap再安装HCOM会导致HCOM出现异常，因此建议安装SMAP前先安装HCOM，安装方式参见HCOM安装。
* /dev/shm/smap\_config保存了NUMA和进程配置等信息，使用SMAP动态库的进程如果需要切换用户，则需要先删除该文件。

#### 安装步骤

1. 安装SMAP。
   
   <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen5971151198"><p class="p" id="p39578483278">yum install -y smap</p></pre>
2. 载入扫描驱动。
   
   <pre class="screen" id="screen8274308918"><p class="p" id="p1958548202716">cd /lib/modules/smap</p><p class="p" id="p295820488271">insmod smap_tracking_core.ko</p></pre>
   
   * 安装smap_access_tracking.ko（HCCS场景）。
     
     <pre class="screen" id="screen1675912812910"><p class="p" id="p195894802714">insmod smap_access_tracking.ko</p></pre>
   * 安装smap_access_tracking.ko（UB场景下smap\_scene的值设置为2，HCCS场景不需要设置）。
     
     <pre class="screen" id="screen1810215131197"><p class="p" id="p995994814275">insmod smap_access_tracking.ko smap_scene=2</p></pre>
   * 安装hist\_tracking.ko（仅UB下可安装）。在UB仿真中安装时，需使能SMAP硬件判热功能，且此ko的插入顺序在smap_access_tracking.ko之后。在B300及之前的仿真版本中，需指定smap\_scene=2，UB硬件场景不传入此参数。
     
     <pre class="screen"><p class="p" id="p8959848192712">insmod smap_histogram_tracking.ko smap_scene=2</p></pre>
3. 检查扫描驱动是否载入成功。
   
   <pre class="screen" id="screen1884674894220"><p class="p" id="p9960648142719">lsmod | grep tracking</p></pre>
   
   * 在HCCS环境能查询到插入的2个ko，则表示载入成功，示例如下：
     
     <pre class="screen" id="screen81169214579"><p class="p" id="p179602488279">[root@controller ~]# lsmod | grep tracking</p><p class="p" id="p15960174814274">access_tracking        65536  1</p><p class="p" id="p1696084810278">tracking_core          28672  21 access_tracking</p></pre>
   * 在UB仿真环境能查询到插入的2个ko，则表示载入成功。
   * 使用SMAP硬件判热功能时，查询到3个ko，则表示载入成功。
4. 虚拟化场景需安装qemu-system-aarch64。
   
   <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen113013468319"><p class="p" id="p17961114816271">yum install qemu-system-aarch64 -y</p></pre>
5. 进入以下目录，载入迁移驱动。
   
   <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen14933143815411"><p class="p" id="p8962164813273">cd /lib/modules/smap</p></pre>
   
   * 池化场景（此处以本地+借用共6个NUMA时为例，node\_modes的参数为逗号分隔的6个5，最多支持16个NUMA。）
     <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen10449144916557"><p class="p" id="p4962648122720">insmod smap_tiering.ko node_modes=5,5,5,5,5,5 smap_mode=2 smap_pgsize=0</p></pre>
   * 池化场景（UB场景下smap\_scene的值设置为2，HCCS场景不设置）。
     <pre class="screen"><p class="p" id="p496284812717">insmod smap_tiering.ko node_modes=5,5,5,5,5,5 smap_mode=2 smap_pgsize=0 smap_scene=2</p></pre>
   * 虚拟化场景（HCCS场景）。
     <pre class="screen"><p class="p" id="p14963144818276">insmod smap_tiering.ko node_modes=5,5,5,5,5,5</p></pre>
   * 虚拟化场景（UB场景下smap\_scene的值设置为2，HCCS场景不设置）。
     <pre class="screen" id="screen10850151716211"><p class="p" id="p1296384811277">insmod smap_tiering.ko node_modes=5,5,5,5,5,5 smap_scene=2</p></pre>
6. 检查迁移驱动是否载入成功，返回smap信息即表示安装成功。
   
   <pre class="screen"><p class="p" id="p20963104815273">lsmod | grep smap</p></pre>
