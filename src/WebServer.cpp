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

#include "WebServer.h"
#include "hAIR.h"
#include <ArduinoJson.h>
#include <LITTLEFS.h>
#include <plog/Init.h>
#include <plog/Log.h>
#include <plog/Logger.h>

bool WebServer::init()
{
    ////////////////////////////////
    /// Root
    ////////////////////////////////

    asyncWebserver.on("/", [&](AsyncWebServerRequest* request) { onRoot(request); });
    asyncWebserver.onNotFound([&](AsyncWebServerRequest* request) { onPageNotFound(request); });

    ////////////////////////////////
    /// Misc
    ////////////////////////////////

    asyncWebserver.on("/restartHAIR", [&](AsyncWebServerRequest* request) { onRestartHAIR(request); });
    asyncWebserver.on("/sensordata", [&](AsyncWebServerRequest* request) { onSensordata(request); });

    ////////////////////////////////
    /// Logger
    ////////////////////////////////

    asyncWebserver.on("/getLoggerSeverity", [&](AsyncWebServerRequest* request) { onGetLoggerSeverity(request); });
    asyncWebserver.on(
        "/setLoggerSeverity",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [&](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) { onSetLoggerSeverity(request, data, len, index, total); });

    ////////////////////////////////
    /// Config
    ////////////////////////////////

    asyncWebserver.on("/downloadConfig", [&](AsyncWebServerRequest* request) { onDownloadConfig(request); });
    asyncWebserver.on(
        "/uploadConfig",
        HTTP_POST,
        [](AsyncWebServerRequest* request) {},
        nullptr,
        [&](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) { onUploadConfig(request, data, len, index, total); });

    return true;
}

void WebServer::logRequest(AsyncWebServerRequest* request)
{
    PLOGD << "HTTP ["
          << request->methodToString()
          << "] Request from ["
          << request->client()->remoteIP().toString().c_str()
          << "] of [" << request->url().c_str() << "]";
}

////////////////////////////////
/// Root
////////////////////////////////

/// Main page (more or less index.html)
void WebServer::onRoot(AsyncWebServerRequest* request)
{
    logRequest(request);

    request->send(LITTLEFS, "/index.html", "text/html");
}

/// Send 404 if requested file does not exist
void WebServer::onPageNotFound(AsyncWebServerRequest* request)
{
    logRequest(request);

    PLOGI << "Unknown URL " << request->url().c_str();
    request->send(404, "text/plain", "404: Not found");
}

////////////////////////////////
/// Misc
////////////////////////////////

void WebServer::onRestartHAIR(AsyncWebServerRequest* request)
{
    logRequest(request);

    PLOGN << "Restarting hAIR...";
    delay(1000);
    ESP.restart();
}

void WebServer::onSensordata(AsyncWebServerRequest* request)
{
    logRequest(request);

    const auto json = hAIR.sensorData.getCopyAsJSONtxt();
    request->send(200, "application/json", json.c_str());
}

////////////////////////////////
/// Logger
////////////////////////////////

void WebServer::onGetLoggerSeverity(AsyncWebServerRequest* request)
{
    logRequest(request);

    String json = "{\"severity\":";
    json += plog::get()->getMaxSeverity();
    json += "}";
    request->send(200, "application/json", json);
}

void WebServer::onSetLoggerSeverity(AsyncWebServerRequest* request, uint8_t* data_, size_t len, size_t index, size_t total)
{
    logRequest(request);

    // data is not \0 terminated, I have no other idea how to handle this apart from using malloc
    char* data = static_cast<char*>(malloc(len + 1));
    data[len] = 0;
    memcpy(data, data_, len);

    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, data);
    if (err == DeserializationError::Ok)
    {
        const auto severity_ = doc["severity"].as<int>();

        // Validate
        const auto severity{plog::Severity(severity_)};
        if (isWithin(severity, plog::none, plog::verbose))
        {
            PLOGN << "Setting logging severity to " << severityToString(severity);
            plog::get()->setMaxSeverity(severity);
        }
    }

    free(data);
    request->send(200);
}

////////////////////////////////
/// Config
////////////////////////////////

void WebServer::onDownloadConfig(AsyncWebServerRequest* request)
{
    logRequest(request);

    request->send(LITTLEFS, HAIR_CONFIG_FILE_NAME, "application/json");
}

void WebServer::onUploadConfig(AsyncWebServerRequest* request, uint8_t* data_, size_t len, size_t index, size_t total)
{
    logRequest(request);

    // data is not \0 terminated, I have no other idea how to handle this apart from using malloc
    char* data = static_cast<char*>(malloc(len + 1));
    data[len] = 0;
    memcpy(data, data_, len);

    // TODO(hsbsw) we get the FormData() here, so until I know of a better method I have to manually parse the body
    String str{data};
    str = str.substring(str.indexOf("Content-Type: application/json") + strlen("Content-Type: application/json"));
    str = str.substring(str.indexOf("{"));
    str = str.substring(0, str.indexOf("---------------") - 1);

    hAIR_System::Config tmp{};
    if (hAIR_System::Config::fromJSON(tmp, str.c_str()))
    {
        auto fileWrite = LITTLEFS.open(HAIR_CONFIG_FILE_NAME, "w");
        fileWrite.write(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
        fileWrite.close();

        request->send(200);
    }
    else
    {
        request->send(406); // Not Acceptable
    }

    free(data);
}
