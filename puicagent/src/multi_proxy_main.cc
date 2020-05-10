// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <memory>

#include <signal.h>
#include <spdlog/spdlog.h>
#include <uv.h>
#include <ip_util.h>

char LOGGER[64] = "main";

int proxy_main(const char* lis_addr, const char* out_addr, const char* fwd_addr);

static void stop_signal_cb(uv_signal_t* handle, int signum)
{
    uv_stop(handle->loop);
}

struct ChildProcess
{
    uv_process_t process;
    uv_process_options_t options;
    uv_stdio_container_t stdio[3];
    std::string ss[7];
    char* args[8];
};

static void child_exit_cb(uv_process_t* handle, int64_t exit_status, int term_signal)
{
    if ((exit_status == 0 || exit_status == 1) && term_signal == 0)
        return;

    auto logger = spdlog::get(LOGGER);
    logger->error(
        "proxy exit, pid={0:d}, code={1:d}, signal={2:d}",
        int(handle->pid), int(exit_status), term_signal
        );

    ChildProcess* child = (ChildProcess*)handle->data;
    int r = uv_spawn(handle->loop, &child->process, &child->options);
    if (r != 0)
        logger->error("proxy restart failed, pid={0:d}, error={1}", int(handle->pid), uv_strerror(r));
}

static int _multi_proxy_main(const std::vector<std::unique_ptr<ChildProcess>> &childs)
{
    auto logger = spdlog::get(LOGGER);

    uv_loop_t* loop = uv_default_loop();

    for (auto itor = childs.begin(); itor != childs.end(); ++itor)
    {
        ChildProcess* child = itor->get();
        int r = uv_spawn(loop, &child->process, &child->options);
        if (r != 0)
        {
            logger->error("proxy spawn failed, error={0}", uv_strerror(r));
            return r;
        }
    }

    logger->info("all proxies spawn");

    uv_signal_t stopsign1;
    uv_signal_init(loop, &stopsign1);
    uv_signal_start(&stopsign1, stop_signal_cb, SIGINT);
    uv_signal_t stopsign2;
    uv_signal_init(loop, &stopsign2);
    uv_signal_start(&stopsign2, stop_signal_cb, SIGTERM);

    uv_run(loop, UV_RUN_DEFAULT);

    for (auto itor = childs.begin(); itor != childs.end(); ++itor)
    {
        ChildProcess* child = itor->get();
        uv_process_kill(&child->process, SIGTERM);
    }

    uv_signal_stop(&stopsign1);
    uv_signal_stop(&stopsign2);

    uv_run(loop, UV_RUN_DEFAULT);

    logger->info("all proxies quit");
    return 0;
}

int multi_proxy_main(const char* path, const char** lis_addrs, int lis_count, const char* out_addr, const char* fwd_addr)
{
    std::vector<std::string> listens;
    for (int i = 0; i < lis_count; ++i)
    {
        std::string lis_ip; int lis_port; sockaddr_storage lis_ss;
        if (!parse_addr(lis_addrs[i], false, lis_ip, lis_port, lis_ss))
        {
            printf("listen address format error.\n");
            return 1;
        }
        listens.push_back(lis_addrs[i]);
    }

    std::string output;
    {
        std::string out_ip; int out_port; sockaddr_storage out_ss;
        if (!parse_addr(out_addr, true, out_ip, out_port, out_ss))
        {
            printf("output address format error.\n");
            return 1;
        }
        output = out_addr;
    }

    std::string forward;
    {
        std::string fwd_ip; int fwd_port; sockaddr_storage fwd_ss;
        if (!parse_addr(fwd_addr, false, fwd_ip, fwd_port, fwd_ss))
        {
            printf("forward address format error.\n");
            return 1;
        }
        forward = fwd_addr;
    }

    // fast path.
    if (listens.size() <= 1)
        return proxy_main(listens[0].c_str(), output.c_str(), forward.c_str());

    std::vector<std::unique_ptr<ChildProcess>> childs;
    for (auto itor = listens.begin(); itor != listens.end(); ++itor)
    {
        childs.push_back(std::make_unique<ChildProcess>());

        ChildProcess* child = childs.back().get();

        child->stdio[0].flags = UV_IGNORE;
        child->stdio[1].flags = UV_INHERIT_FD;
        child->stdio[1].data.fd = 1;
        child->stdio[2].flags = UV_INHERIT_FD;
        child->stdio[2].data.fd = 2;

        child->options.stdio = child->stdio;
        child->options.stdio_count = 3;

        child->ss[0] = path;
        child->ss[1] = "-l";
        child->ss[2] = *itor;
        child->ss[3] = "-o";
        child->ss[4] = output;
        child->ss[5] = "-f";
        child->ss[6] = forward;

        child->args[0] = const_cast<char*>(child->ss[0].c_str());
        child->args[1] = const_cast<char*>(child->ss[1].c_str());
        child->args[2] = const_cast<char*>(child->ss[2].c_str());
        child->args[3] = const_cast<char*>(child->ss[3].c_str());
        child->args[4] = const_cast<char*>(child->ss[4].c_str());
        child->args[5] = const_cast<char*>(child->ss[5].c_str());
        child->args[6] = const_cast<char*>(child->ss[6].c_str());
        child->args[7] = nullptr;

        child->options.exit_cb = child_exit_cb;
        child->options.file = path;
        child->options.args = child->args;

        child->process.data = child;
    }

    spdlog::set_pattern("%Y/%m/%d %H:%M:%S.%f [%n][%l] %v");
    spdlog::stderr_logger_st(LOGGER);
    int code = _multi_proxy_main(childs);
    spdlog::drop_all();

    return code;
}
