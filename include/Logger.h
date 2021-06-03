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

#include "Display.h"
#include "Utilities.h"
#include <NTPClient.h>
#include <WebSocketsServer.h>
#include <iomanip>
#include <iostream>
#include <plog/Appenders/IAppender.h>
#include <plog/Util.h>

class hAIR_Formatter
{
public:
    hAIR_Formatter(NTPClient& ntpclient)
        : ntpclient(ntpclient)
    {
    }

    plog::util::nstring header()
    {
        return plog::util::nstring();
    }

    plog::util::nstring format(const plog::Record& record)
    {
        const auto dateTime{getFormattedDate(ntpclient.getEpochTime())};
        const auto now{millis()};
        const auto severity{severityToString(record.getSeverity())};
        const auto threadID{record.getTid()};
        const auto func{record.getFunc()};
        const auto line{record.getLine()};
        const auto message{record.getMessage()};

        // TODO(hsbsw) get threadID/threadName for logging
        //extern void vTaskGetInfo(TaskHandle_t xTask, TaskStatus_t* pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState) PRIVILEGED_FUNCTION;
        //extern void* pxCurrentTCB;
        //TaskStatus_t xTaskDetails;
        //vTaskGetInfo(xTaskGetCurrentTaskHandle(),
        //             &xTaskDetails,
        //             pdFALSE,   // Include the high water mark in xTaskDetails.
        //             eRunning); // Include the task state in xTaskDetails.
        //const auto threadID{xTaskDetails.uxTaskNumber};
        //const auto threadName{pcTaskGetName(xTaskGetCurrentTaskHandle())};

        //2004-02-12T15:19:21+00:00 [  12345678] [INFO ] [0] [hAIR_System::threadFunction_sensorDataDistribution@315] MESSAGE
        plog::util::nostringstream ss;
        ss << dateTime.c_str() << PLOG_NSTR(' ')                                                                         // Time
           << PLOG_NSTR('[') << std::setfill(PLOG_NSTR(' ')) << std::setw(10) << std::right << now << PLOG_NSTR("] ")    // millis (will rollover after 49.7 days, hence 10 is enough)
           << PLOG_NSTR('[') << std::setfill(PLOG_NSTR(' ')) << std::setw(5) << std::left << severity << PLOG_NSTR("] ") // Severity
           << PLOG_NSTR('[') << threadID << PLOG_NSTR("] ")                                                              // Thread ID
           << PLOG_NSTR('[') << func << PLOG_NSTR('@') << line << PLOG_NSTR("] ")                                        // Function
           << message;                                                                                                   // Message

        return ss.str();
    }

private:
    NTPClient& ntpclient;
};

// All appenders MUST inherit IAppender interface.
class hAIR_Appender : public plog::IAppender
{
public:
    hAIR_Appender(hAIR_Formatter& formatter, Display& display, WebSocketsServer& websocket)
        : formatter(formatter), display(display), websocket(websocket)
    {
    }

    // This is a method from IAppender that MUST be implemented.
    virtual void write(const plog::Record& record)
    {
        // Use the formatter to get a string from a record.
        plog::util::nstring str = formatter.format(record);

        // Log to Serial
        Serial.println(str.c_str());

        // Log to Display
        if (record.getSeverity() <= plog::error)
        {
            display.printErrorMessage(str.c_str());
        }
        else
        {
            display.printDebugMessage(str.c_str());
        }

        // Log to WebSocket
        plog::util::nostringstream ss;
        ss << "{\"logMessage\":\""
           << str
           << "\"}";
        const auto jsonStr = ss.str();
        websocket.broadcastTXT(jsonStr.c_str(), jsonStr.size());
    }

private:
    hAIR_Formatter&   formatter;
    Display&          display;
    WebSocketsServer& websocket;
};
