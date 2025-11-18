/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * rmrs is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "turbo_module_conf.h"
#include "turbo_file_util.h"
#include "turbo_conf_manager.h"

#include <filesystem>

namespace turbo::config {
namespace fs = std::filesystem;
using namespace turbo::common;
using namespace turbo::utils;

RetCode TurboModuleConf::Init()
{
    fs::path exeRootDir(TurboFileUtil::GetExecutableRootDir());
    std::string confPath;
    std::string libPath;
    try {
        confPath = fs::canonical(fs::path(exeRootDir) / CONFIG_DEFAULT_DIR).string();
        libPath = fs::canonical(fs::path(exeRootDir) / LIB_DEFAULT_DIR).string();
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[Conf] Path resolution failed: " << e.what() << std::endl;
        return TURBO_ERROR;
    }

    return TurboConfManager::GetInstance().Init(confPath, libPath);
}

RetCode TurboModuleConf::Start()
{
    return TURBO_OK;
}

void TurboModuleConf::Stop() {}

void TurboModuleConf::UnInit()
{
    // 最后释放配置
    TurboConfManager::GetInstance().Clear();
}

std::string TurboModuleConf::Name()
{
    return "conf";
}

}  //  namespace turbo::config