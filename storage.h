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

byte eepromGetFanWorkTimeValue()
{
	return EEPROM.read(1);
}
void eepromSaveFanWorkTimeValue(byte v)
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
