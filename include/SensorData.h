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

#include "Utilities.h"
#include <Arduino.h>
#include <sstream>

struct SensorData
{
    template<typename T>
    static String helper_toJSONtxt_raw(const T& obj)
    {
        std::stringstream ss;
        obj.appendJSONtxt(ss);
        return ss.str().c_str();
    }

    template<typename T>
    static String helper_toJSONtxt(const T& obj)
    {
        std::stringstream ss;
        ss << '{';
        obj.appendJSONtxt(ss);
        ss << '}';
        return ss.str().c_str();
    }

    struct SGP_IAQ
    {
        bool isValid;

        uint16_t TVOC; // [ppb]
        uint16_t eCO2; // [ppm]

        void appendJSONtxt(std::stringstream& ss) const
        {
            ss << "\"SGP30_IAQ\": {"
               << " \"TVOC\": " << TVOC << ","
               << " \"eCO2\": " << eCO2 << " "
               << "}";
        }

        String toJSONtxt_raw() const
        {
            return SensorData::helper_toJSONtxt_raw<SGP_IAQ>(*this);
        }
        String toJSONtxt() const
        {
            return SensorData::helper_toJSONtxt<SGP_IAQ>(*this);
        }
    };

    struct SGP_IAQraw
    {
        bool isValid;

        uint16_t rawH2;      // [AU]
        uint16_t rawEthanol; // [AU]

        void appendJSONtxt(std::stringstream& ss) const
        {
            ss << "\"SGP30_IAQraw\": {"
               << " \"rawH2\": " << rawH2 << ","
               << " \"rawEthanol\": " << rawEthanol << " "
               << "}";
        }

        String toJSONtxt_raw() const
        {
            return SensorData::helper_toJSONtxt_raw<SGP_IAQraw>(*this);
        }
        String toJSONtxt() const
        {
            return SensorData::helper_toJSONtxt<SGP_IAQraw>(*this);
        }
    };

    struct BME_Data
    {
        bool isValid;

        float temperature{22.1}; // [°C]
        float humidity{45.2};    // [%] / [%RH]
        float pressure{1013.25}; // [hPa]

        void appendJSONtxt(std::stringstream& ss) const
        {
            ss << "\"BMExxx_Data\": {"
               << " \"temperature\": " << temperature << ","
               << " \"humidity\": " << humidity << ","
               << " \"pressure\": " << pressure << " "
               << "}";
        }

        String toJSONtxt_raw() const
        {
            return SensorData::helper_toJSONtxt_raw<BME_Data>(*this);
        }
        String toJSONtxt() const
        {
            return SensorData::helper_toJSONtxt<BME_Data>(*this);
        }
    };

    void appendJSONtxt(std::stringstream& ss) const
    {
        ss << "\"hAIR\": {";
        sgp_iaq.appendJSONtxt(ss);
        ss << ", ";
        sgp_iaqRaw.appendJSONtxt(ss);
        ss << ", ";
        bme_data.appendJSONtxt(ss);
        ss << "}";
    }

    String toJSONtxt_raw() const
    {
        return SensorData::helper_toJSONtxt_raw<SensorData>(*this);
    }
    String toJSONtxt() const
    {
        return SensorData::helper_toJSONtxt<SensorData>(*this);
    }

    bool isValid() const
    {
        return sgp_iaq.isValid && sgp_iaqRaw.isValid && bme_data.isValid;
    }

    SGP_IAQ    sgp_iaq;
    SGP_IAQraw sgp_iaqRaw;
    BME_Data   bme_data;
};

class SensorDataStorage
{
public:
    SensorData getCopy()
    {
        m_mtx.lock();
        SensorData tmp{m_data};
        m_mtx.unlock();
        return tmp;
    }

    void update(SensorData data)
    {
        m_mtx.lock();
        m_data = data;
        m_mtx.unlock();
    }

    String getCopyAsJSONtxt()
    {
        return getCopy().toJSONtxt();
    }

private:
    SensorData m_data;
    std::mutex m_mtx{};
};
