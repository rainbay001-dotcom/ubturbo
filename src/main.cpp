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
#include <iostream>
#include <csignal>
#include <thread>

#include "turbo_common.h"
#include "turbo_ipc_server.h"
#include "turbo_error.h"
#include "turbo_logger.h"
#include "turbo_main.h"
#include "turbo_def.h"

namespace turbo {

volatile sig_atomic_t g_stopFlag = 0;

void SignalHandler(int signum)
{
    std::cout << "Received signal " << signum << std::endl;
    if (signum == SIGPIPE) {
        std::cout << "SIGPIPE signal received, ignore it." << std::endl;
        return;
    }

    turbo::main::TurboMain::GetInstance().Stop();
    g_stopFlag = 1;
}
}  // namespace turbo
using namespace turbo::config;
using namespace turbo::log;
using namespace turbo::ipc::server;

int main()
{
    const std::array signals = {SIGINT, SIGTERM, SIGHUP, SIGPIPE};

    for (int sig : signals) {
        if (signal(sig, turbo::SignalHandler) == SIG_ERR) {
            std::cerr << "[Main] Failed to register handler for signal " << sig << "." << std::endl;
        }
    }

    RetCode ret = turbo::main::TurboMain::GetInstance().Run();
    if (ret != TURBO_OK) {
        std::cerr << "[Main] Turbo main init failed." << std::endl;
        turbo::main::TurboMain::GetInstance().Stop();
        return -1;
    }

    UBTURBO_LOG_INFO(MODULE_NAME, MODULE_CODE) << "[Main] UBTurbo initialized successfully.";
    while (!turbo::g_stopFlag) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "[Main] UBTurbo exited successfully." << std::endl;
    return 0;
}
