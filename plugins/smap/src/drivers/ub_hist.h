/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: SMAP : ub_hist
 */

#ifndef UB_HIST_H
#define UB_HIST_H

#include <linux/device.h>
#include <linux/io.h>

#define UB_HIST_MAGIC 'u'
#define UB_HIST_IOCTL_QUERY_BA_COUNT _IOR(UB_HIST_MAGIC, 1, int)
#define UB_HIST_IOCTL_QUERY_BA_TAGS _IOR(UB_HIST_MAGIC, 2, int)
#define UB_HIST_IOCTL_QUERY_BA_INFO _IOR(UB_HIST_MAGIC, 3, int)
#define UB_HIST_IOCTL_SET_REGS _IOW(UB_HIST_MAGIC, 4, int)
#define UB_HIST_IOCTL_GET_REGS _IOR(UB_HIST_MAGIC, 5, int)
#define UB_HIST_IOCTL_GET_STATISTIC_RESULT _IOR(UB_HIST_MAGIC, 6, int)

#define BA_STS_VALUE_SIZE 2
#define BA_STS_VALUE_COUNT 8192
#define BA_STS_TOTAL_LEHGTH (BA_STS_VALUE_COUNT * BA_STS_VALUE_SIZE)
#define BA_STS_WORD_COUNT (BA_STS_TOTAL_LEHGTH / sizeof(u32))

#define KB (1ULL << 10)
#define MB (1ULL << 20)
#define GB (1ULL << 30)

#define SIZE_4K (4 * KB)
#define SHIFT_4K 12

#define SIZE_2M (2 * MB)
#define SHIFT_2M 21

#define SIZE_32M (32 * MB)
#define SHIFT_32M 25

#define SIZE_16G (16 * GB)
#define SHIFT_16G 34

enum ub_hist_sts_size {
	STS_SIZE_4K = 0,
	STS_SIZE_2M = 1,
};

union ub_hist_ba_reg_ctrl {
	struct {
		uint32_t ram_init_done : 1;
		uint32_t ecc_inject_mode : 1;
		uint32_t ecc_inject_en : 2;
		uint32_t sts_enable : 1;
		uint32_t sts_size : 1;
		uint32_t sts_rd_en : 1;
		uint32_t sts_wr_en : 1;
		uint32_t ecc2bit_err_to_poison : 1;
		uint32_t sts_base_addr : 23;
	};
	uint32_t val;
};

union ub_hist_ba_reg_ecc {
	struct {
		uint32_t smap_1bit_ecc_int_en : 1;
		uint32_t smap_1bit_ecc_int_src : 1;
		uint32_t smap_1bit_ecc_int_sts : 1;
		uint32_t smap_2bit_ecc_int_en : 1;
		uint32_t smap_2bit_ecc_int_src : 1;
		uint32_t smap_2bit_ecc_int_sts : 1;
		uint32_t reserved : 2;
		uint32_t ecc_addr_1bit : 12;
		uint32_t ecc_addr_2bit : 12;
	};
	uint32_t val;
};

union ub_hist_ba_reg_thresh {
	struct {
		uint32_t ecc_1bit_intr_cnt : 16;
		uint32_t ecc_1bit_intr_th : 16;
	};
	uint32_t val;
};

enum {
	BA_CTRL_REG_CFG_MASK = 0x1,
	BA_ECC_REG_CFG_MASK = 0x2,
	BA_THRESH_REG_CFG_MASK = 0x4,
};

struct ub_hist_ba_regs {
	union ub_hist_ba_reg_ctrl reg_ctrl;
	union ub_hist_ba_reg_ecc reg_ecc;
	union ub_hist_ba_reg_thresh reg_thresh;
};

struct ub_hist_ba_config {
	uint32_t mask;
	uint32_t rsvd;
	struct ub_hist_ba_regs regs;
};

struct ub_hist_reg_config {
	uint64_t ba_tag;
	struct ub_hist_ba_config config;
};

struct phy_addr_range {
	phys_addr_t start;
	phys_addr_t end;
};

struct ub_hist_ba_info {
	uint64_t ba_tag;
	struct phy_addr_range nc_range;
	struct phy_addr_range cc_range;
};

struct ub_hist_ba_result {
	uint64_t ba_tag;
	uint16_t buffer[BA_STS_VALUE_COUNT];
};

struct ub_hist_ba_tags {
	int32_t ba_count;
	int32_t rsvd;
	uint64_t ba_tags[0];
};

enum platform_type {
	PLATFORM_QEMU_ONE_SOCKET = 0,
	PLATFORM_EVB_ONE_SOCKET = 1,
	PLATFORM_QEMU_TWO_SOCKETS = 2,
	PLATFORM_EVB_TWO_SOCKETS = 3,
};

/* External Functions */
int ub_hist_init(enum platform_type platform);
void ub_hist_exit(void);
int ub_hist_lock_device(void);
void ub_hist_unlock_device(void);
int ub_hist_query_ba_count(void);
int ub_hist_query_ba_tags(uint64_t *p_tags, int count);
int ub_hist_query_ba_info(uint64_t ba_tag, struct ub_hist_ba_info *ba_info);
int ub_hist_set_state(struct ub_hist_ba_config *config, uint64_t ba_ga);
int ub_hist_get_state(struct ub_hist_ba_config *config, uint64_t ba_ga);
int ub_hist_get_statistic_result(struct ub_hist_ba_result *result);

#endif
