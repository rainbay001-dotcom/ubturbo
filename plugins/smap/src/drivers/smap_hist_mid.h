/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: SMAP : smap_hist_mid
 */

#ifndef SMAP_HIST_MID_H
#define SMAP_HIST_MID_H

enum scan_mode {
	SCAN_WITH_4K = 0,
	SCAN_WITH_2M = 1,
	SCAN_MODE_MAX,
};

enum run_mode {
	RUN_SYNC_BLOCKING = 0,
	RUN_ASYNC_BACKGROUND = 1,
	RUN_ASYNC_THREADING = 2,
	RUN_MODE_MAX,
};

enum hist_ecc_mode {
	ECC_MODE_DISABLE = 0,
	ECC_MODE_1BIT = 1,
	ECC_MODE_1BIT_WITH_REPORT = 2,
	ECC_MODE_2BIT = 3,
	ECC_MODE_2BIT_WITH_REPORT = 4,
	ECC_MODE_MAX,
};

enum hist_err_code {
	ERR_CODE_NONE = 0,
	ERR_CODE_BA_MATCH = 1,
	ERR_CODE_GENERAL = 2,
	ERR_CODE_OPERATION = 3,
};

struct hist_roi_data {
	phys_addr_t start_addr;
	size_t length;
};

struct hist_scan_cfg {
	bool sort_enable;
	enum scan_mode scan_mode;
	enum run_mode run_mode;
	uint32_t scan_interval;
	uint32_t result_ratio;
};

struct addr_count_pair {
	uint16_t count;
	phys_addr_t address;
};

union hist_status {
	uint32_t value;
	struct {
		struct {
			uint16_t running : 1;
			uint16_t ready : 1;
			uint16_t error : 1;
			uint16_t initialized : 1;
			uint16_t rsvd : 12;
		} flags;
		uint16_t err_code : 16;
	};
};

#define SCAN_TIME_INTERVAL_MIN 1
#define SCAN_TIME_INTERVAL_MAX 5000
#define SCAN_RESULT_RATIO_MIN 1
#define SCAN_RESULT_RATIO_MAX 100
#define SCAN_INTERVAL_RANGE_US 1000

/* External Functions */
int smap_hist_mid_init(void);
void smap_hist_mid_exit(void);
size_t smap_hist_middle_get_roi_count(void);
size_t smap_hist_middle_get_roi_total_len(void);
int smap_hist_get_roi_info(struct hist_roi_data *user_rois, size_t count);
int smap_hist_middle_add_roi(uint64_t start_addr, size_t length);
int smap_hist_middle_del_roi(uint64_t start_addr, size_t length);
bool smap_hist_middle_query_addr_in_roi(phys_addr_t addr);
int smap_hist_reinit_roi_list(void);
int smap_hist_middle_reset_roi(void);
union hist_status smap_hist_middle_run_status(void);
int smap_hist_middle_get_hot_pages(struct addr_count_pair *result_pair,
				   uint32_t *result_count);
int smap_hist_middle_scan_enable(void);
int smap_hist_middle_scan_disable(void);
int smap_hist_middle_set_scan_config(struct hist_scan_cfg *cfg_data);
int smap_hist_middle_get_scan_config(struct hist_scan_cfg *cfg_data);
int smap_hist_middle_set_ecc_config(enum hist_ecc_mode mode, uint16_t thresh);
int smap_hist_middle_get_ecc_status(bool *ecc);
void smap_hist_middle_reset(void);
#endif
