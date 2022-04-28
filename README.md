# About this repo

This is a watchface for the Watchy open source smart watch : https://watchy.sqfmi.com/

## Compilation instructions
Compiled using PlatformIO.
The necessary libraries are listed in the platformio.ini file.

# Features

## Watchface
I simply used the Starry Horizon watch face and built from it.
https://github.com/sqfmi/Watchy/tree/master/examples/WatchFaces/StarryHorizon
You can change the watchface by editing the drawWatchFace method.

## World Time
Three cities can be displayed.
You can choose the cities in a list and specify if DST is on in the city or not.

![World Time](/readme_assets/wtime.png "World Time")

![World Time](/readme_assets/wtimeset.png "Set World Time")

## Chronograph
Chronograph that counts days, hours and minutes.
The "Time after return" indicator shows what time it would be if the current chronograph amount was added to the current time.
This is useful if you are on a hike, and want to know what time it would be if you turned back around right now and went back to your car.

![Chronograph](/readme_assets/chrono.png "Chronograph")

## Timers
Five different timers, that can repeat if necessary.
Cycle through timers with the down button and start/pause a timer with the up button.

![Timer](/readme_assets/timer.png "Timer")

![Timer Set](/readme_assets/timerset.png "Timer Set")

## Alarms
Five different alarms, that can either set up daily (with weekday selection), or on a specific day and repeat weekly, monthly, yearly. (they can also occur only once)
Cycle through alarms with the down button and toggle an alarm on and off with the up button.

![Alarm](/readme_assets/alarm.png "Alarm")

![Alarm Set](/readme_assets/alarmset.png "Alarm Set")

## PET (Phase Elapsed Time)
Set a time and date, and the PET will tell you how many days/hours:second remaining until that date.
Then, it will count up from that time and date.
Time and date is marked negative if in the future and positive if in the past.
Cycle through PETs with the up or down buttons.

![PET](/readme_assets/PET.png "PET")

## About the button behavior
When the watch is awake, the button behavior is different than the base Watchy.
You can stack button presses, and if the buttons have not been touched for more than 250ms, the display is updated.
This allows to mash the up and down buttons when setting up alarms and timers, so that you don't have to wait for the display to update at each value update.

## Roadmap

- present alerts nicely and choose the type of buzz. For now, they are just a hack of the showBuzz function.
- maybe add reminders that can alert a certain amount of time before a PET reaches 0/00:00

## Acknowledgements

Thank you @robey for providing the fonts: https://code.lag.net/robey/nuwatchy/src/branch/main/fonts