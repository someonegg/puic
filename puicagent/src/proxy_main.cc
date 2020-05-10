// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <signal.h>
#include <spdlog/spdlog.h>
#include <uv.h>
#include <ip_util.h>

#include "puic_agent.h"
#include "agent_server.h"

const char* version = "v2";

static void stop_signal_cb(uv_signal_t* handle, int signum)
{
    uv_stop(handle->loop);
}

static void monitor_cb(uv_prepare_t* handle)
{
    ProxyManager* manager = (ProxyManager*)handle->data;
    manager->Monitor();
}

static int _proxy_main(
    const char* lis_ip, int lis_port, const sockaddr_storage &lis_ss,
    const char* out_ip, int out_port, const sockaddr_storage &out_ss,
    const char* fwd_ip, int fwd_port, const sockaddr_storage &fwd_ss
    )
{
    auto logger = spdlog::get(PROXY_LOGGER);
    logger->info("proxy start, listen={0}:{1:d}, output={2}:{3:d}, forward={4}:{5:d}",
        lis_ip, lis_port, out_ip, out_port, fwd_ip, fwd_port);

    uv_loop_t* loop = uv_default_loop();

    uv_signal_t stopsign1;
    uv_signal_init(loop, &stopsign1);
    uv_signal_start(&stopsign1, stop_signal_cb, SIGINT);
    uv_signal_t stopsign2;
    uv_signal_init(loop, &stopsign2);
    uv_signal_start(&stopsign2, stop_signal_cb, SIGTERM);

    ProxyManager manager(
        loop,
        lis_ip, lis_port, lis_ss,
        out_ip, out_port, out_ss,
        fwd_ip, fwd_port, fwd_ss
        );

    uv_prepare_t monitor;
    uv_prepare_init(loop, &monitor);
    monitor.data = &manager;
    uv_prepare_start(&monitor, monitor_cb);

    if (!manager.Start())
    {
        logger->error("proxy start failed");
        return 1;
    }

    uv_run(loop, UV_RUN_DEFAULT);

    manager.Stop();
    uv_prepare_stop(&monitor);
    uv_signal_stop(&stopsign1);
    uv_signal_stop(&stopsign2);

    uv_run(loop, UV_RUN_DEFAULT);

    logger->info("proxy quit");
    return 0;
}

int proxy_main(const char* lis_addr, const char* out_addr, const char* fwd_addr)
{
    std::string lis_ip; int lis_port; sockaddr_storage lis_ss;
    if (!parse_addr(lis_addr, false, lis_ip, lis_port, lis_ss))
    {
        printf("listen address format error.\n");
        return 1;
    }

    std::string out_ip; int out_port; sockaddr_storage out_ss;
    if (!parse_addr(out_addr, true, out_ip, out_port, out_ss))
    {
        printf("output address format error.\n");
        return 1;
    }

    std::string fwd_ip; int fwd_port; sockaddr_storage fwd_ss;
    if (!parse_addr(fwd_addr, false, fwd_ip, fwd_port, fwd_ss))
    {
        printf("forward address format error.\n");
        return 1;
    }

    auto pid = uv_os_getpid();
    sprintf(PROXY_LOGGER, "proxy%s.%d", version, int(pid));
    sprintf(Agent_LOGGER, "agent.%d", int(pid));

    spdlog::set_pattern("%Y/%m/%d %H:%M:%S.%f [%n][%l] %v");
    spdlog::stderr_logger_st(PROXY_LOGGER);
    spdlog::stderr_logger_st(Agent_LOGGER);
    int code = _proxy_main(
        lis_ip.c_str(), lis_port, lis_ss,
        out_ip.c_str(), out_port, out_ss,
        fwd_ip.c_str(), fwd_port, fwd_ss
        );
    spdlog::drop_all();

    return code;
}
