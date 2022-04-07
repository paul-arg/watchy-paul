#ifndef CONFIG_H
#define CONFIG_H

#include "arduino_pins.h"

//display
#define DISPLAY_WIDTH 200
#define DISPLAY_HEIGHT 200
//wifi
#define WIFI_AP_TIMEOUT 60
#define WIFI_AP_SSID "Watchy AP"
//menu
#define WATCHFACE_STATE -1
#define MAIN_MENU_STATE 0
#define APP_STATE 1
#define FW_UPDATE_STATE 2

//additional states
#define WORLD_TIME_STATE 3
#define CHRONOGRAPH_STATE 4
#define TIMER_STATE 5
#define ALARM_STATE 6
#define PET_STATE 7
#define MET_STATE 8
#define ALARM_SET_STATE 9
#define SLEEP_STATE 10
#define TIMER_SET_STATE 11
#define PET_SET_STATE 12
#define WORLD_TIME_SET_STATE 13
#define TIME_SET_STATE 14

#define MENU_HEIGHT 25
#define MENU_LENGTH 7
//set time
#define SET_CITY 0
#define SET_HOUR 1
#define SET_MINUTE 2
#define SET_DST_ON 3
#define SET_DAY 4
#define SET_MONTH 5
#define SET_YEAR 6

#define HOUR_12_24 24

//sleep hours
#define SLEEP_STARTS_HOUR 1
#define SLEEP_STARTS_MINUTE 0
#define SLEEP_ENDS_HOUR 7
#define SLEEP_ENDS_MINUTE 0

//set world time -- modes
#define SET_WORLD_TIME_INDEX 0
#define SET_WORLD_TIME_CITY 1
#define SET_WORLD_TIME_DST_ON 2

//set alarm -- modes
#define SET_ALARM_ON 0
#define SET_ALARM_HOUR 1
#define SET_ALARM_MINUTE 2
#define SET_ALARM_DATE 3 //bool: does the alarm happen at certain date?
#define SET_ALARM_DAY 4 //date of the alarm if previous is true
#define SET_ALARM_MONTH 5
#define SET_ALARM_YEAR 6
#define SET_ALARM_REPEAT 7 //repetition of alarm
#define SET_ALARM_MONDAY 8
#define SET_ALARM_TUESDAY 9
#define SET_ALARM_WEDNESDAY 10
#define SET_ALARM_THURSDAY 11
#define SET_ALARM_FRIDAY 12
#define SET_ALARM_SATURDAY 13
#define SET_ALARM_SUNDAY 14

//set alarm -- values
#define ALARM_REPEAT_DAILY 0
#define ALARM_REPEAT_NONE 1
#define ALARM_REPEAT_WEEKLY 2
#define ALARM_REPEAT_MONTHLY 3
#define ALARM_REPEAT_YEARLY 4

//set timer -- modes
#define SET_TIMER_ON 0
#define SET_TIMER_DAYS 1
#define SET_TIMER_HOURS 2
#define SET_TIMER_MINUTES 3
#define SET_TIMER_WILL_REPEAT 4

//set PET -- modes
#define SET_PET_ON 0
#define SET_PET_WILL_BUZZ 1
#define SET_PET_HOUR 2
#define SET_PET_MINUTE 3
#define SET_PET_DAY 4
#define SET_PET_MONTH 5
#define SET_PET_YEAR 6


//BLE OTA
#define BLE_DEVICE_NAME "Watchy BLE OTA"
#define WATCHFACE_NAME "Watchy 7 Segment"
#define SOFTWARE_VERSION_MAJOR 1
#define SOFTWARE_VERSION_MINOR 0
#define SOFTWARE_VERSION_PATCH 0
#define HARDWARE_VERSION_MAJOR 1
#define HARDWARE_VERSION_MINOR 0
//Versioning
#define WATCHY_LIB_VER "1.4.0"

#endif
