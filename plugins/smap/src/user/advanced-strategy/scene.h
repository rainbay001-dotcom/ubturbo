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
#ifndef __SCENE_H__
#define __SCENE_H__

#include "scene_info.h"
#include "manage/manage.h"

int InitSceneInfo(SceneInfo *info);
int GetProcessSceneAttr(Scene scene, SceneInfo *info);
int SetProcessSceneAttr(ProcessAttr *process);
void SetAdaptMem(bool flag);
void ConfigRatios(struct ProcessManager *manager);
void ConfigMultiVmRatio(struct ProcessManager *manager);
bool GetAdaptMem(void);

#endif /* __SCENE_H__ */
