////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// hAIR - HSB Air Station
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MIT License
///
/// Copyright (c) 2021 hsbsw (https://github.com/hsbsw)
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Arduino.h>
#include <Fs.h>
#include <mutex>

template <typename T>
inline bool isBetween(T val, T min, T max)
{
    return (min < val) && (val < max);
}

template <typename T>
inline bool isWithin(T val, T min, T max)
{
    return (min <= val) && (val <= max);
}

class AutoLock
{
  public:
    AutoLock(std::mutex& mtx) : m_mtx(mtx) { m_mtx.lock(); }
    ~AutoLock() { m_mtx.unlock(); }

  private:
    std::mutex& m_mtx;
};

using Timestamp = int32_t;

inline bool dt_max(Timestamp now, Timestamp last, Timestamp max)
{
    return now - last > max;
}

class TaskItem
{
  public:
    inline bool shallRun(Timestamp now = millis())
    {
        if (m_frequency == 0 || m_delayTime == 0)
        {
            return false;
        }

        const auto shall{dt_max(now, m_ts_lastTry, m_delayTime)};
        if (shall)
        {
            updateTry(now);
        }
        return shall;
    }

    inline void updateTry(Timestamp now = millis())
    {
        m_ts_lastTry = now;
    }

    inline void updateSuccess(Timestamp now = millis())
    {
        m_ts_lastSuccess = now;
    }

    inline Timestamp getLastTry()
    {
        return m_ts_lastTry;
    }

    inline Timestamp getLastSuccess()
    {
        return m_ts_lastSuccess;
    }

    inline void setFrequency(float frequency)
    {
        m_frequency = frequency;
        m_delayTime = 1000.0f / frequency;
    }

    inline void setDelayTime(int32_t delayTime)
    {
        m_frequency = static_cast<int32_t>(1000.0f / delayTime);
        m_delayTime = delayTime;
    }

  private:
    int32_t m_ts_lastTry{};
    int32_t m_ts_lastSuccess{};
    int32_t m_delayTime{};
    float m_frequency{};
};

void listDir(fs::FS& fs, const char* dirname, uint8_t levels);

String getFormattedTime(unsigned long secs);
String getFormattedDate(unsigned long secs);
