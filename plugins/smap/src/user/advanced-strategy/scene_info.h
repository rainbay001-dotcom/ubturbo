/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef __SCENE_INFO_H__
#define __SCENE_INFO_H__

#include <inttypes.h>

/* enter and exit unstable threshold */
#define EXIT_UNSTABLE_L2HOT_THRESHOLD_NUM 5
#define ENTER_UNSTABLE_THRESHOLD_RATIO 0.01
#define ENTER_UNSTABLE_THRESHOLD_NUM 100
#define EXIT_UNSTABLE_THRESHOLD_RATIO 0.002
#define ENTER_UNSTABLE_WINDOW_SIZE 2
#define EXIT_UNSTABLE_WINDOW_SIZE 6

/* enter heavy-stable threshold */
#define ENTER_HEAVY_WINDOW_SIZE 3

/* hungry window size */
#define L2_CHECK_WINDOW_SIZE 3

/* guarantee keep window size */
#define KEEP_GUARANTEE_WINDOW_SIZE 5

/* min guarantee vm size */
#define GUARANTEE_AFFLUENT_SIZE 1.05

/* min guarantee vm size */
#define MIN_GUARANTEE_SIZE 0.5

/* enter and exit heavy-load threshold */
#define HEAVY_HOT_RATIO 0.60

/* migrate cycle */
#define UNSTABLE_MIGRATE_CYCLE 2000
#define HEAVY_STABLE_MIGRATE_CYCLE 2000
#define LIGHT_STABLE_MIGRATE_CYCLE 2000

/* scan time */
#define UNSTABLE_SCAN_CYCLE 200
#define HEAVY_STABLE_SCAN_CYCLE 200
#define LIGHT_STABLE_SCAN_CYCLE 200

#define PAGE_INFO_DEPTH 8

typedef enum {
    LIGHT_STABLE_SCENE = 0,
    HEAVY_STABLE_SCENE,
    UNSTABLE_SCENE,
    SCENE_MAX,
} Scene;

typedef struct {
    int scanCycle;
    int migCycle;
} SceneCycle;

typedef enum {
    LESS = 0,
    STAY,
    SATISFIED,
    FULL_SATISFIED,
} LocalMemStatus;

typedef struct {
    uint32_t nrPages;
    uint32_t nrL1Page;
    uint32_t nrL2Page;
    uint32_t nrHot;
    uint32_t nrL1Hot;
    uint32_t nrL2Hot;
    uint32_t nrL1Guarantee;
    uint32_t nrL1GuaranteeBk;
    uint32_t nrL1Planed;
} PageInfo;

typedef struct {
    int pageInfoIndex;
    PageInfo pageInfo[PAGE_INFO_DEPTH];
    Scene lastScene;
    Scene currScene;
    SceneCycle cycles;
    LocalMemStatus status;
} SceneInfo;

#endif /* __SCENE_INFO_H__ */
