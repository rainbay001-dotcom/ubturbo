# 1 UB DMA简介

本驱动基于URMA实现异步内存拷贝，实现在内存拷贝中CPU算力往UB硬件上卸载。

在内存借用场景中，用户可通过内核DMA engine标准接口实现远端内存之间的内存异步拷贝。



# 2 UB DMA安装

## 2.1 UB DMA rpm包安装

查询rpm是否安装（rpm包根据具体的使用系统版本填写）

```
rpm -qa | grep ubturbo-ubdma-1.0.0-2.oe2403sp3.arrch64.rpm
```

若未安装执行一下命令进行安装

```
rpm -ivh --force ubturbo-ubdma-1.0.0-2.oe2403sp3.arrch64.rpm
```

安装完成后会生成/lib/modules/ub_dma/目录



## 2.2 UB DMA驱动安装

在上述安装后生成的目录中会生成ub_dma.ko，执行如下命令进行安装

```
insmod ub_dma.ko
```

可以通过下面命令查询是否插入成功

```
lsmod | grep ub_dma
```

