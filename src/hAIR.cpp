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

#include "hAIR.h"
#include "Utilities.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <AsyncElegantOTA.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <LITTLEFS.h>
#include <Preferences.h>
#include <SPI.h>
#include <WiFi.h>
#include <iostream>
#include <plog/Init.h>
#include <plog/Log.h>

// IDK, 100hz maybe?
constexpr auto THREAD_FREQUENCY{100};
constexpr auto THREAD_DELAYTIME{static_cast<int32_t>(1000.0F / THREAD_FREQUENCY)};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Main
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void hAIR_System::setup()
{
    // Initialization and POST is interleveaved
    // We start printing/displaying the POST after loading the config file, since we only then know which baudrate to use.
    // Printing and then reprinting would be a lot of clutter.

    ////////////////////////////////
    /// Base Layer
    ////////////////////////////////

    // We see the Serial and TFT display as given, so we init it first
    initDisplay();

    // Since we want to load out config file to init everything, we have to init the filesystem next
    constexpr auto FORMAT_FILESYSTEM_IF_INIT_FAILED{true};
    initFilesystem(FORMAT_FILESYSTEM_IF_INIT_FAILED);

    // Load config file
    constexpr auto CONFIG_CREATE_IF_LOAD_FAILED{true};
    loadConfigFromFileOrDefault(CONFIG_CREATE_IF_LOAD_FAILED);

    // Serial with baudrate (config is either defaulted or loaded)
    Serial.begin(config.serial_baudrate);

    // Start printing/displaying POST
    printAndDisplayPOSTline("hAIR", HAIR_VERSION_STRING, false); // print who or what we are
    printAndDisplayPOSTline("Display", post.display ? "OK" : "Failed", !post.display);
    printAndDisplayPOSTline("Filesystem", post.filesystem ? "Mounted" : "Created", !post.filesystem);
    printAndDisplayPOSTline("Config", post.config ? "File" : "Default", !post.config);

    // WiFi
    printAndDisplayPOSTcomponent("WiFi");
    initWiFi(post.config);
    printAndDisplayPOSTresult(post.wifi ? WiFi.localIP().toString() : "Failed", !post.wifi);

    // MDNS
    initMDNS();
    String mdnsPOST{HAIR_WIFI_HOSTNAME};
    mdnsPOST += ".local";
    printAndDisplayPOSTline("MDNS", post.mdns ? mdnsPOST : "----", !(post.mdns && post.wifi));

    // OTA
    initOTA();
    printAndDisplayPOSTline("OTA", post.ota ? "Running" : "Failed", !post.ota);

    // NTP
    printAndDisplayPOSTcomponent("NTP");
    if (post.wifi)
    {
        initNTP();
    }
    printAndDisplayPOSTresult(post.ntpclient ? getFormattedDate(components.ntpclient.getEpochTime()) : "----", !post.ntpclient);

    // WebServer
    initAsyncWebserver();
    printAndDisplayPOSTline("AsyncWebserver", post.asyncWebserver ? "Running" : "Failed", !post.asyncWebserver);

    // Logger
    initLogger();
    printAndDisplayPOSTline("Logger", post.logger ? "OK" : "Failed", !post.logger);
    printAndDisplayPOSTline("Severity", severityToString(plog::Severity(config.logger_severity)), !post.logger);

    ////////////////////////////////
    /// Application Layer
    ////////////////////////////////

    post.webserver = components.webserver.init();
    printAndDisplayPOSTline("Webserver", post.webserver ? "Running" : "Failed", !post.webserver);

    components.websocketSensorData.begin();
    components.websocketLogMessages.begin();

    initSGP();
    printAndDisplayPOSTline("SGP30", post.sgp30 ? "OK" : "Failed", !post.sgp30);
    printAndDisplayPOSTline("SGP30_BL", post.sgp30_baseline ? "Set from Prefs" : "Failed", !post.sgp30_baseline);

    // Initialization done, show the POST for a little while
    //delay(10000);
    delay(100);

    // Blank screen
    components.tft.fillScreen(TFT_BLACK);

    ////////////////////////////////
    /// Config
    ////////////////////////////////

    PLOGI << "Config: " << Config::toJSON(config).c_str();

    // Now show the config for a little while
    //delay(10000);
    delay(100);

    // Blank screen
    components.tft.fillScreen(TFT_BLACK);

    ////////////////////////////////
    /// Start Threads
    ////////////////////////////////

    constexpr auto THREAD_STACK_SIZE{16 * 1024};
    constexpr auto THREAD_PRIORITY{0};
    constexpr auto THREAD_SDA_CORE{0};
    constexpr auto THREAD_SDD_CORE{1};
    constexpr auto THREAD_SDA_NAME{"sda"};
    constexpr auto THREAD_SDD_NAME{"sdd"};

    auto threadSkeleton = [](void* param)
    {
        auto* p = static_cast<hAIR_System::TaskParams*>(param);

        while (true)
        {
            const auto now{millis()};

            p->callThreadFunction(now);

            delay(THREAD_DELAYTIME);
        }
    };

    runtime.task_sda_sqp_IAQ.setFrequency(config.sgp_IAQ_frequency);
    runtime.task_sda_sqp_IAQraw.setFrequency(config.sgp_IAQraw_frequency);
    runtime.task_sda_sqp_baseline.setFrequency(60000); // Adafruit example is 60 seconds
    runtime.task_sda_bme_measure.setFrequency(config.bme_measure_frequency);
    xTaskCreatePinnedToCore(threadSkeleton,
                            THREAD_SDA_NAME,
                            THREAD_STACK_SIZE,
                            &threadParams_sensorDataAcquisition,
                            THREAD_PRIORITY,
                            &thread_sensorDataAcquisition,
                            THREAD_SDA_CORE);

    runtime.task_sdd_serial.setFrequency(config.sdd_serial_frequency);
    runtime.task_sdd_display.setFrequency(config.sdd_display_frequency);
    runtime.task_sdd_websocket.setFrequency(config.sdd_websocket_frequency);
    xTaskCreatePinnedToCore(threadSkeleton,
                            THREAD_SDD_NAME,
                            THREAD_STACK_SIZE,
                            &threadParams_sensorDataDistribution,
                            THREAD_PRIORITY,
                            &thread_sensorDataDistribution,
                            THREAD_SDD_CORE);
}

void hAIR_System::loop()
{
    delay(THREAD_DELAYTIME);

    threadFunction_loop(millis());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Thread Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void hAIR_System::threadFunction_sensorDataAcquisition(Timestamp now)
{
    // We need to preserve old variables, since the SGP methods fail kind of often :(
    auto tmp{sensorData.getCopy()};

    ////////////////////////////////
    /// SGP30 Data Acquisition
    ////////////////////////////////

    if (runtime.task_sda_sqp_IAQ.shallRun(now))
    {
        /// return absolute humidity [mg/m^3] with approximation formula
        /// If you have a temperature / humidity sensor, you can set the absolute humidity to enable the humditiy compensation for the air quality signals
        /// @param temperature [°C]
        /// @param humidity [%RH]
        auto getAbsoluteHumidity = [](float temperature, float humidity) -> uint32_t
        {
            // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
            const float    absoluteHumidity       = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
            const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity);                                                                         // [mg/m^3]
            return absoluteHumidityScaled;
        };

        // Compensate humidity prior to measuring
        components.sgp.setHumidity(getAbsoluteHumidity(tmp.bme_data.temperature, tmp.bme_data.humidity));

        if (components.sgp.IAQmeasure())
        {
            runtime.task_sda_sqp_IAQ.updateSuccess(now);

            tmp.sgp_iaq.TVOC = components.sgp.TVOC;
            tmp.sgp_iaq.eCO2 = components.sgp.eCO2;

            tmp.sgp_iaq.isValid = true;
        }
        else
        {
            // only print a warning if sensor has not been read within the last minute, since it produces a lot of errors
            if (dt_max(now, runtime.task_sda_sqp_IAQ.getLastSuccess(), 60000))
            {
                PLOGW << "SGP30 IAQ data acquisition failed\n";

                tmp.sgp_iaq.isValid = false;
            }
        }
    }

    if (runtime.task_sda_sqp_IAQraw.shallRun(now))
    {
        if (components.sgp.IAQmeasureRaw())
        {
            runtime.task_sda_sqp_IAQraw.updateSuccess(now);

            tmp.sgp_iaqRaw.rawH2      = components.sgp.rawH2;
            tmp.sgp_iaqRaw.rawEthanol = components.sgp.rawEthanol;

            tmp.sgp_iaqRaw.isValid = true;
        }
        else
        {
            // only print a warning if sensor has not been read within the last minute, since it produces a lot of errors
            if (dt_max(now, runtime.task_sda_sqp_IAQraw.getLastSuccess(), 60000))
            {
                PLOGW << "SGP30 raw IAQ data acquisition failed\n";

                tmp.sgp_iaqRaw.isValid = true;
            }
        }
    }

    ////////////////////////////////
    /// SGP30 Baseline
    ////////////////////////////////

    if (runtime.task_sda_sqp_baseline.shallRun(now))
    {
        // https://learn.adafruit.com/adafruit-sgp30-gas-tvoc-eco2-mox-sensor/arduino-code
        // To make that easy, SGP lets you query the 'baseline calibration readings' from the sensor with code like this:
        // uint16_t TVOC_base, eCO2_base;
        // sgp.getIAQBaseline(&eCO2_base, &TVOC_base);
        // This will grab the two 16-bit sensor calibration words and place them in the variables so-named.
        // You should store these in EEPROM, FLASH or hard-coded. Then, next time you start up the sensor, you can pre-fill the calibration words with sgp.setIAQBaseline(eCO2_baseline, TVOC_baseline);

        uint16_t TVOC_baseline{};
        uint16_t eCO2_baseline{};
        if (components.sgp.getIAQBaseline(&eCO2_baseline, &TVOC_baseline))
        {
            // https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
            Preferences preferences;
            preferences.begin("SGP30", false);
            preferences.putUShort("TVOC_baseline", TVOC_baseline);
            preferences.putUShort("eCO2_baseline", eCO2_baseline);
            preferences.end();
        }
    }

    ////////////////////////////////
    /// BMExxx
    ////////////////////////////////

    if (runtime.task_sda_bme_measure.shallRun(now))
    {
        runtime.task_sda_bme_measure.updateSuccess(now);

        // let every value wander by 0.05 per measurement
        tmp.bme_data.temperature += ((rand() % 1000) - 500) / 10000.0f;
        tmp.bme_data.humidity += ((rand() % 1000) - 500) / 10000.0f;
        tmp.bme_data.pressure += ((rand() % 1000) - 500) / 10000.0f;

        tmp.bme_data.isValid = true;
    }

    ////////////////////////////////
    /// Data Storage
    ////////////////////////////////

    sensorData.update(tmp);
}

void hAIR_System::threadFunction_sensorDataDistribution(Timestamp now)
{
    ////////////////////////////////
    /// Sensor Data
    ////////////////////////////////

    const auto data    = sensorData.getCopy();
    auto       jsonStr = data.toJSONtxt(); // broadcastTXT does not accecpt const

    ////////////////////////////////
    /// Serial
    ////////////////////////////////

    if (runtime.task_sdd_serial.shallRun(now))
    {
        Serial.println(jsonStr);
    }

    ////////////////////////////////
    /// Display
    ////////////////////////////////

    if (runtime.task_sdd_display.shallRun(now))
    {
        components.display.printSensorData(data);
    }

    ////////////////////////////////
    /// WebSocket
    ////////////////////////////////

    if (runtime.task_sdd_websocket.shallRun(now))
    {
        if (data.isValid())
        {
            components.websocketSensorData.broadcastTXT(jsonStr);
        }
    }
}

void hAIR_System::threadFunction_loop(Timestamp now)
{
    ////////////////////////////////
    /// Base Layer
    ////////////////////////////////

    AsyncElegantOTA.loop();
    ArduinoOTA.handle();
    components.ntpclient.update();

    if (runtime.task_system_restartBecauseWiFiFailed.shallRun())
    {
        // Restart hAIR if WiFi connection has not been established
        PLOGN << "Restarting hAIR...";
        delay(1000);
        ESP.restart();
    }

    ////////////////////////////////
    /// Application Layer
    ////////////////////////////////

    components.websocketSensorData.loop();
    components.websocketLogMessages.loop();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Init
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void hAIR_System::initDisplay()
{
    components.tft.begin();
    components.tft.fillScreen(TFT_BLACK);
    components.tft.setRotation(0); // 0 => Portrait; 1 => Landscape
    //components.tft.setTextColor(TFT_GREEN, TFT_BLACK);
    //components.tft.setFreeFont(&FreeMono9pt7b);
    components.tft.setTextFont(1);
    components.tft.setTextSize(1);
    components.tft.setTextWrap(false, false);
    components.tft.setCursor(0, 0);

    components.tft.setTextColor(TFT_WHITE, TFT_BLACK);

    post.display = true; // Is there a way to detect whether the display was initialized correctly?
}

void hAIR_System::initFilesystem(bool formatIfFailed)
{
    post.filesystem = LITTLEFS.begin(formatIfFailed);
}

void hAIR_System::loadConfigFromFileOrDefault(bool saveIfLoadFailed)
{
    config = {};

    auto fileRead = LITTLEFS.open(HAIR_CONFIG_FILE_NAME);
    if (fileRead)
    {
        const auto jsonStr = fileRead.readString();
        fileRead.close();

        Config tmp{};
        if (Config::fromJSON(tmp, jsonStr))
        {
            config = tmp;

            post.config = true;
            return;
        }
    }

    if (saveIfLoadFailed)
    {
        std::array<char, 512> jsonStr{};

        const auto size = Config::toJSON<jsonStr.size()>(config, jsonStr.data());

        auto fileWrite = LITTLEFS.open(HAIR_CONFIG_FILE_NAME, "w");
        fileWrite.write(reinterpret_cast<uint8_t*>(jsonStr.data()), size);
        fileWrite.close();
    }

    post.config = false;
}

void hAIR_System::initWiFi(bool configWasLoaded)
{
    WiFi.mode(WIFI_STA);

    // If the config was loaded and the file stated to use preferences, we try to load the preferences,
    // If the config was not loaded, attempt to load preferences anyway, since we have no data in the default values of the config
    // Else, the config was loaded but ssid/password are custom, then use these values
    if ((configWasLoaded && ((config.wifi_ssid == String{"USE_PREFERENCES"}) || (config.wifi_password == String{"USE_PREFERENCES"}))) ||
        !configWasLoaded)
    {
        Preferences preferences;
        preferences.begin("wifi", true);
        const auto ssid     = preferences.getString("ssid");
        const auto password = preferences.getString("password");
        preferences.end();

        WiFi.begin(ssid.c_str(), password.c_str());
    }
    else
    {
        WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
    }

    WiFi.setHostname(HAIR_WIFI_HOSTNAME);

    constexpr auto WIFI_RETRIES{15};
    for (auto tries = 0; tries < WIFI_RETRIES; ++tries)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            post.wifi = true;
            return;
        }
        printAndDisplayPOSTprogress();
        delay(500);
    }

    constexpr auto WIFI_RESTART_TIMEOUT_MS{5 * 60000}; // Set to 5 minutes, use 0 for no restart
    runtime.task_system_restartBecauseWiFiFailed.setDelayTime(WIFI_RESTART_TIMEOUT_MS);
    runtime.task_system_restartBecauseWiFiFailed.updateTry(millis());

    post.wifi = false;
}

void hAIR_System::initMDNS()
{
    post.mdns = MDNS.begin(HAIR_WIFI_HOSTNAME);
}

void hAIR_System::initOTA()
{
    AsyncElegantOTA.begin(&components.asyncWebserver);
    ArduinoOTA.setHostname(HAIR_WIFI_HOSTNAME);
    ArduinoOTA.begin();

    post.ota = true; // Is there a way to detect whether it was initialized correctly?
}

void hAIR_System::initNTP()
{
    components.ntpclient.begin();
    components.ntpclient.setTimeOffset(7200); // UTC + 2

    int32_t cnt{};
    while (!components.ntpclient.update())
    {
        printAndDisplayPOSTprogress();
        components.ntpclient.forceUpdate();

        // Something like a timeout
        if (cnt++ > 10)
        {
            post.ntpclient = false;
            return;
        }
    }

    post.ntpclient = true;
}

void hAIR_System::initAsyncWebserver()
{
    // we cannot use a port from the config file, since it would require reassigning the component member variable,
    // but that somehow led to infinite resets
    //components.webserver = AsyncWebServer(config.webserver_port);
    components.asyncWebserver.begin();
    post.asyncWebserver = true;
}

void hAIR_System::initLogger()
{
    plog::init(plog::Severity(config.logger_severity), &components.appender);

    post.logger = true;
}

void hAIR_System::initSGP()
{
    post.sgp30 = components.sgp.begin();

    if (post.sgp30)
    {
        // See Baseline acquisition in SDA
        Preferences preferences;
        preferences.begin("SGP30", true);
        uint16_t TVOC_baseline = preferences.getUShort("TVOC_baseline", 0U);
        uint16_t eCO2_baseline = preferences.getUShort("eCO2_baseline", 0U);
        preferences.end();

        PLOGN << "TV " << TVOC_baseline << " CO " << eCO2_baseline;

        // https://learn.adafruit.com/adafruit-sgp30-gas-tvoc-eco2-mox-sensor/arduino-code, should be around 400 if it was ever initialized
        if (eCO2_baseline != 0)
        {
            post.sgp30_baseline = components.sgp.setIAQBaseline(eCO2_baseline, TVOC_baseline);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Post
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void hAIR_System::printAndDisplayPOSTcomponent(const char* componentName)
{
    Serial.print("[POST] ");
    Serial.print(componentName);
    Serial.print(": ");

    components.tft.print(componentName);
    components.tft.print(": "); // wrap after component
}

void hAIR_System::printAndDisplayPOSTresult(const String& result, bool doDisplayInRed)
{
    Serial.println(result);

    components.tft.println(""); // wrap after component
    components.tft.print(" ");  // automatical indent
    if (doDisplayInRed)
    {
        components.tft.setTextColor(TFT_RED, TFT_BLACK);
    }
    components.tft.println(result); // wrap after line

    components.tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void hAIR_System::printAndDisplayPOSTline(const char* componentName, const String& result, bool doDisplayInRed)
{
    printAndDisplayPOSTcomponent(componentName);
    printAndDisplayPOSTresult(result, doDisplayInRed);
}

void hAIR_System::printAndDisplayPOSTprogress()
{
    Serial.print(".");
    components.tft.print(".");
}
