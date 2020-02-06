#include <EEPROM.h>
#define byte uint8_t

byte eepromGetDelayValueAfterLightOn()
{
	return EEPROM.read(0);
}
void eepromSaveDelayValueAfterLightOn(byte v)
{
	EEPROM.write(0, v);
}

byte eepromGetDelayValueAfterLightOff()
{
	return EEPROM.read(1);
}
void eepromSaveDelayValueAfterLightOff(byte v)
{
	EEPROM.write(1, v);
}

byte eepromGetLightThresholdValue()
{
	return EEPROM.read(2);
}
void eepromSaveLightThresholdValue(byte v)
{
	EEPROM.write(2, v);
}
