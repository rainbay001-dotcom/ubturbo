## SMAP简介

SMAP是在灵衢超节点架构中, 基于内存池化技术单节点能够使用其他节点内存资源，可以将进程的热页留在本地, 冷页迁往远端, 达到扩容的目的。

例如, 在虚拟化场景中, 随着虚机内使用内存增加, NUMA内的内存使用率随之增加, 当达到一定水线时, 虚机的部分内存需要逃生到借用内存。
此时SMAP提供能力, 将虚机冷数据按照可控比例迁出至借用内存。
在之后的时间内, SMAP周期性统计内存冷热信息, 保证虚机内应用在使用远端内存时, 性能可控。
当NUMA内的内存使用率下降到一定水线时, SMAP支持按照借用的内存迁回虚机数据。

## SMAP安装&部署

#### 前提条件

* 安装前先创建ubturbo用户和用户组，配置免密登录，命令如下。

  <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen580884719598"><p class="p" id="p1395174862716">groupadd -r ubturbo</p><p class="p" id="p179511848102716">useradd -r -g ubturbo -s /sbin/nologin ubturbo</p><p class="p" id="p1095194882719">touch /etc/sudoers.d/ubturbo</p><p class="p" id="p17951104832720">echo "ubturbo ALL=(root) NOPASSWD:/user/local/bin/cat.sh" > /etc/sudoers.d/ubturbo</p></pre>

* 当前部署环境需开启ACPI，并提前安装好obmm、URMA、libvirt库和numactl库。执行以下命令关闭numa\_balancing和透明大页。
  
  <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen580884719598"><p class="p" id="p1395174862716">echo 0 > /proc/sys/kernel/numa_balancing</p><p class="p" id="p179511848102716">echo 0 > /proc/sys/vm/compaction_proactiveness</p><p class="p" id="p1095194882719">echo never > /sys/kernel/mm/transparent_hugepage/defrag</p><p class="p" id="p17951104832720">echo never > /sys/kernel/mm/transparent_hugepage/enabled</p></pre>
* 安装libvirt，并配置，然后启动服务。容器场景略过此步骤。
  
  <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen1743004145"><p class="p" id="p13953174812279">yum install -y qemu*</p><p class="p" id="p295334842710">yum install -y libvirt*</p><p class="p" id="p795354842714">//进行libvirt配置</p><p class="p" id="p139531948192713">systemctl start libvirtd</p></pre>
* 由于SMAP与HCOM对obmm均有依赖，若先安装smap再安装HCOM会导致HCOM出现异常，因此建议安装SMAP前先安装HCOM，安装方式参见HCOM安装。
* /dev/shm/smap\_config保存了NUMA和进程配置等信息，使用SMAP动态库的进程如果需要切换用户，则需要先删除该文件。
* smap的period.config文件依赖ubturbo-rmrs组件，若环境上未安装ubturbo-rmrs，则需要手动创建目录/opt/ubturbo/conf，且确保目录权限和smap二进制文件权限一致。

#### 安装步骤

1. 安装SMAP。
   
   安装smap软件包。
   <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen5971151198"><p class="p" id="p39578483278">rpm -ivh smap-x.x.x-x.oe2403sp1.aarch64.rpm</p></pre>
2. 载入扫描驱动。
   
   <pre class="screen" id="screen8274308918"><p class="p" id="p1958548202716">cd /lib/modules/smap</p><p class="p" id="p295820488271">insmod smap_tracking_core.ko</p></pre>
   
   
   * 安装smap_histogram_tracking.ko。
     
     <pre class="screen" id="screen1810215131197"><p class="p" id="p995994814275">insmod smap_histogram_tracking.ko</p></pre>
   * 安装smap_access_tracking.ko。在UB仿真中安装时，如使能SMAP硬件判热功能，则增加enable_hist=1参数。
     
     <pre class="screen"><p class="p" id="p8959848192712">insmod smap_access_tracking.ko smap_scene=2</p></pre>
3. 检查扫描驱动是否载入成功。
   
   <pre class="screen" id="screen1884674894220"><p class="p" id="p9960648142719">lsmod | grep tracking</p></pre>
   
   * 在环境能查询到插入的3个ko，则表示载入成功，示例如下：
     
     <pre class="screen" id="screen81169214579"><p class="p" id="p179602488279">[root@controller ~]# lsmod | grep tracking</p><p class="p" id="p15960174814274">smap_access_tracking         65536  0</p><p class="p" id="p1696084810278">smap_histogram_tracking      28672  1 access_tracking</p><p class="p" id="p179602488279">smap_tracking_core           28672  1 smap_access_tracking</p></pre>
4. 虚拟化场景需安装qemu-system-aarch64。
   
   <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen113013468319"><p class="p" id="p17961114816271">yum install qemu-system-aarch64 -y</p></pre>
5. 进入以下目录，载入迁移驱动。
   
   <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen14933143815411"><p class="p" id="p8962164813273">cd /lib/modules/smap</p></pre>
   
   * 容器场景
     <pre class="screen" id="ZH-CN_TOPIC_0000002029393654__screen10449144916557"><p class="p" id="p4962648122720">insmod smap_tiering.ko smap_pgsize=0</p></pre>
   * 虚拟化场景
     <pre class="screen" id="screen10850151716211"><p class="p" id="p1296384811277">insmod smap_tiering.ko smap_scene=2</p></pre>
6. 检查迁移驱动是否载入成功，返回smap信息即表示安装成功。
   
   <pre class="screen"><p class="p" id="p20963104815273">lsmod | grep smap</p></pre>
