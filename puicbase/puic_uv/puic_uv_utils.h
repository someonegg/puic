// Copyright 2019 someonegg. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef PUIC_UV_UTILS_H_
#define PUIC_UV_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <ctime>

#include <uv.h>
#include "net/quic/core/quic_alarm.h"
#include "net/quic/platform/api/quic_clock.h"

namespace net {

class PuicUVAlarm : public QuicAlarm
{
    uv_loop_t* m_loop;
    const QuicClock* m_clock; // not owned.

    uv_timer_t* m_timer;

public:
    PuicUVAlarm(
        uv_loop_t* loop,
        const QuicClock* clock,
        QuicArenaScopedPtr<QuicAlarm::Delegate> delegate
        )
        : QuicAlarm(std::move(delegate))
        , m_loop(loop)
        , m_clock(clock)
        , m_timer(nullptr)
    {
    }

    ~PuicUVAlarm() override
    {
        if (m_timer != nullptr)
        {
            uv_close((uv_handle_t*)m_timer, (uv_close_cb)free);
            m_timer = nullptr;
        }
    }

    // QuicAlarm overrides.
    void SetImpl() override
    {
        QuicTime deadline = this->deadline(), now = m_clock->ApproximateNow();

        int64_t delay_ms = 0;
        if (deadline > now)
        {
            delay_ms = (deadline - now).ToMilliseconds();
            if (delay_ms < 1)
                delay_ms = 1; // The minimum is 1ms.
        }

        if (m_timer == nullptr)
        {
            m_timer = (uv_timer_t*)malloc(sizeof(uv_timer_t));
            uv_timer_init(m_loop, m_timer);
            m_timer->data = this;
        }

        uv_timer_start(m_timer, timerCallback, delay_ms, 0);
    }

    void CancelImpl() override
    {
        if (m_timer == nullptr)
            return;

        uv_timer_stop(m_timer);
    }

private:
    static void timerCallback(uv_timer_t* handle)
    {
        if (uv_is_closing((uv_handle_t*)handle))
            return;

        PuicUVAlarm* pThis = (PuicUVAlarm*)(handle->data);
        pThis->Fire();
    }
};

class PuicUVClock : public QuicClock
{
    uv_loop_t* m_loop;

public:
    PuicUVClock(uv_loop_t* loop) : m_loop(loop)
    {
    }

    QuicTime ApproximateNow() const override
    {
        return QuicTime::Zero() +
            QuicTime::Delta::FromMicroseconds(uv_hrnow(m_loop) / 1000);
    }
    QuicTime Now() const override
    {
        return QuicTime::Zero() +
            QuicTime::Delta::FromMicroseconds(uv_hrtime() / 1000);
    }
    QuicWallTime WallNow() const override
    {
        // TODO
        return QuicWallTime::FromUNIXMicroseconds(uint64_t(time(nullptr))*1e6);
    }
};

} // namespace net

#endif // PUIC_UV_UTILS_H_
