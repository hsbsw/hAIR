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

#include <TFT_eSPI.h>
#include <mutex>

// Forward declare SensorData type
struct SensorData;

class Display
{
  public:
    Display(TFT_eSPI& tft) : tft(tft) {}

    void printDebugMessage(const String& text);
    void printErrorMessage(const String& text);
    void printSensorData(const SensorData& sensorData);

    inline void setDefaultColor() { tft.setTextColor(TFT_WHITE, TFT_BLACK); }

    inline void setRedColor() { tft.setTextColor(TFT_RED, TFT_BLACK); }

  private:
    TFT_eSPI& tft;
    std::mutex m_mtx;

    /// Employ some caching
    enum class DisplayTextConfig
    {
        POST, // right after startup
        SensorData,
        DebugText,
    } currentDTC{DisplayTextConfig::POST};

    void setCursor(int x, int y);
    void setLineColumn(int line, int column);
    void drawLine(const char* text, double value, int line);
    void drawLine(const char* text, int line);
};
