// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic_agent.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <spdlog/spdlog.h>

class ProxyJob : public AgentConn::Callback
{
    ProxyManager &m_manager;
    bool m_running;

    AgentConn* m_srcConn;
    std::string m_srcIp;
    int m_srcPort;

    puicclient_conn m_dstConn;
    std::string m_dstIp;
    int m_dstPort;

public:
    ProxyJob(
        ProxyManager &manager,
        AgentConn* srcConn,
        const char* srcIp, int srcPort,
        const char* dstIp, int dstPort
        )
        : m_manager(manager)
        , m_running(false)
        , m_srcConn(srcConn)
        , m_srcIp(srcIp)
        , m_srcPort(srcPort)
        , m_dstConn(nullptr)
        , m_dstIp(dstIp)
        , m_dstPort(dstPort)
    {
    }

    ~ProxyJob() override
    {
        spdlog::get(PROXY_LOGGER)->info(
            "proxy delete, client={0}:{1:d}, server={2}:{3:d}",
            m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort);
    }

    void Start()
    {
        m_running = true;

        int r = PUICCLIENT_CreateConn(m_manager.dialer(), "",
            m_dstIp.c_str(), m_dstPort, &m_dstConn);
        if (r != 0)
        {
            OnPUICConnErr(r, false, "puicclient_createconn");
            return;
        }

        PUICCLIENT_SetConnData(m_dstConn, this);

        PUICCLIENT_ConnConnect(m_dstConn);
    }

    inline bool running() const { return m_running; }

private:
    void clean()
    {
        if (!running())
            return;

        m_running = false;

        if (m_srcConn != nullptr)
        {
            m_srcConn->Close();
            m_srcConn = nullptr;
        }

        if (m_dstConn != nullptr)
        {
            PUICCLIENT_SetConnData(m_dstConn, nullptr);
            PUICCLIENT_ConnDisconnect(m_dstConn);
            m_dstConn = nullptr;
        }

        m_manager.JobToClean(this);
    }

    void OnAgentConnErr(const char* op, int err) override
    {
        spdlog::get(PROXY_LOGGER)->info(
            "tcp error, op={0}, code={1:d}, client={2}:{3:d}, server={4}:{5:d}",
            op, err, m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort);
        clean();
    }

    void OnAgentConnEOF() override
    {
        assert(running());
        spdlog::get(PROXY_LOGGER)->info(
            "tcp EOF, client={0}:{1:d}, server={2}:{3:d}",
            m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort);

        size_t buffered = 0;
        PUICCLIENT_ConnWrite(m_dstConn, nullptr, 0, true, &buffered);
        return;
    }

    void OnAgentConnRcvd(const char* data, size_t len) override
    {
        assert(running());

        size_t buffered = 0;
        PUICCLIENT_ConnWrite(m_dstConn, data, len, false, &buffered);
        if (!running())
            return;

        may_tcp_to_puic(buffered);
        return;
    }

    void OnAgentConnSent(size_t len) override
    {
        assert(running());

        PUICCLIENT_ConnMarkConsumed(m_dstConn, len);
        if (!running())
            return;

        may_puic_to_tcp();
        return;
    }

    friend class ProxyManager;

    void OnPUICConnConnected()
    {
        assert(running());

        m_srcConn->Proxy(this);
        if (!running())
            return;

        m_srcConn->StartRead();
        return;
    }

    void OnPUICConnErr(int connErr, bool fromRemote, const char* details)
    {
        const char* from = fromRemote ? "remote" : "self";
        spdlog::get(PROXY_LOGGER)->info(
            "puic error, code={0:d}, from={1}, client={2}:{3:d}, server={4}:{5:d}, details={6}",
            connErr, from, m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort, details);
        clean();
    }

    void OnPUICConnCanRead()
    {
        assert(running());

        may_puic_to_tcp();
        return;
    }

    void OnPUICConnCanWrite(size_t remain)
    {
        assert(running());

        may_tcp_to_puic(remain);
        return;
    }

    void may_puic_to_tcp()
    {
        if (m_srcConn->writing())
            return;

        // 4 is the capacity of uv_write_t.bufsml.
        const char* regs[4];
        size_t lens[4];
        uv_buf_t bufs[4];
        int count = 4;

        int r = PUICCLIENT_ConnReadableRegions(m_dstConn, regs, lens, &count);
        if (!running())
            return;

        if (r == PUICLIERR_EOF)
        {
            spdlog::get(PROXY_LOGGER)->info(
                "puic EOF, client={0}:{1:d}, server={2}:{3:d}",
                m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort);
            m_srcConn->Shutdown();
            return;
        }

        if (count == 0)
            return;

        size_t len = 0;
        for (int i = 0; i < count; ++i)
        {
            bufs[i].base = (char*)regs[i];
            bufs[i].len = (unsigned)lens[i];
            len += lens[i];
        }

        m_srcConn->Write(bufs, count, len);
        return;
    }

    void may_tcp_to_puic(size_t buffered)
    {
        if (buffered >= PUICCONN_WRITEBUF_UPPER)
        {
            m_srcConn->StopRead();
            return;
        }

        if (buffered <= PUICCONN_WRITEBUF_LOWER)
        {
            m_srcConn->StartRead();
            return;
        }
    }
};

ProxyManager::ProxyManager(
    uv_loop_t* loop,
    const char* lisIp, int lisPort, const sockaddr_storage &lisSS,
    const char* outIp, int outPort, const sockaddr_storage &outSS,
    const char* fwdIp, int fwdPort, const sockaddr_storage &fwdSS
    )
    : m_loop(loop)
    , m_outIp(outIp)
    , m_outPort(outPort)
    , m_fwdIp(fwdIp)
    , m_fwdPort(fwdPort)
    , m_listener(loop, lisSS)
    , m_dialer(nullptr)
{
}

ProxyManager::~ProxyManager()
{
    assert(m_dialer == nullptr);
}

bool ProxyManager::Start()
{
    auto logger = spdlog::get(PROXY_LOGGER);

    puicclient_dialer dialer = nullptr;
    int r = PUICCLIENT_CreateDialer(m_loop, m_outIp.c_str(), m_outPort, &dialer);
    if (r != 0)
    {
        logger->error("create dialer failed, code={0:d}", r);
        return false;
    }

    PUICCLIENT_ConnsCallbaks cbs;
    cbs.OnConnected = ProxyManager::OnPUICConnected;
    cbs.OnCanRead = ProxyManager::OnPUICanRead;
    cbs.OnCanWrite = ProxyManager::OnPUICanWrite;
    cbs.OnDisconnected = ProxyManager::OnPUICDisconnected;
    r = PUICCLIENT_DialerStart(dialer, &cbs, this);
    if (r != 0)
    {
        PUICCLIENT_DialerClose(dialer);
        logger->error("dialer start failed, code={0:d}", r);
        return false;
    }

    if (!m_listener.Start(this))
    {
        PUICCLIENT_DialerClose(dialer);
        return false;
    }

    m_dialer = dialer;
    return true;
}

void ProxyManager::Stop()
{
    m_listener.Close();

    PUICCLIENT_DialerClose(m_dialer);
    m_dialer = nullptr;
}

void ProxyManager::Monitor()
{
    for (auto itor = m_cleanJobs.begin(); itor != m_cleanJobs.end(); ++itor)
    {
        delete *itor;
    }
    m_cleanJobs.clear();
}

void ProxyManager::JobToClean(ProxyJob* job)
{
    m_cleanJobs.push_back(job);
}

void ProxyManager::OnAgentConnAccept(
    AgentConn* conn,
    const char* srcIp, int srcPort
    )
{
    spdlog::get(PROXY_LOGGER)->info("proxy new, client={0}:{1:d}",
        srcIp, srcPort);

    ProxyJob* job = new ProxyJob(*this, conn, srcIp, srcPort, m_fwdIp.c_str(), m_fwdPort);
    job->Start();
}

void ProxyManager::OnPUICConnected(
    void* userData,
    puicclient_conn conn
    )
{
    ProxyJob* job = (ProxyJob*)PUICCLIENT_ConnData(conn);
    if (job == nullptr)
        return;
    job->OnPUICConnConnected();
}

void ProxyManager::OnPUICanRead(
    void* userData,
    puicclient_conn conn
    )
{
    ProxyJob* job = (ProxyJob*)PUICCLIENT_ConnData(conn);
    if (job == nullptr)
        return;
    job->OnPUICConnCanRead();
}

void ProxyManager::OnPUICanWrite(
    void* userData,
    puicclient_conn conn,
    size_t remain
    )
{
    ProxyJob* job = (ProxyJob*)PUICCLIENT_ConnData(conn);
    if (job == nullptr)
        return;
    job->OnPUICConnCanWrite(remain);
}

void ProxyManager::OnPUICDisconnected(
    void* userData,
    puicclient_conn conn,
    int connErr, bool fromRemote, const char* details
    )
{
    ProxyJob* job = (ProxyJob*)PUICCLIENT_ConnData(conn);
    if (job == nullptr)
        return;
    job->OnPUICConnErr(connErr, fromRemote, details);
}

char PROXY_LOGGER[64];
