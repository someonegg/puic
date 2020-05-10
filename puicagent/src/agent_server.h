// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef __AGENT_SERVER_H__
#define __AGENT_SERVER_H__

#include <uv.h>

const size_t TCPCONN_BACKLOG = 256;
const size_t TCPCONN_READBUF_SIZE = 16 * 1024;

class AgentServer;

class AgentConn
{
public:
    class Callback
    {
    public:
        virtual ~Callback() {}

        virtual void OnAgentConnErr(const char* op, int err) = 0;
        virtual void OnAgentConnEOF() = 0;
        virtual void OnAgentConnRcvd(const char* data, size_t len) = 0;
        virtual void OnAgentConnSent(size_t len) = 0;
    };

private:
    AgentServer &m_server;

    uv_tcp_t m_socket;
    uv_shutdown_t m_shutdown;

    Callback* m_callback;

    bool m_reading;
    char m_readBuf[TCPCONN_READBUF_SIZE];

    bool m_writing;
    uv_write_t m_write;

    AgentConn(AgentServer &server);
    ~AgentConn();

public:
    static AgentConn* Create(AgentServer &server);

    inline bool reading() const { return m_reading; }
    inline bool writing() const { return m_writing; }

    void Accept();

    void Proxy(Callback* cb);
    void Close();

    void StartRead();
    void StopRead();

    void Write(const uv_buf_t bufs[], int nbufs, size_t len);
    void Shutdown();

private:
    void close();
    void client_handshaked();

    static void on_uv_close(uv_handle_t* handle);
    static void on_uv_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void on_uv_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);
    static void on_uv_write(uv_write_t* req, int status);
    static void on_uv_shutdown(uv_shutdown_t* req, int status);
};

class AgentServer
{
public:
    class Callback
    {
    public:
        virtual ~Callback() {}

        virtual void OnAgentConnAccept(
            AgentConn* conn,
            const char* srcIp, int srcPort
            ) = 0;
    };

private:
    uv_loop_t* m_loop;
    sockaddr_storage m_listenSS;

    uv_tcp_t* m_listener;
    Callback* m_callback;

public:
    AgentServer(uv_loop_t* loop, const sockaddr_storage &lisSS);
    ~AgentServer();

    bool Start(Callback* cb);

    // The sub-AgentConns are not affected.
    void Close();

public:
    inline uv_loop_t* loop() const { return m_loop; }
    inline uv_stream_t* listener() const { return (uv_stream_t*)m_listener; }
    inline Callback* callback() const { return m_callback; }

private:
    void close();

    static void on_uv_connection(uv_stream_t *server, int status);
};

extern char Agent_LOGGER[64];

#endif // __AGENT_SERVER_H__
