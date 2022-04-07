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
        void showChronograph(bool partialRefresh);
        void showWorldTime(bool partialRefresh);
        void showTimer(bool partialRefresh);
        void showTimerSet(bool partialRefresh);
        void showAlarm(bool partialRefresh);
        void showAlarmSet(bool partialRefresh);
        void drawAlarmSet();
        void showPET(bool partialRefresh);
        void showPETSet(bool partialRefresh);
        void showMET(bool partialRefresh);
        void drawSleep();
        void showSleep(bool partialRefresh);
        void checkForAlarms();
        void awakeLogic();

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
