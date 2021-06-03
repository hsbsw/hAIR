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

#include "Utilities.h"

void listDir(fs::FS& fs, const char* dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\r\n", dirname);

    fs::File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("- failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println(" - not a directory");
        return;
    }

    fs::File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("  DIR : ");

#ifdef CONFIG_LITTLEFS_FOR_IDF_3_2
            Serial.println(file.name());
#else
            Serial.print(file.name());
            time_t     t        = file.getLastWrite();
            struct tm* tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
#endif

            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");

#ifdef CONFIG_LITTLEFS_FOR_IDF_3_2
            Serial.println(file.size());
#else
            Serial.print(file.size());
            time_t     t        = file.getLastWrite();
            struct tm* tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
#endif
        }
        file = root.openNextFile();
    }
}

#ifndef LEAP_YEAR
#define LEAP_YEAR(Y) ((Y > 0) && !(Y % 4) && ((Y % 100) || !(Y % 400)))
#endif

// https://github.com/arduino-libraries/NTPClient/pull/94
String getFormattedTime(unsigned long secs)
{
    unsigned long rawTime  = secs;
    unsigned long hours    = (rawTime % 86400L) / 3600;
    String        hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

    unsigned long minutes   = (rawTime % 3600) / 60;
    String        minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

    unsigned long seconds   = rawTime % 60;
    String        secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

    return hoursStr + ":" + minuteStr + ":" + secondStr;
}

// https://github.com/arduino-libraries/NTPClient/pull/94
// Based on https://github.com/PaulStoffregen/Time/blob/master/Time.cpp
// currently assumes UTC timezone, instead of using this->_timeOffset
String getFormattedDate(unsigned long secs)
{
    unsigned long        rawTime = secs / 86400L; // in days
    unsigned long        days = 0, year = 1970;
    uint8_t              month;
    static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    while ((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime)
        year++;
    rawTime -= days - (LEAP_YEAR(year) ? 366 : 365); // now it is days in this year, starting at 0
    days = 0;
    for (month = 0; month < 12; month++)
    {
        uint8_t monthLength;
        if (month == 1)
        { // february
            monthLength = LEAP_YEAR(year) ? 29 : 28;
        }
        else
        {
            monthLength = monthDays[month];
        }
        if (rawTime < monthLength)
            break;
        rawTime -= monthLength;
    }
    String monthStr = ++month < 10 ? "0" + String(month) : String(month);       // jan is month 1
    String dayStr   = ++rawTime < 10 ? "0" + String(rawTime) : String(rawTime); // day of month
    return String(year) + "-" + monthStr + "-" + dayStr + "T" + getFormattedTime(secs ? secs : 0) + "Z";
}
