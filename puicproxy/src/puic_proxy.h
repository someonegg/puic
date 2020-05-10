// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_PROXY_H__
#define __PUIC_PROXY_H__

#include <string>
#include <vector>

#include <uv.h>
#include <puic_server_lib.h>

const size_t PUICCONN_WRITEBUF_LOWER = 4 * 1024;
const size_t PUICCONN_WRITEBUF_UPPER = 8 * 1024;

class ProxyJob;

class ProxyManager
{
    uv_loop_t* m_loop;
    bool m_useProxyProtocol;

    std::string m_listenIp;
    int m_listenPort;
    sockaddr_storage m_forwardSS;

    puicserver_listener m_listener;

    std::vector<ProxyJob*> m_cleanJobs;

public:
    ProxyManager(
        uv_loop_t* loop, bool useProxyProtocol,
        const char* lisIp, int lisPort, const sockaddr_storage &lisSS,
        const char* fwdIp, int fwdPort, const sockaddr_storage &fwdSS
        );
    ~ProxyManager();

    bool Start();

    void Stop();

    void Monitor();

    void JobToClean(ProxyJob* job);

    inline int UseProxyProtocol() const { return m_useProxyProtocol; }
    inline const char *GetListenIp() const { return m_listenIp.c_str(); }
    inline int GetListenPort() const { return m_listenPort; }

private:
    static void OnPUICPreAccept(
        void* userData,
        const char* clientIp, int clientPort
        );

    static void OnPUICAccept(
        void* userData,
        puicserver_conn conn,
        const char* clientIp, int clientPort
        );

    static void OnPUICanRead(
        void* userData,
        puicserver_conn conn
        );

    static void OnPUICanWrite(
        void* userData,
        puicserver_conn conn,
        size_t remain
        );

    static void OnPUICDisconnected(
        void* userData,
        puicserver_conn conn,
        int connErr, bool fromRemote, const char* details
        );
};

extern char PROXY_LOGGER[64];

#endif // __PUIC_PROXY_H__
