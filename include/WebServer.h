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
#include <ESPAsyncWebServer.h>

// Forward declare hAIR_System type
class hAIR_System;

class WebServer
{
public:
    enum class HTTPStatusCode
    {
        Ok            = 200,
        NotFound      = 404,
        NotAcceptable = 406
    };

    WebServer(hAIR_System& hAIR, AsyncWebServer& asyncWebserver)
        : hAIR(hAIR), asyncWebserver(asyncWebserver)
    {
    }

    bool init();

private:
    hAIR_System&    hAIR;
    AsyncWebServer& asyncWebserver;

    ////////////////////////////////
    /// Logging
    ////////////////////////////////

    void logRequest(AsyncWebServerRequest* request);
    auto logReply(AsyncWebServerRequest* request, HTTPStatusCode code) -> int32_t;

    ////////////////////////////////
    /// Root Callbacks
    ////////////////////////////////

    void onRoot(AsyncWebServerRequest* request);         // return index.html
    void onPageNotFound(AsyncWebServerRequest* request); // display 404

    ////////////////////////////////
    /// Webpage Callbacks
    ////////////////////////////////

    // Misc
    void onRestartHAIR(AsyncWebServerRequest* request);
    void onSensordata(AsyncWebServerRequest* request); // return json str of sensor data

    // Logger
    void onGetLoggerSeverity(AsyncWebServerRequest* request);
    void onSetLoggerSeverity(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);

    // Config
    void onDownloadConfig(AsyncWebServerRequest* request);
    void onUploadConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
};
