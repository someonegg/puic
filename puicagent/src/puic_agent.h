// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __PUIC_AGENT_H__
#define __PUIC_AGENT_H__

#include <string>
#include <vector>

#include <uv.h>
#include <puic_client_lib.h>
#include "agent_server.h"

const size_t PUICCONN_WRITEBUF_LOWER = 4 * 1024;
const size_t PUICCONN_WRITEBUF_UPPER = 8 * 1024;

class ProxyJob;

class ProxyManager : public AgentServer::Callback
{
    uv_loop_t* m_loop;

    std::string m_outIp;
    int m_outPort;
    std::string m_fwdIp;
    int m_fwdPort;

    AgentServer m_listener;
    puicclient_dialer m_dialer;

    std::vector<ProxyJob*> m_cleanJobs;

public:
    ProxyManager(
        uv_loop_t* loop,
        const char* lisIp, int lisPort, const sockaddr_storage &lisSS,
        const char* outIp, int outPort, const sockaddr_storage &outSS,
        const char* fwdIp, int fwdPort, const sockaddr_storage &fwdSS
        );
    ~ProxyManager();

    bool Start();

    void Stop();

    void Monitor();

    void JobToClean(ProxyJob* job);

    inline puicclient_dialer dialer() { return m_dialer; }

private:
    void OnAgentConnAccept(
        AgentConn* conn,
        const char* srcIp, int srcPort
        ) override;

    static void OnPUICConnected(
        void* userData,
        puicclient_conn conn
        );

    static void OnPUICanRead(
        void* userData,
        puicclient_conn conn
        );

    static void OnPUICanWrite(
        void* userData,
        puicclient_conn conn,
        size_t remain
        );

    static void OnPUICDisconnected(
        void* userData,
        puicclient_conn conn,
        int connErr, bool fromRemote, const char* details
        );
};

extern char PROXY_LOGGER[64];

#endif // __PUIC_AGENT_H__
