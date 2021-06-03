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

#include "Display.h"
#include "Utilities.h"
#include "hAIR.h"

#define XMIN       (10)
#define YMIN       (20)
#define LINE_INC   (30)
#define COLUMN_INC (30)

void Display::setCursor(int x, int y)
{
    tft.setCursor(XMIN + x, YMIN + y);
}

void Display::setLineColumn(int line, int column)
{
    setCursor(COLUMN_INC * column, LINE_INC * line);
}

void Display::drawLine(const char* text, double value, int line)
{
    setLineColumn(line, 0);
    tft.print(text);
    setLineColumn(line, 1);
    tft.print(value);
}

void Display::drawLine(const char* text, int line)
{
    setLineColumn(line, 0);
    tft.print(text);
}

void Display::printDebugMessage(const String& text)
{
    AutoLock lock(m_mtx);

    if (currentDTC != DisplayTextConfig::DebugText)
    {
        setRedColor();
        tft.setTextSize(1);
        tft.setTextWrap(true, false);
        currentDTC = DisplayTextConfig::DebugText;
    }

    tft.fillRect(0, tft.getViewportHeight() * 2 / 3, tft.getViewportWidth(), tft.getViewportHeight(), TFT_BLACK);
    tft.setCursor(0, tft.getViewportHeight() * 2 / 3);
    tft.println(text.c_str());
}

void Display::printErrorMessage(const String& text)
{
    printDebugMessage(text);
}

void Display::printSensorData(const SensorData& sensorData)
{
    AutoLock lock(m_mtx);

    if (currentDTC != DisplayTextConfig::SensorData)
    {
        setDefaultColor();
        tft.setTextSize(2);
        tft.setTextWrap(false, false);
        currentDTC = DisplayTextConfig::SensorData;
    }

    //tft.fillScreen(TFT_BLACK);
    //tft.fillRect(0, 0, tft.getViewportWidth(), tft.getViewportHeight() * 2 / 3, TFT_BLACK);
    tft.setCursor(0, 0);

    tft.print("hAIR - ");
    tft.println(HAIR_VERSION_STRING);
    tft.print("TVOC [ppb]\n ");
    tft.print(sensorData.sgp_iaq.TVOC);
    tft.println("      ");
    tft.print("eCO2 [ppm]\n ");
    tft.print(sensorData.sgp_iaq.eCO2);
    tft.println("      ");
    tft.print("H2 [1]\n ");
    tft.print(sensorData.sgp_iaqRaw.rawH2);
    tft.println("      ");
    tft.print("ethanol [1]\n ");
    tft.print(sensorData.sgp_iaqRaw.rawEthanol);
    tft.println("      ");
}
