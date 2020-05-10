// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "puic_proxy.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <spdlog/spdlog.h>

const size_t TCPCONN_READBUF_SIZE = 16 * 1024;

class TCPConn
{
public:
    class Callback
    {
    public:
        virtual ~Callback() {}

        virtual void OnTCPConnErr(const char* op, int err) = 0;
        virtual void OnTCPConnBound(const char* locIp, int locPort) = 0;
        virtual void OnTCPConnEOF() = 0;
        virtual void OnTCPConnRcvd(const char* data, size_t len) = 0;
        virtual void OnTCPConnSent(size_t len) = 0;
    };

private:
    bool m_useProxyProtocol;
    Callback* m_callback;

    uv_tcp_t m_socket;
    uv_connect_t m_connect;
    uv_shutdown_t m_shutdown;

    bool m_reading;
    char m_readBuf[TCPCONN_READBUF_SIZE];

    bool m_writing;
    uv_write_t m_write;
    char m_pph[256]; // proxy protocol head, enough.
    uv_write_t m_pphWrite;

    TCPConn(bool useProxyProtocol)
        : m_useProxyProtocol(useProxyProtocol)
        , m_callback(nullptr)
        , m_reading(false)
        , m_writing(false)
        , m_pph()
    {
    }

    ~TCPConn()
    {
        assert(m_callback == nullptr);
    }

public:
    static TCPConn* Create(bool useProxyProtocol)
    {
        return new TCPConn(useProxyProtocol);
    }

    inline bool reading() const { return m_reading; }
    inline bool writing() const { return m_writing; }

    void Start(uv_loop_t* loop, const sockaddr* addr, Callback* callback,
        const char* ppsrcIp, int ppsrcPort, const char* ppdstIp, int ppdstPort)
    {
        m_callback = callback;

        uv_tcp_init(loop, &m_socket);
        uv_tcp_nodelay(&m_socket, 1);
        m_socket.data = this;

        int r = uv_tcp_connect(&m_connect, &m_socket, addr, on_uv_connect);
        if (r != 0)
        {
            m_callback->OnTCPConnErr("tcp_connect", r);
            return;
        }
        else
        {
            int delay = 1; // 1s
            uv_tcp_keepalive(&m_socket, 1, delay);
            int rcv_size = 512 * 1024; // 512k
            uv_recv_buffer_size((uv_handle_t*)&m_socket, &rcv_size);
            int snd_size = 512 * 1024; // 512k
            uv_send_buffer_size((uv_handle_t*)&m_socket, &snd_size);
        }

        if (m_useProxyProtocol)
            SendPPH(ppsrcIp, ppsrcPort, ppdstIp, ppdstPort);

        return;
    }

    void Close()
    {
        uv_close((uv_handle_t*)&m_socket, on_uv_close);
        m_callback = nullptr;
    }

    void StartRead()
    {
        if (reading())
            return;

        int r = uv_read_start((uv_stream_t*)&m_socket, on_uv_alloc, on_uv_read);
        if (r != 0)
        {
            m_callback->OnTCPConnErr("tcp_read", r);
            return;
        }
        m_reading = true;
    }

    void StopRead()
    {
        if (!reading())
            return;

        uv_read_stop((uv_stream_t*)&m_socket);
        m_reading = false;
    }

    void SendPPH(const char* ppsrcIp, int ppsrcPort, const char* ppdstIp, int ppdstPort)
    {
        char* ipv6 = strchr(const_cast<char*>(ppsrcIp), ':');
        const char* format_s = (ipv6 == nullptr ?
            "PROXY TCP4 %s %s %d %d\r\n" : "PROXY TCP6 %s %s %d %d\r\n");

        uv_buf_t buf;
        buf.base = m_pph;
        buf.len = snprintf(m_pph, sizeof(m_pph),
            format_s, ppsrcIp, ppdstIp, ppsrcPort, ppdstPort);

        int r = uv_write(&m_pphWrite, (uv_stream_t*)&m_socket, &buf, 1, nullptr);
        if (r != 0)
        {
            m_callback->OnTCPConnErr("tcp_write_pph", r);
            return;
        }
    }

    void Write(const uv_buf_t bufs[], int nbufs, size_t len)
    {
        assert(!writing());
        if (writing() || len == 0)
            return;

        m_write.data = (void*)len;
        int r = uv_write(&m_write, (uv_stream_t*)&m_socket, bufs, nbufs, on_uv_write);
        if (r != 0)
        {
            m_callback->OnTCPConnErr("tcp_write", r);
            return;
        }
        m_writing = true;
    }

    void Shutdown()
    {
        int r = uv_shutdown(&m_shutdown, (uv_stream_t*)&m_socket, on_uv_shutdown);
        if (r != 0)
        {
            m_callback->OnTCPConnErr("tcp_shutdown", r);
            return;
        }
    }

private:
    static void on_uv_connect(uv_connect_t* req, int status)
    {
        uv_stream_t* handle = req->handle;
        if(uv_is_closing((uv_handle_t*)handle))
            return;

        TCPConn* pThis = (TCPConn*)handle->data;

        if (status != 0)
        {
            pThis->m_callback->OnTCPConnErr("tcp_connect", status);
            return;
        }

        sockaddr_storage bound_ss = {0};
        int len_ss = sizeof(bound_ss);
        uv_tcp_getsockname(&pThis->m_socket, (sockaddr*)&bound_ss, &len_ss);

        if (bound_ss.ss_family == AF_INET)
        {
            sockaddr_in* in = (sockaddr_in*)&bound_ss;
            char name[64] = {0};
            uv_ip4_name(in, name, sizeof(name));
            pThis->m_callback->OnTCPConnBound(name, ntohs(in->sin_port));
        }
        else if (bound_ss.ss_family == AF_INET6)
        {
            sockaddr_in6* in6 = (sockaddr_in6*)&bound_ss;
            char name[64] = {0};
            uv_ip6_name(in6, name, sizeof(name));
            pThis->m_callback->OnTCPConnBound(name, ntohs(in6->sin6_port));
        }
    }

    static void on_uv_close(uv_handle_t* handle)
    {
        TCPConn* pThis = (TCPConn*)handle->data;
        delete pThis;
    }

    static void on_uv_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
    {
        if (uv_is_closing((uv_handle_t*)handle))
        {
            *buf = uv_buf_init(nullptr, 0);
            return;
        }

        TCPConn* pThis = (TCPConn*)handle->data;
        *buf = uv_buf_init(pThis->m_readBuf, TCPCONN_READBUF_SIZE);
    }

    static void on_uv_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf)
    {
        if (uv_is_closing((uv_handle_t*)handle))
            return;

        TCPConn* pThis = (TCPConn*)handle->data;

        if (nread > 0)
        {
            pThis->m_callback->OnTCPConnRcvd(buf->base, nread);
            return;
        }

        if (nread == UV_EOF)
        {
            pThis->m_callback->OnTCPConnEOF();
            return;
        }

        if (nread < 0)
        {
            pThis->m_callback->OnTCPConnErr("tcp_read", nread);
            return;
        }
    }

    static void on_uv_write(uv_write_t* req, int status)
    {
        uv_stream_t* handle = req->handle;
        if(uv_is_closing((uv_handle_t*)handle))
            return;

        TCPConn* pThis = (TCPConn*)handle->data;

        if (status != 0)
        {
            pThis->m_callback->OnTCPConnErr("tcp_write", status);
            return;
        }

        assert(pThis->writing());
        pThis->m_writing = false;

        size_t len = (size_t)req->data;
        pThis->m_callback->OnTCPConnSent(len);
    }

    static void on_uv_shutdown(uv_shutdown_t* req, int status)
    {
        uv_stream_t* handle = req->handle;
        if(uv_is_closing((uv_handle_t*)handle))
            return;

        TCPConn* pThis = (TCPConn*)handle->data;

        if (status != 0)
        {
            pThis->m_callback->OnTCPConnErr("tcp_shutdown", status);
            return;
        }
    }
};

class ProxyJob : public TCPConn::Callback
{
    ProxyManager* m_manager;
    bool m_running;

    puicserver_conn m_srcConn;
    std::string m_srcIp;
    int m_srcPort;

    TCPConn* m_dstConn;
    std::string m_dstIp; // conn local address.
    int m_dstPort;

public:
    ProxyJob(
        ProxyManager* manager,
        puicserver_conn srcConn,
        const char* srcIp, int srcPort
        )
        : m_manager(manager)
        , m_running(false)
        , m_srcConn(srcConn)
        , m_srcIp(srcIp)
        , m_srcPort(srcPort)
        , m_dstConn(nullptr)
        , m_dstPort(0)
    {
        PUICSERVER_SetConnData(m_srcConn, this);
    }

    ~ProxyJob() override
    {
        spdlog::get(PROXY_LOGGER)->info(
            "proxy delete, client={0}:{1:d}, local={2}:{3:d}",
            m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort);
    }

    void Start(uv_loop_t* loop, const sockaddr* dst_addr, bool useProxyProtocol)
    {
        m_running = true;

        m_dstConn = TCPConn::Create(useProxyProtocol);

        m_dstConn->Start(loop, dst_addr, this,
            m_srcIp.c_str(), m_srcPort,
            m_manager->GetListenIp(), m_manager->GetListenPort());
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
            PUICSERVER_SetConnData(m_srcConn, nullptr);
            PUICSERVER_ConnDisconnect(m_srcConn);
            m_srcConn = nullptr;
        }

        if (m_dstConn != nullptr)
        {
            m_dstConn->Close();
            m_dstConn = nullptr;
        }

        m_manager->JobToClean(this);
    }

    void OnTCPConnErr(const char* op, int err) override
    {
        spdlog::get(PROXY_LOGGER)->info(
            "tcp error, op={0}, code={1:d}, client={2}:{3:d}, local={4}:{5:d}",
            op, err, m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort);
        clean();
    }

    void OnTCPConnBound(const char* locIp, int locPort) override
    {
        assert(running());

        m_dstIp = locIp;
        m_dstPort = locPort;

        spdlog::get(PROXY_LOGGER)->info(
            "proxy start, client={0}:{1:d}, local={2}:{3:d}",
            m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort);

        m_dstConn->StartRead();
        return;
    }

    void OnTCPConnEOF() override
    {
        assert(running());
        spdlog::get(PROXY_LOGGER)->info(
            "tcp EOF, client={0}:{1:d}, server={2}:{3:d}",
            m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort);

        size_t buffered = 0;
        PUICSERVER_ConnWrite(m_srcConn, nullptr, 0, true, &buffered);
        return;
    }

    void OnTCPConnRcvd(const char* data, size_t len) override
    {
        assert(running());

        size_t buffered = 0;
        PUICSERVER_ConnWrite(m_srcConn, data, len, false, &buffered);
        if (!running())
            return;

        may_tcp_to_puic(buffered);
        return;
    }

    void OnTCPConnSent(size_t len) override
    {
        assert(running());

        PUICSERVER_ConnMarkConsumed(m_srcConn, len);
        if (!running())
            return;

        may_puic_to_tcp();
        return;
    }

    friend class ProxyManager;

    void OnPUICConnErr(int connErr, bool fromRemote, const char* details)
    {
        const char* from = fromRemote ? "remote" : "self";
        spdlog::get(PROXY_LOGGER)->info(
            "puic error, code={0:d}, from={1}, client={2}:{3:d}, local={4}:{5:d}, details={6}",
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
        if (m_dstConn->writing())
            return;

        // 4 is the capacity of uv_write_t.bufsml.
        const char* regs[4];
        size_t lens[4];
        uv_buf_t bufs[4];
        int count = 4;

        int r = PUICSERVER_ConnReadableRegions(m_srcConn, regs, lens, &count);
        if (!running())
            return;

        if (r == PUISRVERR_EOF)
        {
            spdlog::get(PROXY_LOGGER)->info(
                "puic EOF, client={0}:{1:d}, server={2}:{3:d}",
                m_srcIp.c_str(), m_srcPort, m_dstIp.c_str(), m_dstPort);
            m_dstConn->Shutdown();
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

        m_dstConn->Write(bufs, count, len);
        return;
    }

    void may_tcp_to_puic(size_t buffered)
    {
        if (buffered >= PUICCONN_WRITEBUF_UPPER)
        {
            m_dstConn->StopRead();
            return;
        }

        if (buffered <= PUICCONN_WRITEBUF_LOWER)
        {
            m_dstConn->StartRead();
            return;
        }
    }
};

ProxyManager::ProxyManager(
    uv_loop_t* loop, bool useProxyProtocol,
    const char* lisIp, int lisPort, const sockaddr_storage &lisSS,
    const char* fwdIp, int fwdPort, const sockaddr_storage &fwdSS
    )
    : m_loop(loop)
    , m_useProxyProtocol(useProxyProtocol)
    , m_listenIp(lisIp)
    , m_listenPort(lisPort)
    , m_forwardSS(fwdSS)
    , m_listener(nullptr)
{
}

ProxyManager::~ProxyManager()
{
    assert(m_listener == nullptr);
}

bool ProxyManager::Start()
{
    auto logger = spdlog::get(PROXY_LOGGER);

    puicserver_listener listener = nullptr;
    int r = PUICSERVER_CreateListener(m_loop, m_listenIp.c_str(), m_listenPort, &listener);
    if (r != 0)
    {
        logger->error("create listener failed, code={0:d}", r);
        return false;
    }

    PUICSERVER_ConnsCallbaks cbs;
    cbs.OnPreAccept = ProxyManager::OnPUICPreAccept;
    cbs.OnAccept = ProxyManager::OnPUICAccept;
    cbs.OnCanRead = ProxyManager::OnPUICanRead;
    cbs.OnCanWrite = ProxyManager::OnPUICanWrite;
    cbs.OnDisconnected = ProxyManager::OnPUICDisconnected;
    r = PUICSERVER_ListenerStart(listener, &cbs, this);
    if (r != 0)
    {
        PUICSERVER_ListenerClose(listener);
        logger->error("listener start failed, code={0:d}", r);
        return false;
    }

    m_listener = listener;
    return true;
}

void ProxyManager::Stop()
{
    PUICSERVER_ListenerClose(m_listener);
    m_listener = nullptr;
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

void ProxyManager::OnPUICPreAccept(
    void* userData,
    const char* clientIp, int clientPort
    )
{
    spdlog::get(PROXY_LOGGER)->info("proxy pre, client={0}:{1:d}", clientIp, clientPort);
}

void ProxyManager::OnPUICAccept(
    void* userData,
    puicserver_conn conn,
    const char* clientIp, int clientPort
    )
{
    spdlog::get(PROXY_LOGGER)->info("proxy new, client={0}:{1:d}", clientIp, clientPort);

    ProxyManager* pThis = (ProxyManager*)userData;

    ProxyJob* job = new ProxyJob(pThis, conn, clientIp, clientPort);
    job->Start(pThis->m_loop, (const sockaddr*)&pThis->m_forwardSS, pThis->UseProxyProtocol());
}

void ProxyManager::OnPUICanRead(
    void* userData,
    puicserver_conn conn
    )
{
    ProxyJob* job = (ProxyJob*)PUICSERVER_ConnData(conn);
    if (job == nullptr)
        return;
    job->OnPUICConnCanRead();
}

void ProxyManager::OnPUICanWrite(
    void* userData,
    puicserver_conn conn,
    size_t remain
    )
{
    ProxyJob* job = (ProxyJob*)PUICSERVER_ConnData(conn);
    if (job == nullptr)
        return;
    job->OnPUICConnCanWrite(remain);
}

void ProxyManager::OnPUICDisconnected(
    void* userData,
    puicserver_conn conn,
    int connErr, bool fromRemote, const char* details
    )
{
    ProxyJob* job = (ProxyJob*)PUICSERVER_ConnData(conn);
    if (job == nullptr)
        return;
    job->OnPUICConnErr(connErr, fromRemote, details);
}

char PROXY_LOGGER[64];
