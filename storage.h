#include <EEPROM.h>
#define byte uint8_t

#define DELAY_BEFORE_FAN_ON 0
#define FAN_WORK_DURATION 1
#define FAN_ON_SENSOR 2
#define DISPLAY_BRIGHTNESS 3
#define SCREENSAVER_ON 4

byte eepromGetDelayBeforeFanOnValue() {
  return EEPROM.read(DELAY_BEFORE_FAN_ON);
}
void eepromSaveDelayBeforeFanOnValue(byte v) {
  EEPROM.write(DELAY_BEFORE_FAN_ON, v);
}

byte eepromGetFanWorkDurationValue() {
  return EEPROM.read(FAN_WORK_DURATION);
}
void eepromSaveFanWorkDurationValue(byte v) {
  EEPROM.write(FAN_WORK_DURATION, v);
}

byte eepromGetFanOnSensorValue() {
  return EEPROM.read(FAN_ON_SENSOR);
}
void eepromSaveFanOnSensorValue(byte v) {
  EEPROM.write(FAN_ON_SENSOR, v);
}

byte eepromGetDisplayBrightnessValue() {
  return EEPROM.read(DISPLAY_BRIGHTNESS);
}
void eepromSaveDisplayBrightnessValue(byte v) {
  EEPROM.write(DISPLAY_BRIGHTNESS, v);
}

byte eepromGetScreensaverOn() {
  return EEPROM.read(SCREENSAVER_ON);
}
void eepromSaveScreensaverOn(byte v) {
  EEPROM.write(SCREENSAVER_ON, v);
}