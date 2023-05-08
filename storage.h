#include <EEPROM.h>
#define byte uint8_t

byte eepromGetDelayBeforeFanOnValue()
{
	return EEPROM.read(0);
}
void eepromSaveDelayBeforeFanOnValue(byte v)
{
	EEPROM.write(0, v);
}

byte eepromGetFanWorkDurationValue()
{
	return EEPROM.read(1);
}
void eepromSaveFanWorkDurationValue(byte v)
{
	EEPROM.write(1, v);
}

byte eepromGetFanOnSensorValue()
{
	return EEPROM.read(2);
}
void eepromSaveFanOnSensorValue(byte v)
{
	EEPROM.write(2, v);
}

byte eepromGetDisplayBrigtnessValue()
{
	return EEPROM.read(3);
}
void eepromSaveDisplayBrigtnessValue(byte v)
{
	EEPROM.write(3, v);
}