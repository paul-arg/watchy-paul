#include "Watchy.h"
#include <ezButton.h>

WatchyRTC Watchy::RTC;
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> Watchy::display(GxEPD2_154_D67(DISPLAY_CS, DISPLAY_DC, DISPLAY_RES, DISPLAY_BUSY));

RTC_DATA_ATTR int guiState;
RTC_DATA_ATTR int menuIndex;
RTC_DATA_ATTR uint8_t alarmIndex = 0; //alarm index to show
RTC_DATA_ATTR uint8_t timerIndex = 0; //timer index to show
RTC_DATA_ATTR uint8_t PETIndex = 0; //PET index to show
RTC_DATA_ATTR BMA423 sensor;
RTC_DATA_ATTR bool WIFI_CONFIGURED;
RTC_DATA_ATTR bool BLE_CONFIGURED;
RTC_DATA_ATTR weatherData currentWeather;
RTC_DATA_ATTR int weatherIntervalCounter = -1;
RTC_DATA_ATTR bool displayFullInit = true;

RTC_DATA_ATTR int8_t alarm_set_value_index = SET_ALARM_ON;
RTC_DATA_ATTR int8_t timer_set_value_index = SET_TIMER_ON;
RTC_DATA_ATTR int8_t PET_set_value_index = SET_PET_ON;

RTC_DATA_ATTR ezButton menu_button(MENU_BTN_PIN);
RTC_DATA_ATTR ezButton back_button(BACK_BTN_PIN);
RTC_DATA_ATTR ezButton up_button(UP_BTN_PIN);
RTC_DATA_ATTR ezButton down_button(DOWN_BTN_PIN);

RTC_DATA_ATTR W_Alarm alarms[5] = {
    {false, 10, 0, false, 29, 3, 2022, ALARM_REPEAT_DAILY  , true, true, true, true, true, true, true},
    {false, 11, 0, false, 29, 3, 2022, ALARM_REPEAT_DAILY  , true, true, true, true, true, true, true},
    {false, 12, 0, false, 29, 3, 2022, ALARM_REPEAT_DAILY  , true, true, true, true, true, true, true},
    {false, 13, 0, false, 29, 3, 2022, ALARM_REPEAT_DAILY  , true, true, true, true, true, true, true},
    {true ,  9, 0, true , 21, 3, 2022, ALARM_REPEAT_MONTHLY, true, true, true, true, true, true, true}
};

RTC_DATA_ATTR W_Chronograph chrono = {false, 0, 0, 0};

RTC_DATA_ATTR W_Timer timers[5] = {
    {false, 1,  0,  0, 1,  0,  0, false, 0},
    {false, 0, 10,  0, 0, 10,  0, false, 0},
    {false, 0, 10,  0, 0, 10,  0, false, 0},
    {false, 0,  5,  0, 0,  5,  0, false, 0},
    {false, 0,  0,  5, 0,  0,  5, false, 0}
};

RTC_DATA_ATTR W_PET PETs[3] = {
    {true , false,  0,  0, 21, 5, 2022},
    {false, false, 17, 30,  2, 4, 2022},
    {false, false, 17, 30,  2, 4, 2022}
};

void Watchy::tick(){
    //do logic
    checkForAlarms();
    addMinuteToChronograph(&chrono);
    for (int i = 0; i < 5; i++) {
        decrementTimer(&timers[i]);
    }
    

    //update displays
    if (guiState == WATCHFACE_STATE) {
        RTC.read(currentTime);
        showWatchFace(true);  // partial updates on tick
    } else if (guiState == WORLD_TIME_STATE) {
        showWorldTime(true);
    } else if (guiState == CHRONOGRAPH_STATE) {
        showChronograph(true);
    } else if (guiState == TIMER_STATE) {
        showTimer(true);
    } else if (guiState == PET_STATE) {
        showPET(true);
    }
}   

void Watchy::init(String datetime) {
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();  // get wake up reason
    Wire.begin(SDA, SCL);                          // init i2c
    RTC.init();

    menu_button.setDebounceTime(50);
    back_button.setDebounceTime(50);
    up_button.setDebounceTime(50);
    down_button.setDebounceTime(50);

    // Init the display here for all cases, if unused, it will do nothing
    display.init(0, displayFullInit, 10, true);  // 10ms by spec, and fast pulldown reset
    display.epd2.setBusyCallback(displayBusyCallback);

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:  // RTC Alarm
            Serial.println("RTC interrupt.");
            tick();
            break;
        case ESP_SLEEP_WAKEUP_EXT1:  // button Press
            Serial.println("Button interrupt.");
            handleButtonPress();
            break;
        default:  // reset
            RTC.config(datetime);
            _bmaConfig();
            RTC.read(currentTime);
            showWatchFace(false);  // full update on reset
            break;
    }
    deepSleep();
}

void Watchy::displayBusyCallback(const void *) {
    gpio_wakeup_enable((gpio_num_t)DISPLAY_BUSY, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    esp_light_sleep_start();
}

void Watchy::deepSleep() {
    Serial.println("Deep sleep starts.");
    display.hibernate();
    displayFullInit = false;  // Notify not to init it again
    RTC.clearAlarm();         // resets the alarm flag in the RTC
    //  Set pins 0-39 to input to avoid power leaking out
    for (int i = 0; i < 40; i++) {
        pinMode(i, INPUT);
    }
    esp_sleep_enable_ext0_wakeup((gpio_num_t)RTC_INT_PIN, 0);              // enable deep sleep wake on RTC interrupt
    esp_sleep_enable_ext1_wakeup(BTN_PIN_MASK, ESP_EXT1_WAKEUP_ANY_HIGH);  // enable deep sleep wake on button press
    esp_deep_sleep_start();
}

void Watchy::checkForAlarms() {
    Serial.println("Checking for alarms.");
    RTC.read(currentTime);
    Serial.printf(
        "now is %02d:%02d on %02d/%02d/%04d\n",
        currentTime.Hour, currentTime.Minute, currentTime.Day, currentTime.Month, 1970 + currentTime.Year);

    // check for alarms
    for (uint8_t i = 0; i <= 4; i++) {
        Serial.printf("Checking alarm %d.\n", i);
        if (!alarms[i].isOn) {
            Serial.println("Alarm is off.");
            continue;
        }

        if (alarms[i].hasDate) {
            uint8_t alarm_weekday = getWeekday(alarms[i].day, alarms[i].month, alarms[i].year);

            Serial.println("Alarm has date. Checking date.");
            if (alarms[i].day == currentTime.Day && alarms[i].month == currentTime.Month && alarms[i].year == 1970 + currentTime.Year) {
                Serial.println("Dates match.");
            } else {
                Serial.println("Dates do not match.");

                if (alarms[i].repeatType != ALARM_REPEAT_NONE){ 
                    Serial.println("Checking repetitions.");
                    Serial.println("Checking if alarm is in the future.");
                    
                    uint32_t now_epoch = makeTime(currentTime);
                    uint32_t alarm_epoch = tmConvert_t(alarms[i].year, alarms[i].month, alarms[i].day, alarms[i].hour, alarms[i].minute, 0);

                    if(now_epoch < alarm_epoch) {
                        Serial.println("Alarm is in the future.");
                        continue;
                    }
                
                    switch (alarms[i].repeatType) {
                        case ALARM_REPEAT_WEEKLY:
                            Serial.println("Alarm repeats weekly.");
                            if (currentTime.Wday != alarm_weekday) {
                                Serial.println("Weekly does not match.");
                                continue;
                            }
                            break;

                        case ALARM_REPEAT_MONTHLY:
                            Serial.println("Alarm repeats monthly.");
                            if (currentTime.Day != alarms[i].day) {
                                Serial.println("Monthly does not match.");
                                continue;
                            }
                            break;

                        case ALARM_REPEAT_YEARLY:
                            Serial.println("Alarm repeats yaerly.");
                            if (currentTime.Day != alarms[i].day || currentTime.Month != alarms[i].month) {
                                Serial.println("Yearly does not match.");
                                continue;
                            }
                            break;
                        
                        default:
                            break;
                    }
                    Serial.println("Repetitions match.");
                } else {
                    Serial.println("Alarm does not repeat.");
                }
            }
        } else {
            Serial.println("Alarm has no date.");
            Serial.println("Checking weekday.");
            switch (currentTime.Wday) {
                case 1: //sunday
                    if (!alarms[i].sunday) {
                        Serial.println("Weekday does not match.");
                        continue;
                    }
                    break;

                case 2: //monday
                    if (!alarms[i].monday) {
                        Serial.println("Weekday does not match.");
                        continue;
                    }
                    break;

                case 3: //tuesday
                    if (!alarms[i].tuesday) {
                        Serial.println("Weekday does not match.");
                        continue;
                    }
                    break;

                case 4: //wednesday
                    if (!alarms[i].wednesday) {
                        Serial.println("Weekday does not match.");
                        continue;
                    }
                    break;

                case 5: //thursday
                    if (!alarms[i].thursday) {
                        Serial.println("Weekday does not match.");
                        continue;
                    }
                    break;

                case 6: //friday
                    if (!alarms[i].friday) {
                        Serial.println("Weekday does not match.");
                        continue;
                    }
                    break;

                case 7: //saturday
                    if (!alarms[i].saturday) {
                        Serial.println("Weekday does not match.");
                        continue;
                    }
                    break;
                
                default:
                    break;
            }
            Serial.println("Weekday matches.");
        }

        Serial.println("Checking time.");
        if (alarms[i].hour == currentTime.Hour && alarms[i].minute == currentTime.Minute) {
            Serial.println("Hours match.");
            Serial.println("Trigerring alarm.");
            showBuzz("Alarm " + String(i + 1) + "!");
        } else {
            Serial.println("Time does not match.");
            continue;
        }
    }
}

void Watchy::handleButtonPress() {
    uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
    // Menu Button
    if (wakeupBit & MENU_BTN_MASK) {
        Serial.println("Menu button (interrupt).");
        menuButton();
        showState(guiState, true);
    }
    // Back Button
    else if (wakeupBit & BACK_BTN_MASK) {
        Serial.println("Back button (interrupt).");
        backButton();
        showState(guiState, true);
    }
    // Up Button
    else if (wakeupBit & UP_BTN_MASK) {
        Serial.println("Up button (interrupt).");
        upButton();
        showState(guiState, true);
    }
    // Down Button
    else if (wakeupBit & DOWN_BTN_MASK) {
        Serial.println("Down button (interrupt).");
        downButton();
        showState(guiState, true);
    }

    awakeLogic();
}

void Watchy::awakeLogic() {
    //when awake
    bool timeout = false;
    long lastPress = millis();
    bool pressed_after_awakening = false;
    bool display_released = false;
    uint8_t lastMinuteChecked = 99;
    pinMode(RTC_INT_PIN, INPUT);
    pinMode(MENU_BTN_PIN, INPUT);
    pinMode(BACK_BTN_PIN, INPUT);
    pinMode(UP_BTN_PIN, INPUT);
    pinMode(DOWN_BTN_PIN, INPUT);
    while (!timeout) {
        if (millis() - lastPress > 5000) {
            timeout = true;
        } else if (!display_released && pressed_after_awakening && millis() - lastPress > 250) {
            Serial.println("Releasing the display");
            display_released = true;
            showState(guiState, true);
        } else {

            if (digitalRead(RTC_INT_PIN) == 0) {  // LOW means RTC ticks
                RTC.read(currentTime);
                if (currentTime.Minute != lastMinuteChecked) {
                    lastMinuteChecked = currentTime.Minute;
                    Serial.println("RTC tick (awake).");
                    tick();
                }
            }

            menu_button.loop();
            back_button.loop();
            up_button.loop();
            down_button.loop();

            //in this context, pressed means released and released means pressed
            if(menu_button.isReleased()) {
                lastPress = millis();
                pressed_after_awakening = true;
                display_released = false;
                Serial.println("Menu button is pressed");
            }
            if(menu_button.isPressed()) {
                lastPress = millis();
                pressed_after_awakening = true;
                display_released = false;
                Serial.println("Menu button is released");
                menuButton();
            }
            
            if(back_button.isReleased()) {
                lastPress = millis();
                pressed_after_awakening = true;
                display_released = false;
                Serial.println("Back button is pressed");
            }
            if(back_button.isPressed()) {
                lastPress = millis();
                pressed_after_awakening = true;
                display_released = false;
                Serial.println("Back button is released");
                backButton();
            }

            if(up_button.isReleased()) {
                lastPress = millis();
                pressed_after_awakening = true;
                display_released = false;
                Serial.println("Up button is pressed");
            }
            if(up_button.isPressed()) {
                lastPress = millis();
                pressed_after_awakening = true;
                display_released = false;
                Serial.println("Up button is released");
                upButton();
            }            

            if(down_button.isReleased()) {
                lastPress = millis();
                pressed_after_awakening = true;
                display_released = false;
                Serial.println("Down button is pressed");
            }
            if(down_button.isPressed()) {
                lastPress = millis();
                pressed_after_awakening = true;
                display_released = false;
                Serial.println("Down button is released");
                downButton();
            }
                       
            

            /*
            // first check if the clock is ticking
            

            // then check for button presses
            if (digitalRead(MENU_BTN_PIN) == 1) {
                Serial.println("Menu button (awake).");
                lastPress = millis();
                menuButton();

            } else if (digitalRead(BACK_BTN_PIN) == 1) {
                Serial.println("Back button (awake).");
                lastPress = millis();
                backButton();

            } else if (digitalRead(UP_BTN_PIN) == 1) {
                Serial.println("Up button (awake).");
                lastPress = millis();
                upButton();

            } else if (digitalRead(DOWN_BTN_PIN) == 1) {
                Serial.println("Down button (awake).");
                lastPress = millis();
                downButton();
            }*/
        }
    }
}

void Watchy::menuButton() {
    if (guiState == WATCHFACE_STATE) {  // enter menu state if coming from watch face
        guiState = WORLD_TIME_STATE;
        //showWorldTime(true);
        // showSleep(false);
    } else if (guiState == SLEEP_STATE) {  // enter menu state if coming from watch face
        guiState = WORLD_TIME_STATE;
        //showWorldTime(true);
    } else if (guiState == WORLD_TIME_STATE) {
        guiState = CHRONOGRAPH_STATE;
        //showChronograph(true);
    } else if (guiState == CHRONOGRAPH_STATE) {
        guiState = TIMER_STATE;
        //showTimer(true);
    } else if (guiState == TIMER_STATE) {
        guiState = ALARM_STATE;
        //showAlarm(true);
    } else if (guiState == TIMER_SET_STATE) {
        if (timer_set_value_index == SET_TIMER_WILL_REPEAT) {
            timer_set_value_index = SET_TIMER_ON;
        } else {
            timer_set_value_index++;
        }
        //showTimerSet(true);
    } else if (guiState == ALARM_STATE) {
        guiState = PET_STATE;
        //showPET(true);
    } else if (guiState == ALARM_SET_STATE) {
        if (alarm_set_value_index == SET_ALARM_SUNDAY) {
            alarm_set_value_index = SET_ALARM_ON;
        } else if (!alarms[alarmIndex].hasDate && alarm_set_value_index == SET_ALARM_DATE) {
            alarm_set_value_index = SET_ALARM_MONDAY;
        } else if (alarms[alarmIndex].repeatType != ALARM_REPEAT_DAILY && alarm_set_value_index == SET_ALARM_REPEAT) {
            alarm_set_value_index = SET_ALARM_ON;
        } else {
            alarm_set_value_index++;
        }
        //showAlarmSet(true);
    } else if (guiState == PET_STATE) {
        //showMET(true); //we won't use MET just yet
        RTC.read(currentTime);
        guiState = WATCHFACE_STATE;
        //showWatchFace(true);

    } else if (guiState == PET_SET_STATE) {
        if (PET_set_value_index == SET_PET_YEAR){
            PET_set_value_index = SET_PET_ON;
        } else if (!PETs[PETIndex].isOn && PET_set_value_index == SET_PET_ON) {
            //do nothing
        } else {
            PET_set_value_index++;
        }
        //showPETSet(true);
    } else if (guiState == MET_STATE) {
        RTC.read(currentTime);
        guiState = WATCHFACE_STATE;
        //showWatchFace(true);
    }

    else if (guiState == MAIN_MENU_STATE) {  // if already in menu, then select menu item
        switch (menuIndex) {
            case 0:
                showAbout();
                break;
            case 1:
                showBuzz("Buzz!");
                break;
            case 2:
                showAccelerometer();
                break;
            case 3:
                setTime();
                break;
            case 4:
                setupWifi();
                break;
            case 5:
                showUpdateFW();
                break;
            case 6:
                showSyncNTP();
                break;
            default:
                break;
        }
    } else if (guiState == FW_UPDATE_STATE) {
        updateFWBegin();
    }
}

void Watchy::backButton() {
    if (guiState == MAIN_MENU_STATE) {  // exit to watch face if already in menu
        RTC.read(currentTime);
        guiState = WATCHFACE_STATE;
        //showWatchFace(true);
    } else if (guiState == APP_STATE) {
        showMenu(menuIndex, true);  // exit to menu if already in app
    } else if (guiState == FW_UPDATE_STATE) {
        showMenu(menuIndex, true);  // exit to menu if already in app
    } else if (guiState == WATCHFACE_STATE) {
        showMenu(menuIndex, true);
    } else if (guiState == TIMER_STATE) {
        timer_set_value_index = SET_TIMER_ON;
        guiState = TIMER_SET_STATE;
        //showTimerSet(true);
    } else if (guiState == TIMER_SET_STATE) {
        timers[timerIndex].days = timers[timerIndex].original_days;
        timers[timerIndex].hours = timers[timerIndex].original_hours;
        timers[timerIndex].minutes = timers[timerIndex].original_minutes;
        timers[timerIndex].repetition_count = 0;
        guiState = TIMER_STATE;
        //showTimer(true);
    }else if (guiState == ALARM_STATE) {
        alarm_set_value_index = SET_ALARM_ON;
        guiState = ALARM_SET_STATE;
        //showAlarmSet(true);
    } else if (guiState == ALARM_SET_STATE) {
        guiState = ALARM_STATE;
        //showAlarm(true);
    } else if (guiState == PET_STATE) {
        guiState = PET_SET_STATE;
        //showPETSet(true);
    } else if (guiState == PET_SET_STATE) {
        guiState = PET_STATE;
        //showPET(true);
    }
}

void Watchy::upButton() {
    if (guiState == MAIN_MENU_STATE) {  // increment menu index
        menuIndex--;
        if (menuIndex < 0) {
            menuIndex = MENU_LENGTH - 1;
        }
        //showMenu(menuIndex, true);
    } else if (guiState == WATCHFACE_STATE) {
        return;
    } else if (guiState == CHRONOGRAPH_STATE) {
        chrono.isRunning = !chrono.isRunning;
        //showChronograph(true);
    } else if (guiState == TIMER_STATE) {
        timers[timerIndex].isRunning = !timers[timerIndex].isRunning;
        //showTimer(true);
    } else if (guiState == TIMER_SET_STATE) {
        
        switch (timer_set_value_index) {
            case SET_TIMER_ON:
                timers[timerIndex].isRunning = !timers[timerIndex].isRunning;
                break;
            
            case SET_TIMER_DAYS:
                timers[timerIndex].original_days++;
                break;

            case SET_TIMER_HOURS:
                if (timers[timerIndex].original_hours == 23) {
                    timers[timerIndex].original_hours = 0;
                } else {
                    timers[timerIndex].original_hours++;
                }
                break;
                break;

            case SET_TIMER_MINUTES:
                if (timers[timerIndex].original_minutes == 59) {
                    timers[timerIndex].original_minutes = 0;
                } else {
                    timers[timerIndex].original_minutes++;
                }
                break;

            case SET_TIMER_WILL_REPEAT:
                timers[timerIndex].willRepeat = !timers[timerIndex].willRepeat;
                break;
        }

        //showTimerSet(true);
    } else if (guiState == ALARM_STATE) { 
        alarms[alarmIndex].isOn = !alarms[alarmIndex].isOn;
        //showAlarm(true);
    } else if (guiState == ALARM_SET_STATE) {
        switch (alarm_set_value_index) {
            case SET_ALARM_ON:
                alarms[alarmIndex].isOn = !alarms[alarmIndex].isOn;
                break;

            case SET_ALARM_MINUTE:
                if (alarms[alarmIndex].minute == 59) {
                    alarms[alarmIndex].minute = 0;
                } else {
                    alarms[alarmIndex].minute++;
                }
                break;

            case SET_ALARM_HOUR:
                if (alarms[alarmIndex].hour == 23) {
                    alarms[alarmIndex].hour = 0;
                } else {
                    alarms[alarmIndex].hour++;
                }
                break;

            case SET_ALARM_DATE:
                alarms[alarmIndex].hasDate = !alarms[alarmIndex].hasDate;
                if (alarms[alarmIndex].hasDate) {
                    alarms[alarmIndex].repeatType = ALARM_REPEAT_NONE;
                } else {
                    alarms[alarmIndex].repeatType = ALARM_REPEAT_DAILY;
                }
                break;

            case SET_ALARM_DAY:
                if (alarms[alarmIndex].day == getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year)) {
                    alarms[alarmIndex].day = 1;
                } else {
                    alarms[alarmIndex].day++;
                }
                break;

            case SET_ALARM_MONTH:
                if (alarms[alarmIndex].month == 12) {
                    alarms[alarmIndex].month = 1;
                } else {
                    alarms[alarmIndex].month++;
                    if (alarms[alarmIndex].day > getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year)) {
                        alarms[alarmIndex].day = getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year);
                    }
                }
                break;

            case SET_ALARM_YEAR:
                alarms[alarmIndex].year++;
                if (alarms[alarmIndex].day > getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year)) {
                    alarms[alarmIndex].day = getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year);
                }
                break;

            case SET_ALARM_REPEAT:
                if (alarms[alarmIndex].repeatType != ALARM_REPEAT_DAILY) {
                    if (alarms[alarmIndex].repeatType == ALARM_REPEAT_YEARLY) {
                        alarms[alarmIndex].repeatType = ALARM_REPEAT_NONE;
                    } else {
                        alarms[alarmIndex].repeatType++;
                    }
                }
                break;

            case SET_ALARM_MONDAY:
                alarms[alarmIndex].monday = !alarms[alarmIndex].monday;
                break;

            case SET_ALARM_TUESDAY:
                alarms[alarmIndex].tuesday = !alarms[alarmIndex].tuesday;
                break;

            case SET_ALARM_WEDNESDAY:
                alarms[alarmIndex].wednesday = !alarms[alarmIndex].wednesday;
                break;

            case SET_ALARM_THURSDAY:
                alarms[alarmIndex].thursday = !alarms[alarmIndex].thursday;
                break;

            case SET_ALARM_FRIDAY:
                alarms[alarmIndex].friday = !alarms[alarmIndex].friday;
                break;

            case SET_ALARM_SATURDAY:
                alarms[alarmIndex].saturday = !alarms[alarmIndex].saturday;
                break;
            
            case SET_ALARM_SUNDAY:
                alarms[alarmIndex].sunday = !alarms[alarmIndex].sunday;
                break;


        }
        //showAlarmSet(true);
    } else if (guiState == PET_STATE) {
        if (PETIndex == 0) {
            PETIndex = 4;
        } else {
            PETIndex--;
        }   
        //showPET(true);
    } else if (guiState == PET_SET_STATE) {
        switch (PET_set_value_index) {
            case SET_PET_ON:
                PETs[PETIndex].isOn = !PETs[PETIndex].isOn;
                if (PETs[PETIndex].isOn) {
                    setPETtoNow(&PETs[PETIndex]);
                }
                break;

            case SET_PET_WILL_BUZZ:
                PETs[PETIndex].willBuzz = !PETs[PETIndex].willBuzz;
                break;

            case SET_PET_MINUTE:
                if (PETs[PETIndex].minute == 59) {
                    PETs[PETIndex].minute = 0;
                } else {
                    PETs[PETIndex].minute++;
                }
                break;

            case SET_PET_HOUR:
                if (PETs[PETIndex].hour == 23) {
                    PETs[PETIndex].hour = 0;
                } else {
                    PETs[PETIndex].hour++;
                }
                break;

            case SET_PET_DAY:
                if (PETs[PETIndex].day == getLastDay(PETs[PETIndex].month, PETs[PETIndex].year)) {
                    PETs[PETIndex].day = 1;
                } else {
                    PETs[PETIndex].day++;
                }
                break;

            case SET_PET_MONTH:
                if (PETs[PETIndex].month == 12) {
                    PETs[PETIndex].month = 1;
                } else {
                    PETs[PETIndex].month++;
                    if (PETs[PETIndex].day > getLastDay(PETs[PETIndex].month, PETs[PETIndex].year)) {
                        PETs[PETIndex].day = getLastDay(PETs[PETIndex].month, PETs[PETIndex].year);
                    }
                }
                break;

            case SET_PET_YEAR:
                PETs[PETIndex].year++;
                if (PETs[PETIndex].day > getLastDay(PETs[PETIndex].month, PETs[PETIndex].year)) {
                    PETs[PETIndex].day = getLastDay(PETs[PETIndex].month, PETs[PETIndex].year);
                }
                break;
        }
        //showPETSet(true);
    }
}

void Watchy::downButton() {
    if (guiState == MAIN_MENU_STATE) {  // decrement menu index
        menuIndex++;
        if (menuIndex > MENU_LENGTH - 1) {
            menuIndex = 0;
        }
        //showMenu(menuIndex, true);
    } else if (guiState == WATCHFACE_STATE) {
        return;
    } else if (guiState == ALARM_STATE) {
        if (alarmIndex == 4) {
            alarmIndex = 0;
        } else {
            alarmIndex++;
        }
        //showAlarm(true);
    } else if (guiState == CHRONOGRAPH_STATE) {
        resetChronograph(&chrono);
        //showChronograph(true);
    } else if (guiState == TIMER_STATE) {
        if (timerIndex == 4) {
            timerIndex = 0;
        } else {
            timerIndex++;
        }
        //showTimer(true);
    } else if (guiState == TIMER_SET_STATE) {
        
        switch (timer_set_value_index) {
            case SET_TIMER_ON:
                timers[timerIndex].isRunning = !timers[timerIndex].isRunning;
                break;
            
            case SET_TIMER_DAYS:
                if (timers[timerIndex].original_days > 0) {
                    timers[timerIndex].original_days--;
                }
                break;

            case SET_TIMER_HOURS:
                if (timers[timerIndex].original_hours == 0) {
                    timers[timerIndex].original_hours = 23;
                } else {
                    timers[timerIndex].original_hours--;
                }
                break;
                break;

            case SET_TIMER_MINUTES:
                if (timers[timerIndex].original_minutes == 0) {
                    timers[timerIndex].original_minutes = 59;
                } else {
                    timers[timerIndex].original_minutes--;
                }
                break;

            case SET_TIMER_WILL_REPEAT:
                timers[timerIndex].willRepeat = !timers[timerIndex].willRepeat;
                break;
        }
        //showTimerSet(true);

    } else if (guiState == ALARM_SET_STATE) {
        switch (alarm_set_value_index) {
            case SET_ALARM_ON:
                alarms[alarmIndex].isOn = !alarms[alarmIndex].isOn;
                break;

            case SET_ALARM_MINUTE:
                if (alarms[alarmIndex].minute == 0) {
                    alarms[alarmIndex].minute = 59;
                } else {
                    alarms[alarmIndex].minute--;
                }
                break;

            case SET_ALARM_HOUR:
                if (alarms[alarmIndex].hour == 0) {
                    alarms[alarmIndex].hour = 23;
                } else {
                    alarms[alarmIndex].hour--;
                }
                break;

            case SET_ALARM_DATE:
                alarms[alarmIndex].hasDate = !alarms[alarmIndex].hasDate;
                if (alarms[alarmIndex].hasDate) {
                    alarms[alarmIndex].repeatType = ALARM_REPEAT_NONE;
                } else {
                    alarms[alarmIndex].repeatType = ALARM_REPEAT_DAILY;
                }
                break;

            case SET_ALARM_DAY:
                if (alarms[alarmIndex].day == 1) {
                    alarms[alarmIndex].day = getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year);
                } else {
                    alarms[alarmIndex].day--;
                }
                break;

            case SET_ALARM_MONTH:
                if (alarms[alarmIndex].month == 1) {
                    alarms[alarmIndex].month = 12;
                } else {
                    alarms[alarmIndex].month--;
                    if (alarms[alarmIndex].day > getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year)) {
                        alarms[alarmIndex].day = getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year);
                    }
                }
                break;

            case SET_ALARM_YEAR:
                alarms[alarmIndex].year--;
                if (alarms[alarmIndex].day > getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year)) {
                    alarms[alarmIndex].day = getLastDay(alarms[alarmIndex].month, alarms[alarmIndex].year);
                }
                break;

            case SET_ALARM_REPEAT:
                if (alarms[alarmIndex].repeatType != ALARM_REPEAT_DAILY) {
                    if (alarms[alarmIndex].repeatType == ALARM_REPEAT_NONE) {
                        alarms[alarmIndex].repeatType = ALARM_REPEAT_YEARLY;
                    } else {
                        alarms[alarmIndex].repeatType--;
                    }
                }
                break;

            case SET_ALARM_MONDAY:
                alarms[alarmIndex].monday = !alarms[alarmIndex].monday;
                break;

            case SET_ALARM_TUESDAY:
                alarms[alarmIndex].tuesday = !alarms[alarmIndex].tuesday;
                break;

            case SET_ALARM_WEDNESDAY:
                alarms[alarmIndex].wednesday = !alarms[alarmIndex].wednesday;
                break;

            case SET_ALARM_THURSDAY:
                alarms[alarmIndex].thursday = !alarms[alarmIndex].thursday;
                break;

            case SET_ALARM_FRIDAY:
                alarms[alarmIndex].friday = !alarms[alarmIndex].friday;
                break;

            case SET_ALARM_SATURDAY:
                alarms[alarmIndex].saturday = !alarms[alarmIndex].saturday;
                break;
            
            case SET_ALARM_SUNDAY:
                alarms[alarmIndex].sunday = !alarms[alarmIndex].sunday;
                break;
        }
        //showAlarmSet(true);
    } else if (guiState == PET_STATE) {
        if (PETIndex == 4) {
            PETIndex = 0;
        } else {
            PETIndex++;
        }        
        //showPET(true);
    } else if (guiState == PET_SET_STATE) {
        switch (PET_set_value_index) {
            case SET_PET_ON:
                PETs[PETIndex].isOn = !PETs[PETIndex].isOn;
                if (PETs[PETIndex].isOn) {
                    setPETtoNow(&PETs[PETIndex]);
                }
                break;

            case SET_PET_WILL_BUZZ:
                PETs[PETIndex].willBuzz = !PETs[PETIndex].willBuzz;
                break;

            case SET_PET_MINUTE:
                if (PETs[PETIndex].minute == 0) {
                    PETs[PETIndex].minute = 59;
                } else {
                    PETs[PETIndex].minute--;
                }
                break;

            case SET_PET_HOUR:
                if (PETs[PETIndex].hour == 0) {
                    PETs[PETIndex].hour = 23;
                } else {
                    PETs[PETIndex].hour--;
                }
                break;

            case SET_PET_DAY:
                if (PETs[PETIndex].day == 1) {
                    PETs[PETIndex].day = getLastDay(PETs[PETIndex].month, PETs[PETIndex].year);
                } else {
                    PETs[PETIndex].day--;
                }
                break;

            case SET_PET_MONTH:
                if (PETs[PETIndex].month == 1) {
                    PETs[PETIndex].month = 12;
                } else {
                    PETs[PETIndex].month--;
                    if (PETs[PETIndex].day > getLastDay(PETs[PETIndex].month, PETs[PETIndex].year)) {
                        PETs[PETIndex].day = getLastDay(PETs[PETIndex].month, PETs[PETIndex].year);
                    }
                }
                break;

            case SET_PET_YEAR:
                PETs[PETIndex].year--;
                if (PETs[PETIndex].day > getLastDay(PETs[PETIndex].month, PETs[PETIndex].year)) {
                    PETs[PETIndex].day = getLastDay(PETs[PETIndex].month, PETs[PETIndex].year);
                }
                break;
        }
        //showPETSet(true);
    }
}

void Watchy::showChronograph(bool partialRefresh) {
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);

    display.drawBitmap(0, 200-13, epd_bitmap_right_arrow, 8, 13, GxEPD_BLACK);
    
    if(chrono.isRunning){
        display.drawBitmap(200-12, 0, epd_bitmap_pause, 12, 13, GxEPD_BLACK);
    } else {
        display.drawBitmap(200-7, 0, epd_bitmap_play, 7, 13, GxEPD_BLACK);
        display.drawBitmap(200-10, 200-37, epd_bitmap_reset_n90, 10, 37, GxEPD_BLACK);
    }
    
    display.setTextWrap(false);
    drawCenteredString("Chronograph", 100, 20, false);
    display.printf("\n\n");

    display.setFont(&Bizcat_32pt7b);
    display.printf("%d/%02d:%02d\n", chrono.days, chrono.hours, chrono.minutes);
    display.setFont(&Bizcat_24pt7b);

    RTC.read(currentTime);
    uint32_t now_epoch = makeTime(currentTime);
    //time after return
    uint32_t TAR_epoch = now_epoch + chrono.days * 86400 + chrono.hours * 3600 + chrono.minutes * 60;
    tmElements_t TAR_tme;
    breakTime(TAR_epoch, TAR_tme);

    display.println("Time after return:");
    display.drawBitmap(0, 120, epd_bitmap_uturn_arrow, 20, 20, GxEPD_BLACK);
    display.setCursor(display.getCursorX() + 25, display.getCursorY());
    display.printf("%02d:%02d\n", TAR_tme.Hour, TAR_tme.Minute);
    display.setCursor(display.getCursorX() + 25, display.getCursorY());
    display.printf("%02d/%02d/%4d", TAR_tme.Day, TAR_tme.Month, tmYearToCalendar(TAR_tme.Year));

    display.display(partialRefresh);
    guiState = CHRONOGRAPH_STATE;
}

void Watchy::showWorldTime(bool partialRefresh) {
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);

    display.drawBitmap(0, 200-13, epd_bitmap_right_arrow, 8, 13, GxEPD_BLACK);
    
    display.setTextWrap(false);
    drawCenteredString("World Time", 100, 20, false);

    display.printf("\n\n");

    RTC.read(currentTime);
    uint32_t now_epoch = makeTime(currentTime);
    uint32_t chi_epoch = now_epoch - 7*3600;
    uint32_t cbb_epoch = now_epoch - 6*3600;
    tmElements_t cbb_tmelements;
    tmElements_t chi_tmelements;
    breakTime(cbb_epoch, cbb_tmelements);
    breakTime(chi_epoch, chi_tmelements);

    display.println("Cochabamba");
    display.printf("%02d:%02d %02d/%02d/%04d\n\n", 
                    cbb_tmelements.Hour, cbb_tmelements.Minute, cbb_tmelements.Day, cbb_tmelements.Month, tmYearToCalendar(cbb_tmelements.Year)
    );

    display.println("Chicago");
    display.printf("%02d:%02d %02d/%02d/%04d\n\n", 
                    chi_tmelements.Hour, chi_tmelements.Minute, chi_tmelements.Day, chi_tmelements.Month, tmYearToCalendar(chi_tmelements.Year)
    );

    display.display(partialRefresh);
    guiState = WORLD_TIME_STATE;
}

void Watchy::showTimer(bool partialRefresh) {
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);

    display.drawBitmap(0, 0, epd_bitmap_set_90, 10, 22, GxEPD_BLACK);
    display.drawBitmap(0, 200-13, epd_bitmap_right_arrow, 8, 13, GxEPD_BLACK);
    if (timers[timerIndex].isRunning){
        display.drawBitmap(200-12, 0, epd_bitmap_pause, 12, 13, GxEPD_BLACK);
    } else {
        display.drawBitmap(200-7, 0, epd_bitmap_play, 7, 13, GxEPD_BLACK);
    }
    display.drawBitmap(200-13, 200-8, epd_bitmap_down_arrow, 13, 8, GxEPD_BLACK);

    display.setTextWrap(false);
    drawCenteredString("Timers", 100, 20, false);
    display.printf("\n\n");

    display.printf("%d", timerIndex+1);

    if (timers[timerIndex].isRunning) {
        display.drawBitmap(display.getCursorX() + 10, display.getCursorY() - 13, epd_bitmap_on, 26, 14, GxEPD_BLACK);
    } else {
        display.drawBitmap(display.getCursorX() + 10, display.getCursorY() - 13, epd_bitmap_off, 26, 14, GxEPD_BLACK);
    }

    display.printf("\n\n");

    display.setFont(&Bizcat_32pt7b);
    display.printf("%d/%02d:%02d\n", timers[timerIndex].days, timers[timerIndex].hours, timers[timerIndex].minutes);
    display.setFont(&Bizcat_24pt7b);

    if (timers[timerIndex].willRepeat) {
        display.drawBitmap(0, display.getCursorY() - 7, epd_bitmap_repeat_arrow, 25, 26, GxEPD_BLACK);
        display.setCursor(display.getCursorX() + 40, display.getCursorY());
        display.println("Will repeat");
        display.setCursor(display.getCursorX() + 40, display.getCursorY());
        display.printf("Count: %d", timers[timerIndex].repetition_count);
    } else {
        display.drawBitmap(0, display.getCursorY() - 16, epd_bitmap_end_arrow, 32, 19, GxEPD_BLACK);
        display.setCursor(display.getCursorX() + 50, display.getCursorY());
        display.println("Will not repeat");
    }

    display.display(partialRefresh);
    guiState = TIMER_STATE;
}

void Watchy::showTimerSet(bool partialRefresh){
    int16_t bound_x, bound_y;
    uint16_t bound_width, bound_height;

    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);

    display.drawBitmap(0, 0, epd_bitmap_done_90, 10, 32, GxEPD_BLACK);
    display.drawBitmap(0, 200-13, epd_bitmap_right_arrow, 8, 13, GxEPD_BLACK);
    display.drawBitmap(200 - 9, 0, epd_bitmap_plus, 9, 9, GxEPD_BLACK);
    display.drawBitmap(200 - 9, 200 - 9, epd_bitmap_minus, 9, 9, GxEPD_BLACK);

    display.setCursor(20, 20);
    display.printf("Set Timer %d\n", timerIndex+1);
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    String timer_on_text;
    if (timers[timerIndex].isRunning) {
        timer_on_text = "On";
    } else {
        timer_on_text = "Off";
    }
    if (timer_set_value_index == SET_TIMER_ON) {
        display.getTextBounds(timer_on_text, 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    display.println(timer_on_text);
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    if (timer_set_value_index == SET_TIMER_DAYS) {
        display.getTextBounds(String(timers[timerIndex].original_days), 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }

    display.print(timers[timerIndex].original_days);
    display.print("/");

    if (timer_set_value_index == SET_TIMER_HOURS) {
        display.getTextBounds("09", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    if (timers[timerIndex].original_hours < 10) {
        display.print("0");
    }
    display.print(timers[timerIndex].original_hours);

    display.print(":");

    if (timer_set_value_index == SET_TIMER_MINUTES) {
        display.getTextBounds("30", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    if (timers[timerIndex].original_minutes < 10) {
        display.print("0");
    }
    display.print(timers[timerIndex].original_minutes);
    display.print("\n");
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    String timer_will_repeat_text;
    if (timers[timerIndex].willRepeat) {
        timer_will_repeat_text = "Repeat at end";
    } else {
        timer_will_repeat_text = "Stop at end";
    }
    if (timer_set_value_index == SET_TIMER_WILL_REPEAT) {
        display.getTextBounds(timer_will_repeat_text, 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    display.println(timer_will_repeat_text);
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    guiState = TIMER_SET_STATE;
    display.display(partialRefresh);
}

void Watchy::showAlarm(bool partialRefresh) {
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);

    display.drawBitmap(0, 0, epd_bitmap_set_90, 10, 22, GxEPD_BLACK);
    display.drawBitmap(0, 200-13, epd_bitmap_right_arrow, 8, 13, GxEPD_BLACK);
    display.drawBitmap(200-13, 200-8, epd_bitmap_down_arrow, 13, 8, GxEPD_BLACK);
    display.drawBitmap(200-10, 0, epd_bitmap_toggle_n90, 10, 45, GxEPD_BLACK);
    
    display.setTextWrap(false);
    drawCenteredString("Alarms", 100, 20, false);
    display.printf("\n\n");

    //display.setFont(&Bizcat_24pt7b);

    display.printf("%d", alarmIndex+1);

    if (alarms[alarmIndex].isOn) {
        display.drawBitmap(display.getCursorX() + 10, display.getCursorY() - 13, epd_bitmap_on, 26, 14, GxEPD_BLACK);
    } else {
        display.drawBitmap(display.getCursorX() + 10, display.getCursorY() - 13, epd_bitmap_off, 26, 14, GxEPD_BLACK);
    }

    display.printf("\n\n");

    display.setFont(&Bizcat_32pt7b);

    if (alarms[alarmIndex].hasDate) {
        display.printf("%02d:%02d\n%02d/%02d/%04d\n", alarms[alarmIndex].hour, alarms[alarmIndex].minute, alarms[alarmIndex].day, alarms[alarmIndex].month, alarms[alarmIndex].year);
        if (alarms[alarmIndex].repeatType != ALARM_REPEAT_NONE) {
            display.setFont(&Bizcat_24pt7b);
            switch (alarms[alarmIndex].repeatType) {
            case ALARM_REPEAT_WEEKLY:
                display.println("Weekly");
                break;

            case ALARM_REPEAT_MONTHLY:
                display.println("Monthly");
                break;

            case ALARM_REPEAT_YEARLY:
                display.println("Yearly");
                break;
            
            default:
                break;
            }
        }
    } else {
        display.printf("%02d:%02d\n", alarms[alarmIndex].hour, alarms[alarmIndex].minute);

        display.setFont(&Bizcat_24pt7b);
        display.print(alarms[alarmIndex].monday?    "M " : "- ");
        display.print(alarms[alarmIndex].tuesday?   "T " : "- ");
        display.print(alarms[alarmIndex].wednesday? "W " : "- ");
        display.print(alarms[alarmIndex].thursday?  "T " : "- ");
        display.print(alarms[alarmIndex].friday?    "F " : "- ");
        display.print(alarms[alarmIndex].saturday?  "S " : "- ");
        display.print(alarms[alarmIndex].sunday?    "S " : "-");
    }

    display.display(partialRefresh);
    guiState = ALARM_STATE;
}

void Watchy::showAlarmSet(bool partialRefresh) {
    guiState = ALARM_SET_STATE;

    drawAlarmSet();
    display.display(partialRefresh);
}

void Watchy::drawAlarmSet() {
    int16_t bound_x, bound_y;
    uint16_t bound_width, bound_height;

    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);

    display.drawBitmap(0, 0, epd_bitmap_done_90, 10, 32, GxEPD_BLACK);
    display.drawBitmap(0, 200-13, epd_bitmap_right_arrow, 8, 13, GxEPD_BLACK);
    display.drawBitmap(200 - 9, 0, epd_bitmap_plus, 9, 9, GxEPD_BLACK);
    display.drawBitmap(200 - 9, 200 - 9, epd_bitmap_minus, 9, 9, GxEPD_BLACK);

    display.setCursor(20, 20);
    display.printf("Set Alarm %d\n", alarmIndex+1);
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    String alarm_on_text;
    if (alarms[alarmIndex].isOn) {
        alarm_on_text = "On";
    } else {
        alarm_on_text = "Off";
    }
    if (alarm_set_value_index == SET_ALARM_ON) {
        display.getTextBounds(alarm_on_text, 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    display.println(alarm_on_text);
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    if (alarm_set_value_index == SET_ALARM_HOUR) {
        display.getTextBounds("09", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    if (alarms[alarmIndex].hour < 10) {
        display.print("0");
    }
    display.print(alarms[alarmIndex].hour);

    display.print(":");

    if (alarm_set_value_index == SET_ALARM_MINUTE) {
        display.getTextBounds("30", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    if (alarms[alarmIndex].minute < 10) {
        display.print("0");
    }
    display.print(alarms[alarmIndex].minute);
    display.print("\n");
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    String alarm_hasDate_text;
    if (alarms[alarmIndex].hasDate) {
        alarm_hasDate_text = "Date";
    } else {
        alarm_hasDate_text = "No date";
    }
    if (alarm_set_value_index == SET_ALARM_DATE) {
        display.getTextBounds(alarm_hasDate_text, 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    display.println(alarm_hasDate_text);
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    if (alarms[alarmIndex].hasDate) {
        if (alarm_set_value_index == SET_ALARM_DAY) {
            display.getTextBounds("27", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        if (alarms[alarmIndex].day < 10) {
            display.print("0");
        }
        display.print(alarms[alarmIndex].day);
        display.print("/");

        if (alarm_set_value_index == SET_ALARM_MONTH) {
            display.getTextBounds("09", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }

        if (alarms[alarmIndex].month < 10) {
            display.print("0");
        }
        display.print(alarms[alarmIndex].month);
        display.print("/");

        if (alarm_set_value_index == SET_ALARM_YEAR) {
            display.getTextBounds("1995", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.print(alarms[alarmIndex].year);
        display.print("\n");
        display.setCursor(display.getCursorX() + 20, display.getCursorY());
    }

    String alarm_repeat_type_text;
    switch (alarms[alarmIndex].repeatType) {
        case ALARM_REPEAT_NONE:
            alarm_repeat_type_text = "None";
            break;

        case ALARM_REPEAT_DAILY:
            alarm_repeat_type_text = "Daily";
            break;

        case ALARM_REPEAT_WEEKLY:
            alarm_repeat_type_text = "Weekly";
            break;

        case ALARM_REPEAT_MONTHLY:
            alarm_repeat_type_text = "Monthly";
            break;

        case ALARM_REPEAT_YEARLY:
            alarm_repeat_type_text = "Yearly";
            break;

        default:
            break;
    }

    if (alarm_set_value_index == SET_ALARM_REPEAT) {
        display.getTextBounds(alarm_repeat_type_text, 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    display.println(alarm_repeat_type_text);
    display.setCursor(display.getCursorX() + 20, display.getCursorY());


    if (alarms[alarmIndex].repeatType == ALARM_REPEAT_DAILY){
        if (alarm_set_value_index == SET_ALARM_MONDAY) {
            display.getTextBounds("M", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.print(alarms[alarmIndex].monday    ? "M " : "- ");

        if (alarm_set_value_index == SET_ALARM_TUESDAY) {
            display.getTextBounds("M", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.print(alarms[alarmIndex].tuesday   ? "T " : "- ");

        if (alarm_set_value_index == SET_ALARM_WEDNESDAY) {
            display.getTextBounds("M", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.print(alarms[alarmIndex].wednesday ? "W " : "- ");

        if (alarm_set_value_index == SET_ALARM_THURSDAY) {
            display.getTextBounds("M", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.print(alarms[alarmIndex].thursday  ? "T " : "- ");

        if (alarm_set_value_index == SET_ALARM_FRIDAY) {
            display.getTextBounds("M", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.print(alarms[alarmIndex].friday    ? "F " : "- ");

        if (alarm_set_value_index == SET_ALARM_SATURDAY) {
            display.getTextBounds("M", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.print(alarms[alarmIndex].saturday  ? "S " : "- ");

        if (alarm_set_value_index == SET_ALARM_SUNDAY) {
            display.getTextBounds("M", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.print(alarms[alarmIndex].sunday    ? "S " : "- ");
    }
}

void Watchy::showPET(bool partialRefresh) {
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);

    display.drawBitmap(0, 200-13, epd_bitmap_right_arrow, 8, 13, GxEPD_BLACK);
    display.drawBitmap(200-13, 0, epd_bitmap_up_arrow, 13, 8, GxEPD_BLACK);
    display.drawBitmap(200-13, 200-8, epd_bitmap_down_arrow, 13, 8, GxEPD_BLACK);
    display.drawBitmap(0, 0, epd_bitmap_set_90, 10, 22, GxEPD_BLACK);


    display.setTextWrap(false);
    drawCenteredString("PET", 100, 20, false);
    display.printf("\n\n");
    display.println(PETIndex + 1);

    RTC.read(currentTime);

    if (PETs[PETIndex].isOn) {
        display.printf("%02d:%02d %02d/%02d/%04d\n\n", PETs[PETIndex].hour, PETs[PETIndex].minute, PETs[PETIndex].day, PETs[PETIndex].month, PETs[PETIndex].year);

        uint32_t PET_epoch = tmConvert_t(PETs[PETIndex].year, PETs[PETIndex].month, PETs[PETIndex].day, PETs[PETIndex].hour, PETs[PETIndex].minute, 0);
        uint32_t now_epoch = tmConvert_t(currentTime.Year + 1970, currentTime.Month, currentTime.Day, currentTime.Hour, currentTime.Minute, 0);
        int32_t diff_seconds = now_epoch - PET_epoch;

        display.setFont(&Bizcat_32pt7b);
        if(diff_seconds >= 0){
            display.print("+");
        } else {
            display.print("-");
        }
        display.printf("%d/%02d:%02d\n", secondsToDays(diff_seconds), secondsToHours(diff_seconds), secondsToMinutes(diff_seconds));

        display.setFont(&Bizcat_24pt7b);

        if (PETs[PETIndex].willBuzz){
            display.println("Will buzz");
        } else {
            display.println("Will not buzz");
        }

    } else {
        display.println("Off");
    }

    
    display.display(partialRefresh);
    guiState = PET_STATE;
}

void Watchy::showPETSet(bool partialRefresh){
    int16_t bound_x, bound_y;
    uint16_t bound_width, bound_height;

    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);

    display.drawBitmap(0, 0, epd_bitmap_done_90, 10, 32, GxEPD_BLACK);
    display.drawBitmap(0, 200-13, epd_bitmap_right_arrow, 8, 13, GxEPD_BLACK);
    display.drawBitmap(200 - 9, 0, epd_bitmap_plus, 9, 9, GxEPD_BLACK);
    display.drawBitmap(200 - 9, 200 - 9, epd_bitmap_minus, 9, 9, GxEPD_BLACK);

    display.setCursor(20, 20);
    display.printf("Set PET %d\n", PETIndex+1);
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    String PET_on_text;
    if (PETs[PETIndex].isOn) {
        PET_on_text = "On";
    } else {
        PET_on_text = "Off";
    }
    if (PET_set_value_index == SET_PET_ON) {
        display.getTextBounds(PET_on_text, 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
        display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
    }
    display.println(PET_on_text);
    display.setCursor(display.getCursorX() + 20, display.getCursorY());

    if (PETs[PETIndex].isOn) {
        String PET_will_buzz_text;
        if (PETs[PETIndex].willBuzz) {
            PET_will_buzz_text = "Will buzz";
        } else {
            PET_will_buzz_text = "Will not buzz";
        }
        if (PET_set_value_index == SET_PET_WILL_BUZZ) {
            display.getTextBounds(PET_will_buzz_text, 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.println(PET_will_buzz_text);
        display.setCursor(display.getCursorX() + 20, display.getCursorY());

        if (PET_set_value_index == SET_PET_HOUR) {
            display.getTextBounds("09", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        if (PETs[PETIndex].hour < 10) {
            display.print("0");
        }
        display.print(PETs[PETIndex].hour);

        display.print(":");

        if (PET_set_value_index == SET_PET_MINUTE) {
            display.getTextBounds("30", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        if (PETs[PETIndex].minute < 10) {
            display.print("0");
        }
        display.print(PETs[PETIndex].minute);
        display.print("\n");
        display.setCursor(display.getCursorX() + 20, display.getCursorY());

        if (PET_set_value_index == SET_PET_DAY) {
            display.getTextBounds("27", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        if (PETs[PETIndex].day < 10) {
            display.print("0");
        }
        display.print(PETs[PETIndex].day);
        display.print("/");

        if (PET_set_value_index == SET_PET_MONTH) {
            display.getTextBounds("09", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }

        if (PETs[PETIndex].month < 10) {
            display.print("0");
        }
        display.print(PETs[PETIndex].month);
        display.print("/");

        if (PET_set_value_index == SET_PET_YEAR) {
            display.getTextBounds("1995", 0, 20, &bound_x, &bound_y, &bound_width, &bound_height);
            display.fillRect(display.getCursorX(), display.getCursorY() + 3, bound_width, 2, GxEPD_BLACK);
        }
        display.print(PETs[PETIndex].year);
        display.print("\n");
        display.setCursor(display.getCursorX() + 20, display.getCursorY());
    }

    guiState = PET_SET_STATE;
    display.display(partialRefresh);
}

void Watchy::showMET(bool partialRefresh) {
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);

    display.drawBitmap(0, 200-13, epd_bitmap_right_arrow, 8, 13, GxEPD_BLACK);
    
    
    display.setTextWrap(false);
    drawCenteredString("MET", 100, 20, false);
    display.printf("\n\n");

    display.display(partialRefresh);
    guiState = MET_STATE;
}

void Watchy::showMenu(byte menuIndex, bool partialRefresh) {
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    int16_t x1, y1;
    uint16_t w, h;
    int16_t yPos;

    const char *menuItems[] = {"About Watchy", "Vibrate Motor", "Show Accelerometer", "Set Time", "Setup WiFi", "Update Firmware", "Sync NTP"};
    for (int i = 0; i < MENU_LENGTH; i++) {
        yPos = MENU_HEIGHT + (MENU_HEIGHT * i);
        display.setCursor(0, yPos);
        if (i == menuIndex) {
            display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
            display.fillRect(x1 - 1, y1 - 10, 200, h + 15, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            display.println(menuItems[i]);
        } else {
            display.setTextColor(GxEPD_WHITE);
            display.println(menuItems[i]);
        }
    }

    display.display(partialRefresh);

    guiState = MAIN_MENU_STATE;
}

void Watchy::showFastMenu(byte menuIndex) {
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    int16_t x1, y1;
    uint16_t w, h;
    int16_t yPos;

    const char *menuItems[] = {"About Watchy", "Vibrate Motor", "Show Accelerometer", "Set Time", "Setup WiFi", "Update Firmware", "Sync NTP"};
    for (int i = 0; i < MENU_LENGTH; i++) {
        yPos = MENU_HEIGHT + (MENU_HEIGHT * i);
        display.setCursor(0, yPos);
        if (i == menuIndex) {
            display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
            display.fillRect(x1 - 1, y1 - 10, 200, h + 15, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            display.println(menuItems[i]);
        } else {
            display.setTextColor(GxEPD_WHITE);
            display.println(menuItems[i]);
        }
    }

    display.display(true);

    guiState = MAIN_MENU_STATE;
}

void Watchy::showAbout() {
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 20);

    display.print("LibVer: ");
    display.println(WATCHY_LIB_VER);

    const char *RTC_HW[3] = {"<UNKNOWN>", "DS3231", "PCF8563"};
    display.print("RTC: ");
    display.println(RTC_HW[RTC.rtcType]);  // 0 = UNKNOWN, 1 = DS3231, 2 = PCF8563

    display.print("Batt: ");
    float voltage = getBatteryVoltage();
    display.print(voltage);
    display.println("V");

    display.print("Batt %: ");
    uint8_t percentage = 2808.3808 * pow(voltage, 4) -
                         43560.9157 * pow(voltage, 3) +
                         252848.5888 * pow(voltage, 2) -
                         650767.4615 * voltage +
                         626532.5703;
    percentage = min((uint8_t)100, percentage);
    percentage = max((uint8_t)0, percentage);
    display.print(percentage);
    display.println("%");

    display.display(true);  // full refresh

    guiState = APP_STATE;
}

void Watchy::showBuzz(String message) {
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&Bizcat_24pt7b);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredString(message, 100, 100, false);
    display.display(true);  // full refresh
    vibMotor();

    switch (guiState) {
        case WATCHFACE_STATE:
            showWatchFace(true);
            break;

        case WORLD_TIME_STATE:
            showWorldTime(true);
            break;

        case CHRONOGRAPH_STATE:
            showChronograph(true);
            break;

        case TIMER_STATE:
            showTimer(true);
            break;

        case ALARM_STATE:
            showAlarm(true);
            break;

        case ALARM_SET_STATE:
            showAlarmSet(true);
            break;

        case PET_STATE:
            showPET(true);
            break;

        case MET_STATE:
            showMET(true);
            break;

        case MAIN_MENU_STATE:
            showMenu(menuIndex, true);
            break;

        default:
            break;
    }
}

void Watchy::showState(int guiState, bool partialRefresh){
    switch (guiState) {
        case WATCHFACE_STATE:
            showWatchFace(partialRefresh);
            break;

        case WORLD_TIME_STATE:
            showWorldTime(partialRefresh);
            break;

        case CHRONOGRAPH_STATE:
            showChronograph(partialRefresh);
            break;

        case TIMER_STATE:
            showTimer(partialRefresh);
            break;

        case TIMER_SET_STATE:
            showTimerSet(partialRefresh);
            break;

        case ALARM_STATE:
            showAlarm(partialRefresh);
            break;

        case ALARM_SET_STATE:
            showAlarmSet(partialRefresh);
            break;

        case PET_STATE:
            showPET(partialRefresh);
            break;

        case PET_SET_STATE:
            showPETSet(partialRefresh);
            break;

        case MET_STATE:
            showMET(partialRefresh);
            break;

        case MAIN_MENU_STATE:
            showMenu(menuIndex, partialRefresh);
            break;

        default:
            break;
    }
}

void Watchy::vibMotor(uint8_t intervalMs, uint8_t length) {
    pinMode(VIB_MOTOR_PIN, OUTPUT);
    bool motorOn = false;
    for (int i = 0; i < length; i++) {
        motorOn = !motorOn;
        digitalWrite(VIB_MOTOR_PIN, motorOn);
        delay(intervalMs);
    }
}

void Watchy::setTime() {
    guiState = APP_STATE;

    RTC.read(currentTime);

    int8_t minute = currentTime.Minute;
    int8_t hour = currentTime.Hour;
    int8_t day = currentTime.Day;
    int8_t month = currentTime.Month;
    int8_t year = tmYearToY2k(currentTime.Year);

    int8_t setIndex = SET_HOUR;

    int8_t blink = 0;

    pinMode(DOWN_BTN_PIN, INPUT);
    pinMode(UP_BTN_PIN, INPUT);
    pinMode(MENU_BTN_PIN, INPUT);
    pinMode(BACK_BTN_PIN, INPUT);

    display.setFullWindow();

    while (1) {
        if (digitalRead(MENU_BTN_PIN) == 1) {
            setIndex++;
            if (setIndex > SET_DAY) {
                break;
            }
        }
        if (digitalRead(BACK_BTN_PIN) == 1) {
            if (setIndex != SET_HOUR) {
                setIndex--;
            }
        }

        blink = 1 - blink;

        if (digitalRead(UP_BTN_PIN) == 1) {
            blink = 1;
            switch (setIndex) {
                case SET_HOUR:
                    hour == 23 ? (hour = 0) : hour++;
                    break;
                case SET_MINUTE:
                    minute == 59 ? (minute = 0) : minute++;
                    break;
                case SET_YEAR:
                    year == 99 ? (year = 0) : year++;
                    break;
                case SET_MONTH:
                    month == 12 ? (month = 1) : month++;
                    break;
                case SET_DAY:
                    day == 31 ? (day = 1) : day++;
                    break;
                default:
                    break;
            }
        }

        if (digitalRead(DOWN_BTN_PIN) == 1) {
            blink = 1;
            switch (setIndex) {
                case SET_HOUR:
                    hour == 0 ? (hour = 23) : hour--;
                    break;
                case SET_MINUTE:
                    minute == 0 ? (minute = 59) : minute--;
                    break;
                case SET_YEAR:
                    year == 0 ? (year = 99) : year--;
                    break;
                case SET_MONTH:
                    month == 1 ? (month = 12) : month--;
                    break;
                case SET_DAY:
                    day == 1 ? (day = 31) : day--;
                    break;
                default:
                    break;
            }
        }

        display.fillScreen(GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&DSEG7_Classic_Bold_53);

        display.setCursor(5, 80);
        if (setIndex == SET_HOUR) {  // blink hour digits
            display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
        }
        if (hour < 10) {
            display.print("0");
        }
        display.print(hour);

        display.setTextColor(GxEPD_WHITE);
        display.print(":");

        display.setCursor(108, 80);
        if (setIndex == SET_MINUTE) {  // blink minute digits
            display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
        }
        if (minute < 10) {
            display.print("0");
        }
        display.print(minute);

        display.setTextColor(GxEPD_WHITE);

        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(45, 150);
        if (setIndex == SET_YEAR) {  // blink minute digits
            display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
        }
        display.print(2000 + year);

        display.setTextColor(GxEPD_WHITE);
        display.print("/");

        if (setIndex == SET_MONTH) {  // blink minute digits
            display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
        }
        if (month < 10) {
            display.print("0");
        }
        display.print(month);

        display.setTextColor(GxEPD_WHITE);
        display.print("/");

        if (setIndex == SET_DAY) {  // blink minute digits
            display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
        }
        if (day < 10) {
            display.print("0");
        }
        display.print(day);
        display.display(true);  // partial refresh
    }

    tmElements_t tm;
    tm.Month = month;
    tm.Day = day;
    tm.Year = y2kYearToTm(year);
    tm.Hour = hour;
    tm.Minute = minute;
    tm.Second = 0;

    RTC.set(tm);

    showMenu(menuIndex, true);
}

void Watchy::showAccelerometer() {
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);

    Accel acc;

    long previousMillis = 0;
    long interval = 200;

    guiState = APP_STATE;

    pinMode(BACK_BTN_PIN, INPUT);

    while (1) {
        unsigned long currentMillis = millis();

        if (digitalRead(BACK_BTN_PIN) == 1) {
            break;
        }

        if (currentMillis - previousMillis > interval) {
            previousMillis = currentMillis;
            // Get acceleration data
            bool res = sensor.getAccel(acc);
            uint8_t direction = sensor.getDirection();
            display.fillScreen(GxEPD_BLACK);
            display.setCursor(0, 30);
            if (res == false) {
                display.println("getAccel FAIL");
            } else {
                display.print("  X:");
                display.println(acc.x);
                display.print("  Y:");
                display.println(acc.y);
                display.print("  Z:");
                display.println(acc.z);

                display.setCursor(30, 130);
                switch (direction) {
                    case DIRECTION_DISP_DOWN:
                        display.println("FACE DOWN");
                        break;
                    case DIRECTION_DISP_UP:
                        display.println("FACE UP");
                        break;
                    case DIRECTION_BOTTOM_EDGE:
                        display.println("BOTTOM EDGE");
                        break;
                    case DIRECTION_TOP_EDGE:
                        display.println("TOP EDGE");
                        break;
                    case DIRECTION_RIGHT_EDGE:
                        display.println("RIGHT EDGE");
                        break;
                    case DIRECTION_LEFT_EDGE:
                        display.println("LEFT EDGE");
                        break;
                    default:
                        display.println("ERROR!!!");
                        break;
                }
            }
            display.display(true);  // full refresh
        }
    }

    showMenu(menuIndex, true);
}

void Watchy::showWatchFace(bool partialRefresh) {
    display.setFullWindow();
    drawWatchFace();
    display.display(partialRefresh);  // partial refresh
    guiState = WATCHFACE_STATE;
}

void Watchy::drawWatchFace() {
    display.setFont(&DSEG7_Classic_Bold_53);
    display.setCursor(5, 53 + 60);
    if (currentTime.Hour < 10) {
        display.print("0");
    }
    display.print(currentTime.Hour);
    display.print(":");
    if (currentTime.Minute < 10) {
        display.print("0");
    }
    display.println(currentTime.Minute);
}

void Watchy::showSleep(bool partialRefresh) {
    guiState = SLEEP_STATE;
    drawSleep();
    display.display(partialRefresh);
}

void Watchy::drawSleep() {
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(0, 0, epd_bitmap_yoshi_sleeping, 200, 200, GxEPD_BLACK);
}

weatherData Watchy::getWeatherData() {
    return getWeatherData(settings.cityID, settings.weatherUnit, settings.weatherLang, settings.weatherURL, settings.weatherAPIKey, settings.weatherUpdateInterval);
}

weatherData Watchy::getWeatherData(String cityID, String units, String lang, String url, String apiKey, uint8_t updateInterval) {
    currentWeather.isMetric = units == String("metric");
    if (weatherIntervalCounter < 0) {  //-1 on first run, set to updateInterval
        weatherIntervalCounter = updateInterval;
    }
    if (weatherIntervalCounter >= updateInterval) {  // only update if WEATHER_UPDATE_INTERVAL has elapsed i.e. 30 minutes
        if (connectWiFi()) {
            HTTPClient http;               // Use Weather API for live data if WiFi is connected
            http.setConnectTimeout(3000);  // 3 second max timeout
            String weatherQueryURL = url + cityID + String("&units=") + units + String("&lang=") + lang + String("&appid=") + apiKey;
            http.begin(weatherQueryURL.c_str());
            int httpResponseCode = http.GET();
            if (httpResponseCode == 200) {
                String payload = http.getString();
                DynamicJsonDocument doc(1024);
                auto error = deserializeJson(doc, payload);
                if (!error) {
                    currentWeather.temperature = doc["main"]["temp"].as<int>();
                    currentWeather.weatherConditionCode = doc["weather"][0]["id"].as<int>();
                    currentWeather.weatherDescription = doc["weather"][0]["main"].as<const char *>();
                } else {
                    Serial.println(error.c_str());
                }
            } else {
                // http error
            }
            http.end();
            // turn off radios
            WiFi.mode(WIFI_OFF);
            btStop();
        } else {                                             // No WiFi, use internal temperature sensor
            uint8_t temperature = sensor.readTemperature();  // celsius
            if (!currentWeather.isMetric) {
                temperature = temperature * 9. / 5. + 32.;  // fahrenheit
            }
            currentWeather.temperature = temperature;
            currentWeather.weatherConditionCode = 800;
        }
        weatherIntervalCounter = 0;
    } else {
        weatherIntervalCounter++;
    }
    return currentWeather;
}

float Watchy::getBatteryVoltage() {
    if (RTC.rtcType == DS3231) {
        return analogReadMilliVolts(BATT_ADC_PIN) / 1000.0f * 2.0f;  // Battery voltage goes through a 1/2 divider.
    } else {
        return analogReadMilliVolts(BATT_ADC_PIN) / 1000.0f * 2.0f;
    }
}

uint16_t Watchy::_readRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)address, (uint8_t)len);
    uint8_t i = 0;
    while (Wire.available()) {
        data[i++] = Wire.read();
    }
    return 0;
}

uint16_t Watchy::_writeRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(data, len);
    return (0 != Wire.endTransmission());
}

void Watchy::_bmaConfig() {
    if (sensor.begin(_readRegister, _writeRegister, delay) == false) {
        // fail to init BMA
        return;
    }

    // Accel parameter structure
    Acfg cfg;
    /*!
        Output data rate in Hz, Optional parameters:
            - BMA4_OUTPUT_DATA_RATE_0_78HZ
            - BMA4_OUTPUT_DATA_RATE_1_56HZ
            - BMA4_OUTPUT_DATA_RATE_3_12HZ
            - BMA4_OUTPUT_DATA_RATE_6_25HZ
            - BMA4_OUTPUT_DATA_RATE_12_5HZ
            - BMA4_OUTPUT_DATA_RATE_25HZ
            - BMA4_OUTPUT_DATA_RATE_50HZ
            - BMA4_OUTPUT_DATA_RATE_100HZ
            - BMA4_OUTPUT_DATA_RATE_200HZ
            - BMA4_OUTPUT_DATA_RATE_400HZ
            - BMA4_OUTPUT_DATA_RATE_800HZ
            - BMA4_OUTPUT_DATA_RATE_1600HZ
    */
    cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
    /*!
        G-range, Optional parameters:
            - BMA4_ACCEL_RANGE_2G
            - BMA4_ACCEL_RANGE_4G
            - BMA4_ACCEL_RANGE_8G
            - BMA4_ACCEL_RANGE_16G
    */
    cfg.range = BMA4_ACCEL_RANGE_2G;
    /*!
        Bandwidth parameter, determines filter configuration, Optional parameters:
            - BMA4_ACCEL_OSR4_AVG1
            - BMA4_ACCEL_OSR2_AVG2
            - BMA4_ACCEL_NORMAL_AVG4
            - BMA4_ACCEL_CIC_AVG8
            - BMA4_ACCEL_RES_AVG16
            - BMA4_ACCEL_RES_AVG32
            - BMA4_ACCEL_RES_AVG64
            - BMA4_ACCEL_RES_AVG128
    */
    cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;

    /*! Filter performance mode , Optional parameters:
        - BMA4_CIC_AVG_MODE
        - BMA4_CONTINUOUS_MODE
    */
    cfg.perf_mode = BMA4_CONTINUOUS_MODE;

    // Configure the BMA423 accelerometer
    sensor.setAccelConfig(cfg);

    // Enable BMA423 accelerometer
    // Warning : Need to use feature, you must first enable the accelerometer
    // Warning : Need to use feature, you must first enable the accelerometer
    sensor.enableAccel();

    struct bma4_int_pin_config config;
    config.edge_ctrl = BMA4_LEVEL_TRIGGER;
    config.lvl = BMA4_ACTIVE_HIGH;
    config.od = BMA4_PUSH_PULL;
    config.output_en = BMA4_OUTPUT_ENABLE;
    config.input_en = BMA4_INPUT_DISABLE;
    // The correct trigger interrupt needs to be configured as needed
    sensor.setINTPinConfig(config, BMA4_INTR1_MAP);

    struct bma423_axes_remap remap_data;
    remap_data.x_axis = 1;
    remap_data.x_axis_sign = 0xFF;
    remap_data.y_axis = 0;
    remap_data.y_axis_sign = 0xFF;
    remap_data.z_axis = 2;
    remap_data.z_axis_sign = 0xFF;
    // Need to raise the wrist function, need to set the correct axis
    sensor.setRemapAxes(&remap_data);

    // Enable BMA423 isStepCounter feature
    sensor.enableFeature(BMA423_STEP_CNTR, true);
    // Enable BMA423 isTilt feature
    sensor.enableFeature(BMA423_TILT, true);
    // Enable BMA423 isDoubleClick feature
    sensor.enableFeature(BMA423_WAKEUP, true);

    // Reset steps
    sensor.resetStepCounter();

    // Turn on feature interrupt
    sensor.enableStepCountInterrupt();
    sensor.enableTiltInterrupt();
    // It corresponds to isDoubleClick interrupt
    sensor.enableWakeupInterrupt();
}

void Watchy::setupWifi() {
    display.epd2.setBusyCallback(0);  // temporarily disable lightsleep on busy
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    wifiManager.setTimeout(WIFI_AP_TIMEOUT);
    wifiManager.setAPCallback(_configModeCallback);
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    if (!wifiManager.autoConnect(WIFI_AP_SSID)) {  // WiFi setup failed
        display.println("Setup failed &");
        display.println("timed out!");
    } else {
        display.println("Connected to");
        display.println(WiFi.SSID());
    }
    display.display(true);  // full refresh
    // turn off radios
    WiFi.mode(WIFI_OFF);
    btStop();
    display.epd2.setBusyCallback(displayBusyCallback);  // enable lightsleep on busy
    guiState = APP_STATE;
}

void Watchy::_configModeCallback(WiFiManager *myWiFiManager) {
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Connect to");
    display.print("SSID: ");
    display.println(WIFI_AP_SSID);
    display.print("IP: ");
    display.println(WiFi.softAPIP());
    display.display(true);  // full refresh
}

bool Watchy::connectWiFi() {
    if (WL_CONNECT_FAILED == WiFi.begin()) {  // WiFi not setup, you can also use hard coded credentials with WiFi.begin(SSID,PASS);
        WIFI_CONFIGURED = false;
    } else {
        if (WL_CONNECTED == WiFi.waitForConnectResult()) {  // attempt to connect for 10s
            WIFI_CONFIGURED = true;
        } else {  // connection failed, time out
            WIFI_CONFIGURED = false;
            // turn off radios
            WiFi.mode(WIFI_OFF);
            btStop();
        }
    }
    return WIFI_CONFIGURED;
}

void Watchy::showUpdateFW() {
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Please visit");
    display.println("watchy.sqfmi.com");
    display.println("with a Bluetooth");
    display.println("enabled device");
    display.println(" ");
    display.println("Press menu button");
    display.println("again when ready");
    display.println(" ");
    display.println("Keep USB powered");
    display.display(true);  // full refresh

    guiState = FW_UPDATE_STATE;
}

void Watchy::updateFWBegin() {
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Bluetooth Started");
    display.println(" ");
    display.println("Watchy BLE OTA");
    display.println(" ");
    display.println("Waiting for");
    display.println("connection...");
    display.display(true);  // full refresh

    BLE BT;
    BT.begin("Watchy BLE OTA");
    int prevStatus = -1;
    int currentStatus;

    while (1) {
        currentStatus = BT.updateStatus();
        if (prevStatus != currentStatus || prevStatus == 1) {
            if (currentStatus == 0) {
                display.setFullWindow();
                display.fillScreen(GxEPD_BLACK);
                display.setFont(&FreeMonoBold9pt7b);
                display.setTextColor(GxEPD_WHITE);
                display.setCursor(0, 30);
                display.println("BLE Connected!");
                display.println(" ");
                display.println("Waiting for");
                display.println("upload...");
                display.display(true);  // full refresh
            }
            if (currentStatus == 1) {
                display.setFullWindow();
                display.fillScreen(GxEPD_BLACK);
                display.setFont(&FreeMonoBold9pt7b);
                display.setTextColor(GxEPD_WHITE);
                display.setCursor(0, 30);
                display.println("Downloading");
                display.println("firmware:");
                display.println(" ");
                display.print(BT.howManyBytes());
                display.println(" bytes");
                display.display(true);  // partial refresh
            }
            if (currentStatus == 2) {
                display.setFullWindow();
                display.fillScreen(GxEPD_BLACK);
                display.setFont(&FreeMonoBold9pt7b);
                display.setTextColor(GxEPD_WHITE);
                display.setCursor(0, 30);
                display.println("Download");
                display.println("completed!");
                display.println(" ");
                display.println("Rebooting...");
                display.display(true);  // full refresh

                delay(2000);
                esp_restart();
            }
            if (currentStatus == 4) {
                display.setFullWindow();
                display.fillScreen(GxEPD_BLACK);
                display.setFont(&FreeMonoBold9pt7b);
                display.setTextColor(GxEPD_WHITE);
                display.setCursor(0, 30);
                display.println("BLE Disconnected!");
                display.println(" ");
                display.println("exiting...");
                display.display(true);  // full refresh
                delay(1000);
                break;
            }
            prevStatus = currentStatus;
        }
        delay(100);
    }

    // turn off radios
    WiFi.mode(WIFI_OFF);
    btStop();
    showMenu(menuIndex, true);
}

void Watchy::showSyncNTP() {
    tmElements_t previousTime;
    RTC.read(previousTime);
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&Bizcat_7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Syncing NTP... ");
    display.display(true);  // full refresh
    if (connectWiFi()) {
        if (syncNTP()) {
            display.println("NTP Sync Success\n");
            
            display.println("Previous time was:");
            display.printf("%02d:%02d:%02d\n", previousTime.Hour, previousTime.Minute, previousTime.Second);
            display.printf("%02d/%02d/%04d\n", previousTime.Day, previousTime.Month, tmYearToCalendar(previousTime.Year));

            RTC.read(currentTime);
            display.println("New time is:");
            display.printf("%02d:%02d:%02d\n", currentTime.Hour, currentTime.Minute, currentTime.Second);
            display.printf("%02d/%02d/%04d\n", currentTime.Day, currentTime.Month, tmYearToCalendar(currentTime.Year));
        } else {
            display.println("NTP Sync Failed");
        }
    } else {
        display.println("WiFi Not Configured");
    }
    display.display(true);  // full refresh
    delay(3000);
    showMenu(menuIndex, true);
}

bool Watchy::syncNTP() {  // NTP sync - call after connecting to WiFi and remember to turn it back off
    return syncNTP(settings.gmtOffset, settings.dstOffset, settings.ntpServer.c_str());
}

bool Watchy::syncNTP(long gmt, int dst, String ntpServer) {  // NTP sync - call after connecting to WiFi and remember to turn it back off
    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, ntpServer.c_str(), gmt);
    timeClient.begin();
    if (!timeClient.forceUpdate()) {
        return false;  // NTP sync failed
    }
    tmElements_t tm;
    breakTime((time_t)timeClient.getEpochTime(), tm);
    RTC.set(tm);
    return true;
}

// function will return total number of days
int Watchy::getLastDay(int month, int year) {
    // february and leap years
    if (month == 2) {
        if ((year % 400 == 0) || (year % 4 == 0 && year % 100 != 0))
            return 29;
        else
            return 28;
    }
    // months that have 31 days
    else if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12)
        return 31;
    else
        return 30;
}

void Watchy::drawCenteredString(const String &str, int x, int y, bool drawBg) {
    int16_t x1, y1;
    uint16_t w, h;

    display.getTextBounds(str, x, y, &x1, &y1, &w, &h);
    //          printf("bounds: %d x %d y, %d x1 %d y1, %d w, %d h\n", 0, 100, x1, y1, w, h);
    display.setCursor(x - w / 2, y);
    if (drawBg) {
        int padY = 3;
        int padX = 10;
        display.fillRect(x - (w / 2 + padX), y - (h + padY), w + padX * 2, h + padY * 2, GxEPD_BLACK);
    }
    // uncomment to draw bounding box
    //          display.drawRect(x - w / 2, y - h, w, h, GxEPD_WHITE);
    display.print(str);
}

uint8_t Watchy::getWeekday(uint8_t day, uint8_t month, uint16_t year){
    return (day + (13 * (month + 1) / 5) + (year) + (year / 4) - (year / 100) + (year / 400)) % 7;
}

void Watchy::addMinuteToChronograph(W_Chronograph *chronograph){
    if(!chronograph->isRunning){
        return;
    }

    if(chronograph->minutes == 59){
        chronograph->minutes = 0;
        if(chronograph->hours == 23){
            chronograph->hours = 0;
            chronograph->days++;
        } else {
            chronograph->hours++;
        }
    } else {
        chronograph->minutes++;
    }
}

void Watchy::resetChronograph(W_Chronograph *chronograph){
    chronograph->days = 0;
    chronograph->hours = 0;
    chronograph->minutes = 0;
}

void Watchy::decrementTimer(W_Timer *timer){
    if(!timer->isRunning){
        return;
    }

    //last minute
    if(timer->days == 0 && timer->hours == 0 && timer->minutes == 1){
        timer->minutes--;

        if (timer->willRepeat){
            timer->days = timer->original_days;
            timer->hours = timer->original_hours;
            timer->minutes = timer->original_minutes;
            timer->repetition_count++;
        } else {
            timer->isRunning = false;
        }
        
        showBuzz("Timer");
        return;
    }

    //otherwise
    if (timer->minutes == 0){
        if(timer->hours == 0){
            timer->hours = 23;
            timer->minutes = 59;
            timer->days--;
        } else {
            timer->minutes = 59;
            timer->hours--;
        }
    } else {
        timer->minutes--;
    }
}

void Watchy::setPETtoNow(W_PET *PET){
    RTC.read(currentTime);

    PET->hour = currentTime.Hour;
    PET->minute = currentTime.Minute;
    PET->day = currentTime.Day;
    PET->month = currentTime.Month;
    PET->year = tmYearToCalendar(currentTime.Year);
}

time_t Watchy::tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss){
    tmElements_t tmSet;
    tmSet.Year = YYYY - 1970;
    tmSet.Month = MM;
    tmSet.Day = DD;
    tmSet.Hour = hh;
    tmSet.Minute = mm;
    tmSet.Second = ss;
    return makeTime(tmSet);
}

uint16_t Watchy::secondsToDays(int32_t diff){
    return abs(diff)/86400;
}

uint8_t Watchy::secondsToHours(int32_t diff){
    return (abs(diff) - secondsToDays(diff)*86400)/3600;
}

uint8_t Watchy::secondsToMinutes(int32_t diff){
    return (abs(diff) - secondsToDays(diff)*86400 - secondsToHours(diff)*3600)/60;
}