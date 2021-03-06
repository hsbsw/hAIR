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
#include "Logger.h"
#include "SensorData.h"
#include "Utilities.h"
#include "WebServer.h"
#include <Adafruit_SGP30.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <TFT_eSPI.h>
#include <WebSocketsServer.h>
#include <WiFiUdp.h>
#include <mutex>

constexpr auto HAIR_VERSION_STRING{"0.1"};
constexpr auto HAIR_WIFI_HOSTNAME{"hAIR"};
constexpr auto HAIR_CONFIG_FILE_NAME{"/hAIR_config.json"};

class hAIR_System
{
public:
    struct Config
    {
        ////////////////////////////////
        /// Base Layer
        ////////////////////////////////

        String wifi_ssid{""};
        String wifi_password{""};

        int32_t serial_baudrate{115200};

        int32_t logger_severity{plog::debug}; // see https://github.com/SergiusTheBest/plog/blob/master/include/plog/Severity.h

        ////////////////////////////////
        /// Application Layer
        ////////////////////////////////

        // 1hz is recommended?! in table 2
        float sgp_IAQ_frequency{1};
        float sgp_IAQraw_frequency{10};

        float bme_measure_frequency{10};

        float sdd_serial_frequency{0};
        float sdd_display_frequency{5};
        float sdd_websocket_frequency{1};

        ////////////////////////////////
        /// JSON
        ////////////////////////////////

        static bool fromJSON(Config& config, const String& jsonStr)
        {
            // https: //arduinojson.org/v6/doc/deserialization/
            StaticJsonDocument<512> doc;

            DeserializationError err = deserializeJson(doc, jsonStr);
            if (err == DeserializationError::Ok)
            {
                Config tmp{};

                config.wifi_ssid               = doc["wifi"]["ssid"].as<String>();
                config.wifi_password           = doc["wifi"]["password"].as<String>();
                config.serial_baudrate         = doc["serial"]["baudrate"];
                config.logger_severity         = doc["logger"]["severity"];
                config.sgp_IAQ_frequency       = doc["sgp30"]["iaqFrequency"];
                config.sgp_IAQraw_frequency    = doc["sgp30"]["iaqRawFrequency"];
                config.bme_measure_frequency   = doc["bmexxx"]["dataFrequency"];
                config.sdd_serial_frequency    = doc["sdd"]["serial_frequency"];
                config.sdd_display_frequency   = doc["sdd"]["display_frequency"];
                config.sdd_websocket_frequency = doc["sdd"]["websocket_frequency"];

                if (validate(config))
                {
                    return true;
                }
            }

            return false;
        }

        template<int N = 512>
        static size_t toJSON(Config config, char* jsonStr)
        {
            // https://arduinojson.org/v6/doc/serialization/
            StaticJsonDocument<N> doc;

            doc["wifi"]["ssid"]               = config.wifi_ssid;
            doc["wifi"]["password"]           = config.wifi_password;
            doc["serial"]["baudrate"]         = config.serial_baudrate;
            doc["logger"]["severity"]         = config.logger_severity;
            doc["sgp30"]["iaqFrequency"]      = config.sgp_IAQ_frequency;
            doc["sgp30"]["iaqRawFrequency"]   = config.sgp_IAQraw_frequency;
            doc["bmexxx"]["dataFrequency"]    = config.bme_measure_frequency;
            doc["sdd"]["serial_frequency"]    = config.sdd_serial_frequency;
            doc["sdd"]["display_frequency"]   = config.sdd_display_frequency;
            doc["sdd"]["websocket_frequency"] = config.sdd_websocket_frequency;

            char tmp[N]{}; // stupid type forwarding...
            auto size = serializeJsonPretty(doc, tmp);
            memcpy(jsonStr, tmp, N);
            return size;
        }

        template<int N = 512>
        static String toJSON(Config config)
        {
            std::array<char, N> jsonStr{};
            toJSON<N>(config, jsonStr.data());
            return String{jsonStr.data()};
        }

        ////////////////////////////////
        /// Validation Layer
        ////////////////////////////////

        static inline bool validate(const Config& config)
        {
            constexpr auto FREQ_MIN{0.0F};
            constexpr auto FREQ_MAX{1000.0F};
            return isWithin(config.serial_baudrate, 9600, 10000000) &&
                   isWithin(plog::Severity(config.logger_severity), plog::none, plog::verbose) &&
                   isWithin(config.sgp_IAQ_frequency, FREQ_MIN, FREQ_MAX) &&
                   isWithin(config.sgp_IAQraw_frequency, FREQ_MIN, FREQ_MAX) &&
                   isWithin(config.bme_measure_frequency, FREQ_MIN, FREQ_MAX) &&
                   isWithin(config.sdd_serial_frequency, FREQ_MIN, FREQ_MAX) &&
                   isWithin(config.sdd_display_frequency, FREQ_MIN, FREQ_MAX) &&
                   isWithin(config.sdd_websocket_frequency, FREQ_MIN, FREQ_MAX);
        }
    };

    struct Components
    {
        Components(SensorDataStorage& sensorData)
            : webserver{sensorData, asyncWebserver}
        {
        }

        ////////////////////////////////
        // Base Layer
        ////////////////////////////////

        TFT_eSPI       tft{};
        AsyncWebServer asyncWebserver{80};
        WiFiUDP        ntpUDP{};
        NTPClient      ntpclient{ntpUDP, "ptbtime1.ptb.de"};

        ////////////////////////////////
        // Application Layer
        ////////////////////////////////

        hAIR_Formatter formatter{ntpclient};
        hAIR_Appender  appender{formatter, display, websocketLogMessages};

        WebServer        webserver;
        WebSocketsServer websocketSensorData{81};
        WebSocketsServer websocketLogMessages{82};

        Display display{tft};

        // https://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/9_Gas_Sensors/Datasheets/Sensirion_Gas_Sensors_Datasheet_SGP30.pdf
        Adafruit_SGP30 sgp{};
    };

    struct Runtime
    {
        ////////////////////////////////
        /// Base Layer
        ////////////////////////////////

        TaskItem task_system_restartBecauseWiFiFailed{};

        ////////////////////////////////
        /// Application Layer
        ////////////////////////////////

        // Sensor Data Acquisition - SGP30
        TaskItem task_sda_sqp_IAQ{};
        TaskItem task_sda_sqp_IAQraw{};
        TaskItem task_sda_sqp_baseline{};

        // Sensor Data Acquisition - BMExxx
        TaskItem task_sda_bme_measure{};

        // Sensor Data Distribution
        TaskItem task_sdd_serial{};
        TaskItem task_sdd_display{};
        TaskItem task_sdd_websocket{};
    };

    struct POST
    {
        bool display;        /// true => success;   false => failed
        bool filesystem;     /// true => mounted?;  false => created
        bool config;         /// true => loaded;    false => default
        bool wifi;           /// true => connected; false => failed
        bool mdns;           /// true => success;   false => failed
        bool ota;            /// true => success;   false => failed
        bool ntpclient;      /// true => connected; false => failed
        bool asyncWebserver; /// true => started;   false => failed
        bool logger;         /// true => success;   false => failed

        bool webserver;      /// true => success;   false => failed
        bool sgp30;          /// true => success;   false => failed
        bool sgp30_baseline; /// true => prefs set; false => failed
    };

    void setup();
    void loop();

private:
    ////////////////////////////////
    /// Member variables
    ////////////////////////////////

    Config            config{};
    Components        components{sensorData};
    Runtime           runtime{};
    SensorDataStorage sensorData{};
    POST              post{};

    ////////////////////////////////
    /// Thread Functions
    ////////////////////////////////

    void threadFunction_sensorDataAcquisition(Timestamp now);
    void threadFunction_sensorDataDistribution(Timestamp now);
    void threadFunction_loop(Timestamp now);

    // We need to use these task params because unlike std::thread, xTaskCreatePinnedToCore won't take a capturing lambda. So 'this' pointer has to live somewhere 'static'
    struct TaskParams
    {
        using fp_t = void (hAIR_System::*)(Timestamp);

        TaskParams(hAIR_System& instance, fp_t fp)
            : m_instance(instance), m_fp(fp)
        {
        }

        hAIR_System& m_instance;
        fp_t         m_fp;

        void callThreadFunction(Timestamp now) const
        {
            (m_instance.*m_fp)(now);
        }
    };

    TaskHandle_t thread_sensorDataAcquisition{};
    TaskHandle_t thread_sensorDataDistribution{};
    TaskParams   threadParams_sensorDataAcquisition{*this, &hAIR_System::threadFunction_sensorDataAcquisition};
    TaskParams   threadParams_sensorDataDistribution{*this, &hAIR_System::threadFunction_sensorDataDistribution};

    ////////////////////////////////
    /// Init
    ////////////////////////////////

    // Base
    void initDisplay();
    void initFilesystem(bool formatIfFailed);
    void loadConfigFromFileOrDefault(bool saveIfLoadFailed);
    void initWiFi(bool configWasLoaded);
    void initMDNS();
    void initOTA();
    void initNTP();
    void initAsyncWebserver();
    void initLogger();

    // Application
    void initSGP();

    ////////////////////////////////
    // Post
    ////////////////////////////////
    void printAndDisplayPOSTcomponent(const char* componentName);
    void printAndDisplayPOSTresult(const String& result, bool doDisplayInRed);
    void printAndDisplayPOSTline(const char* componentName, const String& result, bool doDisplayInRed);
    void printAndDisplayPOSTprogress();
};
