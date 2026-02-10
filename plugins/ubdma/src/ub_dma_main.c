/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ub-dma memory notifier
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/dma-map-ops.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/hugetlb.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/migrate.h>
#include <linux/dma-mapping.h>

#include "ub_dma_log.h"
#include "ub_dma_iomem.h"
#include "urma.h"
#include "segment_manager.h"
#include "memory_notifier.h"
#include "ub_virt_dma.h"

#define DMA_SLAVE_NAME "slave"
#define UB_DMA_KBUILD_MODNAME "ub_dma_engine"

#define UB_DMA_DEV "ub_dma_dev"
#define UB_DMA_CLASS "ub_dma_class"
#define UB_DMA_DEVICE "ub_dma_device"
#define BASE_MINOR 0
#define NR_MINOR 1


#define UB_DMA_VCHAN_NUM 1

#define RECIVE_POLL_TIMES 100

uint32_t g_ub_dma_log_level = UB_DMA_LOG_LEVEL_INFO;

struct ub_dma_desc {
    struct ub_virt_dma_desc vd;
    uint64_t src_addr;
    uint64_t dest_addr;
    size_t len;
};

struct ub_dma_vchan {
    struct ub_virt_dma_chan vc;
    enum dma_status status;
    struct list_head node;
};

struct ub_dma_dev {
    struct dma_device slave;
    struct ub_dma_vchan *vchans;
    uint32_t vchan_num;
    struct device_driver driver;
};

static struct ub_dma_dev *g_udev;
static dev_t dev = 0;
static struct class *dev_class;
static struct cdev ub_dma_cdev;
static struct device *ub_dma_device;

static u64 ub_dma_all_dmamask = DMA_BIT_MASK(32);

static inline struct ub_dma_desc *to_ub_dma_desc(struct dma_async_tx_descriptor *tx)
{
    return container_of(tx, struct ub_dma_desc, vd.tx);
}

static inline struct ub_dma_dev *to_ub_dma_dev(struct dma_device *d)
{
    return container_of(d, struct ub_dma_dev, slave);
}

static inline struct ub_dma_vchan *to_ub_dma_vchan(struct dma_chan *chan)
{
    return container_of(chan, struct ub_dma_vchan, vc.chan);
}

static void ub_dma_free_desc(struct ub_virt_dma_desc *vd)
{
    struct ub_dma_desc *txd = to_ub_dma_desc(&vd->tx);
    ub_dma_log_debug("ub dma free desc %p\n", txd);

    kfree(txd);
}

static struct dma_async_tx_descriptor *ub_dma_prep_dma_memcpy(
    struct dma_chan *chan, dma_addr_t dest, dma_addr_t src, size_t len, unsigned long flags)
{
    struct ub_dma_vchan *vchan = to_ub_dma_vchan(chan);
    struct ub_dma_desc *txd;

    if (!len) {
        ub_dma_log_err("ub dma prep dma memcpy len is 0\n");
        return NULL;
    }

    txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
    if (!txd) {
        ub_dma_log_err("ub dma prep dma memcpy alloc txd failed.\n");
        return NULL;
    }
    txd->src_addr = (uint64_t)src;
    txd->dest_addr = (uint64_t)dest;
    txd->len = len;

    return vchan_tx_prep(&vchan->vc, &txd->vd, flags);
}

static inline void buid_urma_segment_info(struct ub_dma_desc *txd, struct urma_trans_segment_info *src_info,
    struct urma_trans_segment_info *dst_info)
{
    src_info->addr = txd->src_addr;
    src_info->len = txd->len;
    src_info->sge = NULL;
    dst_info->addr = txd->dest_addr;
    dst_info->len = txd->len;
    dst_info->sge = NULL;
}

static void ub_dma_issue_pending(struct dma_chan *chan)
{
    struct ub_dma_vchan *vchan = to_ub_dma_vchan(chan);
    unsigned long flags;
    int ret;
    struct ub_virt_dma_desc *desc;
    struct ub_dma_desc *txd;
    struct urma_trans_segment_info src_info;
    struct urma_trans_segment_info dst_info;

    spin_lock_irqsave(&vchan->vc.lock, flags);
    if (vchan_issue_pending(&vchan->vc)) {
        if (list_empty(&vchan->node)) {
            desc = vchan_next_desc(&vchan->vc);
            txd = to_ub_dma_desc(&desc->tx);
            buid_urma_segment_info(txd, &src_info, &dst_info);
            ret = get_urma_trans_segment(&src_info, &dst_info);
            if (ret) {
                list_del(&desc->node);
                desc->tx_result.result = DMA_TRANS_WRITE_FAILED;
                goto complete_dma_pending;
            }
            ret = urma_run_send(txd->src_addr, txd->dest_addr, src_info.sge, dst_info.sge, txd->len);
            if (ret) {
                list_del(&desc->node);
                ub_dma_log_err("urma send failed\n");
                desc->tx_result.result = DMA_TRANS_WRITE_FAILED;
                goto complete_dma_pending;
            }
        }
    } else {
        desc->tx_result.result = DMA_TRANS_NOERROR;
        ub_dma_log_info("vchan %p: no pending txd.\n", &vchan->vc);
        goto complete_dma_pending;
    }
    spin_unlock_irqrestore(&vchan->vc.lock, flags);
    return;

complete_dma_pending:
    vchan_cookie_complete(desc);
    spin_unlock_irqrestore(&vchan->vc.lock, flags);
}

static enum dma_status ub_dma_tx_status(struct dma_chan *chan, dma_cookie_t cookie, struct dma_tx_state *state)
{
    enum dma_status ret;
    struct ub_dma_vchan *vchan = to_ub_dma_vchan(chan);

    ret = dma_cookie_status(chan, cookie, state);
    if (ret == DMA_COMPLETE) {
        return vchan->status;
    }
    return ret;
}

static dma_addr_t ub_dma_map_page(struct device *dev, struct page *page,
    unsigned long offset, size_t size, enum dma_data_direction dir, unsigned long attrs)
{
    unsigned long page_len = page_size(page);
    void *addr = page_address(page) + offset;

    if (offset >= page_len) {
        ub_dma_log_err("offset %lx >= page size %lx\n", offset, page_len);
        return DMA_MAPPING_ERROR;
    }

    return (dma_addr_t)addr;
}

static void ub_dma_unmap_page(struct device *dev, dma_addr_t dma_addr,
    size_t size, enum dma_data_direction dir, unsigned long attrs)
{
    return;
}

static const struct dma_map_ops ub_dma_ops = {
    .map_page       = ub_dma_map_page,
    .unmap_page     = ub_dma_unmap_page,
};

static int ub_dma_config(struct dma_chan *chan, struct dma_slave_config *config)
{
    const char *name = "tx";

    chan->name = kasprintf(GFP_KERNEL, "dma:%s", name);
    if (!chan->name) {
        ub_dma_log_err("failed to alloc dma chan name.\n");
        return -ENOMEM;
    }

    return 0;
}

static inline void ub_dma_free(struct ub_dma_dev *udev)
{
    int i;

    for (i = 0; i < udev->vchan_num; i++) {
        struct ub_dma_vchan *vchan = &udev->vchans[i];
        list_del(&vchan->vc.chan.device_node);
        tasklet_kill(&vchan->vc.task);
    }
}

static void ub_dma_free_chan_resources(struct dma_chan *chan)
{
    struct ub_dma_vchan *vchan = to_ub_dma_vchan(chan);

    vchan_free_chan_resources(&vchan->vc);
}

static int ub_dma_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int ub_dma_release(struct inode *inode, struct file *file)
{
    return 0;
}

static struct file_operations ub_dma_fops = {
    .owner = THIS_MODULE,
    .open = ub_dma_open,
    .release = ub_dma_release,
};

static int ub_dma_dev_init(void)
{
    int rc = alloc_chrdev_region(&dev, BASE_MINOR, NR_MINOR, UB_DMA_DEV);
    if (rc < 0) {
        ub_dma_log_err("Cannot allocate major number\n");
        return rc;
    }

    cdev_init(&ub_dma_cdev, &ub_dma_fops);

    rc = cdev_add(&ub_dma_cdev, dev, 1);
    if (rc) {
        ub_dma_log_err("Cannot add the device to the system.\n");
        goto err_cdev;
    }

    dev_class = class_create(UB_DMA_CLASS);
    if (IS_ERR(dev_class)) {
        ub_dma_log_err("Cannot create the struct class.\n");
        rc = PTR_ERR(dev_class);
        goto err_class;
    }

    ub_dma_device = device_create(dev_class, NULL, dev, NULL, UB_DMA_DEVICE);
    if (IS_ERR(ub_dma_device)) {
        ub_dma_log_err("Cannot create the Device.\n");
        rc = PTR_ERR(ub_dma_device);
        goto err_device;
    }
    return 0;

err_device:
    class_destroy(dev_class);
err_class:
    cdev_del(&ub_dma_cdev);
err_cdev:
    unregister_chrdev_region(dev, NR_MINOR);
    return rc;
}

void ub_dma_dev_exit(void)
{
    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    cdev_del(&ub_dma_cdev);
    unregister_chrdev_region(dev, NR_MINOR);
}

void urma_callback_free_desc(struct ubcore_jfc *jfc, struct dmaengine_result res)
{
    int ret;
    struct ub_virt_dma_desc *desc;
    struct ub_dma_vchan *vchan;
    int i;

    ret = ubcore_rearm_jfc(jfc, false);
    if (ret) {
        ub_dma_log_err("ub core rearm jfc fail.\n");
    }
    for (i = 0; i < g_udev->vchan_num; i++) {
        vchan = &g_udev->vchans[i];
        if (list_empty(&vchan->vc.desc_issued)) {
            continue;
        }
        spin_lock(&vchan->vc.lock);
        desc = vchan_next_desc(&vchan->vc);
        list_del(&desc->node);
        desc->tx_result = res;
        vchan->status = (res.result == DMA_TRANS_NOERROR) ? DMA_COMPLETE : DMA_ERROR;
        vchan_cookie_complete(desc);
        spin_unlock(&vchan->vc.lock);
        ub_dma_log_debug("ub_dma_client_comp_callback, vchan %p, status %d\n", vchan, vchan->status);
        break;
    }
}

static inline int urma_callback_check_cr(struct urma_mem_trans *urma_handler, int cqe_num,
    uint32_t wr_cnt, int ci, struct dmaengine_result *res)
{
    int i;
    struct ubcore_cr *cr = urma_handler->cr;

    for (i = 0; i < cqe_num; i++) {
        if (cr[ci + i].status != UBCORE_CR_SUCCESS) {
            ub_dma_log_err("wr failed. status %d\n", cr[ci + i].status);
            res->result = DMA_TRANS_WRITE_FAILED;
            ubcore_flush_jfs(urma_handler->client_jfs, wr_cnt, urma_handler->cr);
            return -EIO;
        }
    }

    return 0;
}

static void ub_dma_client_comp_callback(struct ubcore_jfc *jfc)
{
    uint32_t wr_cnt = 1;
    uint32_t try_times = RECIVE_POLL_TIMES;
    uint32_t wr_left = wr_cnt;
    struct urma_mem_trans *urma_handler = get_urma_jetty();
    struct ubcore_cr *cr = urma_handler->cr;
    int ret;
    int ci = 0;
    struct dmaengine_result res;

    memset(cr, 0, sizeof(struct ubcore_cr));
    while (try_times && wr_left) {
        ret = ubcore_poll_jfc(urma_handler->client_jfc, wr_cnt, cr + ci);
        if (ret < 0) {
            ub_dma_log_err("failed to poll jfc, ret %d\n", ret);
            res.result = DMA_TRANS_WRITE_FAILED;
            goto free_desc;
        } else if (ret > 0) {
            wr_left -= ret;
            if (urma_callback_check_cr(urma_handler, ret, wr_cnt, ci, &res)) {
                goto free_desc;
            }
            ci += ret;
        } else {
            try_times--;
        }
    }

    if (wr_left) {
        ub_dma_log_err("poll timeout, expect %u, left %u\n", wr_cnt, wr_left);
        res.result = DMA_TRANS_ABORTED;
        goto free_desc;
    }
    res.result = DMA_TRANS_NOERROR;

free_desc:
    urma_callback_free_desc(jfc, res);
}

int ub_dma_init(void)
{
    struct ub_dma_dev *udc;
    int ret;
    int i;

    ret = ub_dma_dev_init();
    if (ret) {
        ub_dma_log_err("ub dma dev init fail.\n");
        return -EBUSY;
    }
    udc = devm_kzalloc(ub_dma_device, sizeof(*udc), GFP_KERNEL);
    if (!udc) {
        ub_dma_log_err("failed to alloc udc mem.\n");
        ret = -ENOMEM;
        goto err_exit_ub_dma_dev;
    }
    udc->driver.owner = THIS_MODULE;
    udc->driver.name = UB_DMA_KBUILD_MODNAME;
    udc->vchan_num = UB_DMA_VCHAN_NUM;
    ub_dma_device->driver = &udc->driver;
    ub_dma_device->dma_mask = &ub_dma_all_dmamask;

    dma_cap_set(DMA_PRIVATE, udc->slave.cap_mask);
    dma_cap_set(DMA_MEMCPY, udc->slave.cap_mask);
    dma_cap_set(DMA_SLAVE, udc->slave.cap_mask);

    INIT_LIST_HEAD(&udc->slave.channels);
    udc->slave.device_free_chan_resources = ub_dma_free_chan_resources;
    udc->slave.device_issue_pending = ub_dma_issue_pending;
    udc->slave.device_prep_dma_memcpy = ub_dma_prep_dma_memcpy;
    udc->slave.device_tx_status = ub_dma_tx_status;
    udc->slave.device_config = ub_dma_config;
    udc->slave.dev = ub_dma_device;
    udc->slave.dev->dma_ops = &ub_dma_ops;

    udc->vchans = devm_kcalloc(ub_dma_device, udc->vchan_num, sizeof(struct ub_dma_vchan), GFP_KERNEL);
    if (!udc->vchans) {
        ub_dma_log_err("failed to alloc udc vchans.\n");
        ret = -ENOMEM;
        goto err_ub_dma_unregister;
    }
    for (i = 0; i < udc->vchan_num; i++) {
        struct ub_dma_vchan *vchan = &udc->vchans[i];
        INIT_LIST_HEAD(&vchan->node);
        vchan->vc.desc_free = ub_dma_free_desc;
        vchan_init(&vchan->vc, &udc->slave);
    }

    ret = dma_async_device_register(&udc->slave);
    if (ret) {
        ub_dma_log_err("failed to register DMA engine device\n");
        goto err_kill_tasklet;
    }

    ret = init_urma_mem_trans(ub_dma_client_comp_callback);
    if (ret) {
        ub_dma_log_err("failed to init urma memory transaction\n");
        goto err_ub_dma_unregister;
    }
    ret = memory_notifier_init();
    if (ret) {
        ub_dma_log_err("memory notifier init fail\n");
        goto err_release_urma_mem_trans;
    }

    ret = urma_meta_sge_init();
    if (ret) {
        ub_dma_log_err("urma meta init failed.\n");
        goto err_memory_notifier_exit;
    }

    ret = iomem_urma_sge_init();
    if (ret) {
        ub_dma_log_err("refresh remote ram fail\n");
        goto err_unregister_urma_meta_sge;
    }

    g_udev = udc;

    return 0;

err_unregister_urma_meta_sge:
    ub_dma_unregister_all_segment();
err_memory_notifier_exit:
    memory_notifier_exit();
err_release_urma_mem_trans:
    release_urma_mem_trans();
err_ub_dma_unregister:
    dma_async_device_unregister(&udc->slave);
err_kill_tasklet:
    ub_dma_free(udc);
err_exit_ub_dma_dev:
    ub_dma_dev_exit();
    return ret;
}

static void __exit ub_dma_exit(void)
{
    destory_iomem_workqueue();
    memory_notifier_exit();
    dma_async_device_unregister(&g_udev->slave);
    ub_dma_unregister_all_segment();
    release_urma_mem_trans();
    ub_dma_dev_exit();
    g_udev = NULL;
}

MODULE_LICENSE("GPL v2");
module_init(ub_dma_init);
module_exit(ub_dma_exit);