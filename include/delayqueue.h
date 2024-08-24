//
// delayqueue.h
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2023 Dale Whinham <daleyo@gmail.com>
//
// This file is part of mt32-pi.
//
// mt32-pi is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// mt32-pi is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// mt32-pi. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _delayqueue_h
#define _delayqueue_h

#include <stdint.h>
#include <assert.h>
#include <circle/types.h>

template<typename T, size_t N>
class CDelayQueue
{
public:
    CDelayQueue()
        : m_rp(0), m_wp(0)
	{
	}

	~CDelayQueue() {}

	void Reset()
	{
        m_rp = 0;
        m_wp = 0;
	}

    int IsEmpty(void)
    {
        return m_rp == m_wp;
    }

    uint32_t Peek(void)
    {
        if (m_rp == m_wp)
        {
            return UINT32_MAX;
        }
        else
        {
            return m_timestamp[m_rp];
        }
    }

    T Dequeue(void)
    {
        if (m_rp != m_wp)
        {
            T ret = m_queue[m_rp];
            m_rp = (m_rp + 1) % N;
            return ret;
        }
        else
        {
            return 0;
        }
    }

    void Enqueue(uint32_t t, T v)
    {
        m_timestamp[m_wp] = t;
        m_queue[m_wp] = v;
        m_wp = (m_wp + 1) % N;
    }

private:
    u32 m_rp;
    u32 m_wp;

	u32 m_timestamp[N];
    T m_queue[N];
};

#endif
