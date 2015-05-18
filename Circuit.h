#include <Arduino.h>
#include <EEPROM.h>

#define NUM_DAYS  7
#define BYTES_PER_CIRCUIT    4
struct Time
{
    uint8_t minute;
    uint8_t hour;
   // char second;
    Time(int EEaddr)
    {
      hour = EEPROM.read(EEaddr);
      if(hour > 59)  //check for valid value
        hour = 0;
      minute = EEPROM.read(EEaddr + 1);
      if(minute > 59)  //check for valid value
        minute = 0;   
    //  second = 0;
    }
};
struct Circuit
{
	const __FlashStringHelper* name;  //stores name in pgmspace
        bool isOn;
        bool shouldBeOn;
        bool isAC;
        char physicalCircuit;
        Time *onTimes[7];   //0 = sunday 6 = Shabbos
        Time *offTimes[7];   
	Circuit(const __FlashStringHelper* n,char circNum,bool iA,int EEaddr)
	{
	  name = n;
           for (char i = 0; i < NUM_DAYS; i++)
           {
              onTimes[i] = new Time(EEaddr + (i * BYTES_PER_CIRCUIT));
              offTimes[i] = new Time((EEaddr + (i * BYTES_PER_CIRCUIT)) + 2);   //each address stores 2 bytes
           }
          isOn = false;
          isAC = iA;
          shouldBeOn = false;
          physicalCircuit = circNum;
	}
};
