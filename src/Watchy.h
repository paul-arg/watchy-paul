#ifndef WATCHY_H
#define WATCHY_H

#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <Wire.h>
#include <ezButton.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "Fonts/FiraCode_Bold9pt7b.h"
#include "Fonts/Bizcat_7b.h"
#include "Fonts/Bizcat_24pt7b.h"
#include "Fonts/Bizcat_32pt7b.h"
#include "Fonts/Ubuntu_Bold10pt7b.h"
#include "DSEG7_Classic_Bold_53.h"
#include "WatchyRTC.h"
#include "BLE.h"
#include "bma.h"
#include "config.h"
#include "icons.h"
#include "images.h"
//#include "secrets.h"
#include "chess_pieces.h"

typedef struct weatherData{
    int8_t temperature;
    int16_t weatherConditionCode;
    bool isMetric;
    String weatherDescription;
}weatherData;

typedef struct watchySettings{
    //Weather Settings
    String cityID;
    String weatherAPIKey;
    String weatherURL;
    String weatherUnit;
    String weatherLang;
    int8_t weatherUpdateInterval;
    //NTP Settings
    String ntpServer;
    int gmtOffset;
    int dstOffset;
}watchySettings;

struct W_City {
    String name;
    int16_t utc_offset; //in minutes
};

RTC_DATA_ATTR const uint8_t number_of_cities = 67;

RTC_DATA_ATTR const W_City cities[67] = {
    {"Lisbon"         ,0},
    {"London"         ,0},
    {"Paris"          ,60},
    {"Prague"         ,60},
    {"Rome"           ,60},
    {"Stockholm"      ,60},
    {"Ankara"         ,120},
    {"Athens"         ,120},
    {"Cairo"          ,120},
    {"Helsinki"       ,120},
    {"Jerusalem"      ,120},
    {"Nicosia"        ,120},
    {"Baghdad"        ,180},
    {"Doha"           ,180},
    {"Jeddah"         ,180},
    {"Moscow"         ,180},
    {"Mamoudzou"      ,180},
    {"Tehran"         ,210},
    {"Saint-Denis"    ,240},
    {"Dubai"          ,240},
    {"Kabul"          ,270},
    {"Karachi"        ,300},
    {"Delhi"          ,330},
    {"Kathmandu"      ,345},
    {"Dhaka"          ,360},
    {"Yangon"         ,390},
    {"Bangkok"        ,420},
    {"Beijing"        ,480},
    {"Hong Kong"      ,480},
    {"Kuala Lumpur"   ,480},
    {"Perth"          ,480},
    {"Singapore"      ,480},
    {"Taipei"         ,480},
    {"Seoul"          ,540},
    {"Tokyo"          ,540},
    {"Adelaide"       ,570},
    {"Guam"           ,600},
    {"Sydney"         ,600},
    {"Noumea"         ,660},
    {"Mata Utu"       ,720},
    {"Wellington"     ,720},
    {"Pago Pago"      ,-660},
    {"Honolulu"       ,-600},
    {"Papeete"        ,-600},
    {"Anchorage"      ,-540},
    {"Los Angeles"    ,-480},
    {"Vancouver"      ,-480},
    {"Denver"         ,-420},
    {"Edmonton"       ,-420},
    {"Chicago"        ,-360},
    {"Mexico"         ,-360},
    {"Winnipeg"       ,-360},
    {"Miami"          ,-300},
    {"New York"       ,-300},
    {"Toronto"        ,-300},
    {"Gustavia"       ,-240},
    {"La Paz"         ,-240},
    {"Caracas"        ,-240},
    {"Basse-Terre"    ,-240},
    {"Fort-de-France" ,-240},
    {"Halifax"        ,-240},
    {"Santiago"       ,-240},
    {"St Johns"       ,-210},
    {"Cayenne"        ,-180},
    {"Buenos Aires"   ,-180},
    {"Rio de Janeiro" ,-180},
    {"Praia"          ,-60}
};

struct W_WorldTime {
    uint8_t city_index;
    bool dst_on;
};

struct W_Alarm {
    bool isOn;
    uint8_t hour;
    uint8_t minute;
    bool hasDate;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t repeatType;
    bool monday;
    bool tuesday;
    bool wednesday;
    bool thursday;
    bool friday;
    bool saturday;
    bool sunday;
};

struct W_Chronograph {
    bool isRunning;
    uint16_t days;
    uint8_t hours;
    uint8_t minutes;
};

struct W_Timer {
    bool isRunning;
    uint16_t original_days;
    uint8_t original_hours;
    uint8_t original_minutes;
    uint16_t days;
    uint8_t hours;
    uint8_t minutes;
    bool willRepeat;
    uint16_t repetition_count;
};

struct W_PET {
    bool isOn;
    bool willBuzz;
    uint8_t hour;
    uint8_t minute;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

struct Alarm_Pattern {
    uint16_t pattern[20]; //pattern of the alarm, with alternating lenghts of buzz and silence (buzz, silence, buzz, silence...)
    uint8_t repetitions;
};

const Alarm_Pattern pattern_1s_10t = {
    {1000, 1000},
    10
};

const Alarm_Pattern pattern_300ms_150ms_5t = {
    {300, 150},
    5
};

// from 0 to 255
#define SOFT_VIBE 63
#define MEDIUM_VIBE 127
#define HIGH_VIBE 191
#define MAX_VIBE 255

class Watchy {
    public:
        static WatchyRTC RTC;
        static GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display;
        tmElements_t currentTime;
        watchySettings settings;
    public:
        explicit Watchy(const watchySettings& s) : settings(s){} //constructor
        void init(String datetime = "");
        void deepSleep();
        static void displayBusyCallback(const void*);
        float getBatteryVoltage();
        void vibMotor(uint8_t intervalMs = 100, uint8_t length = 20);

        void handleButtonPress();
        void showMenu(byte menuIndex, bool partialRefresh);
        void showFastMenu(byte menuIndex);
        void showAbout();
        void showBuzz(String message);
        void showAccelerometer();
        void showUpdateFW();
        void showSyncNTP();
        bool syncNTP();
        bool syncNTP(long gmt, int dst, String ntpServer);
        void setTime();
        void setupWifi();
        bool connectWiFi();
        weatherData getWeatherData();
        weatherData getWeatherData(String cityID, String units, String lang, String url, String apiKey, uint8_t updateInterval);
        void updateFWBegin();

        void showState(int guiState, bool partialRefresh);
        void showWatchFace(bool partialRefresh);
        void showTimeSet(bool partialRefresh);
        void showChronograph(bool partialRefresh);
        void showWorldTime(bool partialRefresh);
        void showWorldTimeSet(bool partialRefresh);
        void showTimer(bool partialRefresh);
        void showTimerSet(bool partialRefresh);
        void showAlarm(bool partialRefresh);
        void showAlarmSet(bool partialRefresh);
        void drawAlarmSet();
        void showPET(bool partialRefresh);
        void showPETSet(bool partialRefresh);
        void showMET(bool partialRefresh);
        void showVibeIntensitySet(bool partialRefresh);
        void drawSleep();
        void showSleep(bool partialRefresh);
        void showChess(bool partialRefresh);
        void drawFEN(String fen, bool partialRefresh);
        void checkForAlarms();
        void checkForPETs();
        void awakeLogic();
        void detectDrift();
        void buzz(Alarm_Pattern const *alarm_pattern, String message, uint8_t vibeIntensity);
        void drawModeIndicator(uint8_t mode);

        void drawPiece(uint8_t piece, uint8_t file, uint8_t rank, bool orientation);

        void menuButton();
        void backButton();
        void upButton();
        void downButton();

        void drawCenteredString(const String &str, int x, int y, bool drawBg);
        uint8_t getWeekday(uint8_t day, uint8_t month, uint16_t year);
        void addMinuteToChronograph(W_Chronograph *chronograph);
        void resetChronograph(W_Chronograph *chronograph);
        void decrementTimer(W_Timer *timer);
        void setPETtoNow(W_PET *PET);
        void tick();
        time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss);
        uint16_t secondsToDays(int32_t diff);
        uint8_t secondsToHours(int32_t diff);
        uint8_t secondsToMinutes(int32_t diff);

        virtual void drawWatchFace(); //override this method for different watch faces

        int getLastDay(int month, int year);
    private:
        void _bmaConfig();
        static void _configModeCallback(WiFiManager *myWiFiManager);
        static uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len);
        static uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len);
};

extern RTC_DATA_ATTR int guiState;
extern RTC_DATA_ATTR int menuIndex;
extern RTC_DATA_ATTR BMA423 sensor;
extern RTC_DATA_ATTR bool WIFI_CONFIGURED;
extern RTC_DATA_ATTR bool BLE_CONFIGURED;

#endif
