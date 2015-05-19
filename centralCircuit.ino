//TODO get real time from computer and implement humidity + temp snesors
#include "Circuit.h"
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Time.h>  
#include <Wire.h>  
#include <DS1307RTC.h> //for RTC clock
#include <DHT11.h> //for temp and humidity sensor
//*************************DEFINES************************
#define UP buttonState >= 73 && buttonState < 239
#define DOWN buttonState >= 239 && buttonState < 419
#define SELECT buttonState >= 700 && buttonState < 800
#define RIGHT  buttonState >= 0 && buttonState < 100
#define LEFT   buttonState >= 470 && buttonState < 600
#define MAIN   0
#define ON 1
#define OFF 2
#define YOMTOV 1
#define SHABBOS 6
#define NUMCIRCS 6
#define TRANSFORMER A3  //on only when an AC circuit needs to be on
#define YTTIME NUMCIRCS //this is the holiday pointer 
#define YTREQUEST 7
#define DHTLIB_OK 0
#define DHTPIN 2  //pin for DHT pin
#define BYTES_PER_SET        (BYTES_PER_CIRCUIT * NUM_DAYS)

//*****************************GLOVBALS********************************8
//Initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); //Create an lcd object and assign the pins
//used for time display control
int buttonState = 0;  //analog state of button determines if pressed
char cursorMarker = 0; //where cursor lies
int8_t scrollControl = 0;  //main menu counter
byte prevMarker = 0;
byte seconds;  
//variables to get 1 second refresh
byte lastDisplayed = second();
bool canDisplay = true;
byte lastChecked = second();  //to get 1 second circuit checking

byte TimeDiff = 0; //determines when system time is displayed
bool canSelect = false; //tells whether I can use the Select button
byte today = 0;  //tells the system which day of the week it is 0 = sunday 6 = Shabbos
//used for yomtov mode
char YTstart = -1; //day on which Yom Tov starts -1 means no day set
byte holidays = 0; //determines how many days of Yom Tov to account for
bool isErev = false; //this tells the system whether to use YOMTOV mode or regular mode on the day set for erev YOmtov
bool updatedNumHolidays = false; //prevents system from updating holiidays counter more than once
//for humidity/temp sensing
DHT11 humTemp(DHTPIN);  //initialize a dht11 on DHTPIN 
int tempThresh = 0;
int humThresh = 0;
bool transOn = false; //transformer control

Circuit* circuits[NUMCIRCS + 1]; //holds the circuits
char *days[8] = {"S","M","T","W","R","F","SH","YT"};

/*! sets up the circuits
/* @note this function is set to a specific device, would have to reimplement for new instance
*/
void setCircuits()
{
  circuits[0] = new Circuit(F("Time"),0,false,0);   //construct a circuit object connected to physical pin 27
  circuits[1] = new Circuit(F("AC1"),3,true,1 * BYTES_PER_SET);
  circuits[2] = new Circuit(F("AC2"),10,true,2 * BYTES_PER_SET);
  circuits[3] = new Circuit(F("AC3"),11,true,3 * BYTES_PER_SET);
  circuits[4] = new Circuit(F("LIGHT1"),13,false,4 * BYTES_PER_SET);
  circuits[5] = new Circuit(F("LIGHT2"),A1,false,5 * BYTES_PER_SET);
  circuits[6] = new Circuit(F("Holiday"),0,false,6 * BYTES_PER_SET);
}

/*! threshold set screen
/* @param thresh the current thresh set
/* @param toSet a reference to either the humidity threshold  or the temperature threshhold
*/
void refreshThreshScreen(int thresh, int *toSet)
{
  lcd.clear();
  lcd.setCursor(0,0);
  if (toSet == &tempThresh)
  {
    lcd.print(F("set Temperature"));
  }
  else if(toSet == &humThresh)
  {
    lcd.print(F("set Humidity"));
  }
    lcd.setCursor(0,1);
    lcd.print(thresh);
    lcd.setCursor(0,1);
}

/*! sets the threshold of either the temperature or the humidity
/* @param toSet a reference to either the humidity threshold  or the temperature threshhold
*/
void setThresh(int *toSet)
{
  int tempValue;
  if (toSet == &tempThresh)
  {
    tempValue = tempThresh;
  }
  else if(toSet == &humThresh)
  {
    tempValue = humThresh;
  }
  refreshThreshScreen(tempValue,toSet);
  clearSELECT();
  while(!(SELECT))
  {
    buttonState = analogRead(A0);
    if (UP)
    {
      tempValue = increment(tempValue,110);
      lcd.print(tempValue);
      lcd.setCursor(0,1);
      refreshThreshScreen(tempValue,toSet);
      while (UP)
      {
        buttonState = analogRead(A0);
      }
    }
     if (DOWN)
    {
      tempValue = decrement(tempValue,110);
       lcd.print(tempValue);
       lcd.setCursor(0,1);
       refreshThreshScreen(tempValue,toSet);
      while (DOWN)
      {
        buttonState = analogRead(A0);
      }
    }
  }
  clearSELECT();
  *toSet = tempValue;
}
void setDay(byte mode)
{
  byte cursor = today * 2;  //set cursor to today's spot
  lcd.clear();
  lcd.setCursor(0,0);
  if (mode == MAIN)
    lcd.print(F("set day of week"));
  if (mode == YOMTOV)
    lcd.print(F("start day?"));
  lcd.setCursor(0,1);
  lcd.print(F("S M T W R F SH"));
  lcd.setCursor(cursor,1);
  clearSELECT();
  while(!(SELECT))
  {
    buttonState = analogRead(A0);
    if (RIGHT)
    {
      cursor = increment(cursor + 1,13);
      lcd.setCursor(cursor,1);
      while (!digitalRead(A0));
    }
    if (LEFT)
    {
      if ((cursor = decrement(cursor - 1, 13)) == 13)
        cursor = 12;
      lcd.setCursor(cursor,1);
       while(LEFT)
        {
                  buttonState = analogRead(A0);
         }
    }
  }
  if (mode == MAIN)
  today = cursor / 2;
  if (mode == YOMTOV)
  {
    YTstart = cursor / 2;
    getNumDays();
    isErev = true;
  }
  clearSELECT();
}
void setTimeRef(byte hours, byte mins, byte secs, byte mode,byte position)
{
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(F("set "));
    if (mode == MAIN)
    {
      lcd.print(F("system time"));
    }
    else if (mode == ON)
    {
      lcd.print(days[position]);
      if (position != YTREQUEST)  //check if this is a Yomtov set
        lcd.print(F(" on time"));
       else
         lcd.print(F(" start"));
    }
    else if (mode == OFF)
    {
      lcd.print(days[position]);
      if (position != YTREQUEST)  //check if this is a YomTov set
        lcd.print(F(" off time"));
      else
        lcd.print(F(" end"));
    }
    lcd.setCursor(0,1);
    printDigits(hours);
    lcd.print(F(":"));
    printDigits(mins);
    lcd.print(F(":"));
    printDigits(secs);
}

void setTimeScreen(Circuit *circ,byte mode,byte position)
{
    byte hours,mins,secs = 0,cursorPos = 1;
    if (mode == MAIN)
    {
      hours = hour();
      mins = minute();
      secs = second();
    }
    else if (mode == ON)
    {
      if (position != YTREQUEST)
      {
        hours = circ->onTimes[position]->hour;
        mins = circ->onTimes[position]->minute;
       // secs = circ->onTimes[position]->second;
      }
      else
      {
        hours = circ->onTimes[0]->hour;
        mins = circ->onTimes[0]->minute;
        //secs = circ->onTimes[0]->second;
      }
    }
    else if (mode == OFF)
    {
      if (position != YTREQUEST)
      {
        hours = circ->offTimes[position]->hour;
        mins = circ->offTimes[position]->minute;
        //secs = circ->offTimes[position]->second;
      }
      else
      {
         hours = circ->offTimes[0]->hour;
        mins = circ->offTimes[0]->minute;
       // secs = circ->offTimes[0]->second;
      }
    }
    lcd.blink();
    setTimeRef(hours,mins,secs,mode,position);
    lcd.setCursor(cursorPos,1);
    clearSELECT();
  while (!(SELECT))
  {
    buttonState = analogRead(A0);
    if (RIGHT)
    {
      if((cursorPos = increment(cursorPos + 2,7)) == 2)
        cursorPos = 1;
      lcd.setCursor(cursorPos,1);
      while (!digitalRead(A0));
    }
     if (LEFT)
     {
        cursorPos = decrement(cursorPos - 2,7);
        lcd.setCursor(cursorPos,1);
        while(LEFT)
        {
                  buttonState = analogRead(A0);
         }
     }
     if (UP)
     {
       if (cursorPos == 1)
       {
         hours = increment(hours,23);
       }
       else if (cursorPos == 4)
       {
         mins = increment(mins,59);
       }
       else if (cursorPos == 7)
       {
         secs = increment(secs,59);
       }
       setTimeRef(hours,mins,secs,mode,position);
       lcd.setCursor(cursorPos,1);
        while(UP)
        {
                  buttonState = analogRead(A0);
         }
     }
          if (DOWN)
     {
       if (cursorPos == 1)
       {
         hours = decrement(hours,23);
       }
       else if (cursorPos == 4)
       {
         mins = decrement(mins,59);
       }
       else if (cursorPos == 7)
       {
         secs = decrement(secs,59);
       }
       setTimeRef(hours,mins,secs,mode,position);
       lcd.setCursor(cursorPos,1);
        while(DOWN)
        {
                  buttonState = analogRead(A0);
         }
     }
  }
  if (mode == MAIN)
    setTime(hours,mins,secs,59,0,7);
   else if (mode == ON)
   {
     if (position != YTREQUEST)
     {
       circ->onTimes[position]->hour = hours;
       circ->onTimes[position]->minute = mins;
       //backup data to EEPROM
       EEPROM.write(((BYTES_PER_SET * scrollControl) + (BYTES_PER_CIRCUIT * position)),circ->onTimes[position]->hour);
       EEPROM.write(((BYTES_PER_SET * scrollControl) + (BYTES_PER_CIRCUIT * position)) + 1,circ->onTimes[position]->minute);
      // circ->onTimes[position]->second = secs;
     }
     else
     {
       circ->onTimes[0]->hour = hours;
       circ->onTimes[0]->minute = mins;
      // circ->onTimes[0]->second = secs;
     }
   }
   else if (mode == OFF)
   {
     if (position != YTREQUEST)
     {
       circ->offTimes[position]->hour = hours;
       circ->offTimes[position]->minute = mins;
       //backup data to EEPROM
       EEPROM.write(((BYTES_PER_SET * scrollControl) + (BYTES_PER_CIRCUIT * position)) + 2,circ->offTimes[position]->hour);
       EEPROM.write((((BYTES_PER_SET * scrollControl)) + (BYTES_PER_CIRCUIT * position)) + 3,circ->offTimes[position]->minute);
       //circ->offTimes[position]->second = secs;
     }
     else
     {
       circ->offTimes[0]->hour = hours;
       circ->offTimes[0]->minute = mins;
      // circ->offTimes[0]->second = secs;
     }
   }
   clearSELECT();
}

void getNumDays()
{
  byte cursor = 0;
  clearSELECT();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("how many days?")); //the F() puts the constant string into prgmspace saving ram
  lcd.setCursor(0,1);
  lcd.print(F("1 2 3"));
  lcd.setCursor(cursor,1);
  while (!(SELECT))
  {
    buttonState = analogRead(A0);
    if (RIGHT)
    {
      if ((cursor = increment(cursor + 1,5)) == 5)
        cursor = 0;
      lcd.setCursor(cursor,1);
       while(RIGHT)
        {
                  buttonState = analogRead(A0);
         }
    }
    if (LEFT)
    {
      cursor = decrement(cursor - 1,4);
      lcd.setCursor(cursor,1);
       while(LEFT)
        {
                  buttonState = analogRead(A0);
         }
    }
  }
    holidays = (cursor / 2) + 2; //an extra day added for the first night
    clearSELECT();
}

void displayrefresh(){
      if ((DOWN && prevMarker == 1) || (canDisplay && cursorMarker == 1))
      {
        if ((DOWN && prevMarker == 1) && canDisplay)
        {
          scrollControl = decrement(scrollControl, NUMCIRCS);
        }
        //print the current menu position on bottom and one before it on top
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print(circuits[scrollControl]->name);
        lcd.setCursor(0,0);
        lcd.print(circuits[decrement(scrollControl, NUMCIRCS)]->name);
      }
      if (UP && prevMarker == 0 || (canDisplay && cursorMarker == 0))
      {
        if ((UP && prevMarker == 0) && canDisplay) 
        {
          scrollControl = increment(scrollControl, NUMCIRCS);
        }
        //print the current mewnu position on top and one after it on bottom
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(circuits[scrollControl]->name);
        lcd.setCursor(0,1);
        lcd.print(circuits[increment(scrollControl, NUMCIRCS)]->name);
      }
       lcd.setCursor(0,cursorMarker);  //this happens whether menu is fully updating or in between
       delay(500); //prevents loop from running multiple times when you rapidly press buttons
       lcd.blink();
       prevMarker = cursorMarker;
}

void showTime()
{
  if (second() != lastDisplayed)
  {
    digitalClockDisplay();
    lastDisplayed = second();
  }
}

void digitalClockDisplay(){
  // digital clock display of the time
  lcd.clear();
  lcd.setCursor(0,0);  //print current time on top
  printDigits(hour());
  lcd.print(F(":"));
  printDigits(minute());
  lcd.print(F(":"));
  printDigits(second());
  lcd.setCursor(0,1);  //print current date on bottom
  if (today == YTstart && !isErev)
  {
    lcd.print(F("YOM TOV"));
  }
  else
  {
    lcd.print(F("S M T W R F SH"));
  }
  lcd.noBlink();
  if (today != YTstart || (today == YTstart && isErev))
    lcd.cursor();
      //increment day counter if it is midnight
    if (hour() == 0 && minute() == 0 && second() == 0)
    {
      today = increment(today,6);
      if (YTstart != -1)
          YTstart = increment(YTstart,6);  //set YomTov day to current Real Time day
    }
                // check Yom Tov mode and set data for that
      if (YTstart != -1 && today == YTstart)
      {
        if (circuits[YTTIME]->onTimes[0]->hour == hour() && circuits[YTTIME]->onTimes[0]->minute == minute())  //if the start of YOmTov has bween reached
        {
          isErev = false; //it is now YomTov
        }
        if (!updatedNumHolidays && circuits[YTTIME]->offTimes[0]->hour == hour() && circuits[YTTIME]->offTimes[0]->minute == minute())  //if the end of Yom Tov has been reached
        {
          holidays--;
          if (holidays == 0)
          {
            YTstart = -1;
          }
          updatedNumHolidays = true;
        }
        if (updatedNumHolidays && minute() == increment(circuits[YTTIME]->offTimes[0]->minute,59))  //reset for tommorow
        {
          updatedNumHolidays = false;
        }
      }
    lcd.setCursor(2 * today,1);
}
/*! prints values to LCD screen
/* @param digits the value to be printed 
*/
void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  if(digits < 10)
    lcd.print('0');
  lcd.print(digits);
}

/*! increments a value so that it rolls back to 0 when it exceeds a max value
/*  @param value the value to increment
/*  @param maxi the maximum value before reseting to 0
*/
int8_t increment(int8_t value, byte maxi)
{
  return (value + 1) % (maxi + 1);
}

/*!decrements given value and loops around if below 0
/*  @param value the value to increment
/*  @param maxi the maximum value before reseting to 0
*/
int8_t decrement(int8_t value, byte maxi)
{
  value--;
  if(value < 0)
  {
    value = maxi;
  }
  return value;
}

/*! converts Celsius to Fahrenheit 
/* @param celcius the temperature to be converted
*/
double Fahrenheit(double celsius)
{
	return 1.8 * celsius + 32;
}

/*! clears the SELECT button 
*/

void clearSELECT()
{
    while(SELECT)
  {
     buttonState = analogRead(A0);
  }
}

/*! handler for switching on and off the circuits
*/
void checkCircuits()
{
  if (second() != lastChecked)  //if 1 second has past check circuits otherwise dont
  {
    float humidity,temperature;  //the read function will fill these values
    int dhtResult = humTemp.read(humidity,temperature);  //read the sensor 
    temperature = (int)Fahrenheit((double)temperature);  //convert to fahrenheit
    for (byte i = 1; i < NUMCIRCS; i++)
    {
      //handle cases for tempuature and humidity sensor (only applies to an AC circuit)
      if (circuits[i]->isAC && dhtResult == DHTLIB_OK && circuits[i]->isOn&& (int)temperature < tempThresh && (int)humidity < humThresh)  //check if circuit is to be turned off temporarily because too hot 
      {
        circuits[i]->shouldBeOn = true;
        pinMode(circuits[i]->physicalCircuit,LOW);
        continue; 
      }
      if (circuits[i]->isAC && dhtResult == DHTLIB_OK && circuits[i]->shouldBeOn && (int)temperature >= (tempThresh + 2) && (int)humidity >= (humThresh + 2))  //check whether circuit that is turned off temporarily should be turned on again
      {
        circuits[i]->shouldBeOn = false;
        pinMode(circuits[i]->physicalCircuit,HIGH);
        continue; 
      }
      if (today != YTstart  || (today == YTstart && isErev))
      {
        if (circuits[i]->onTimes[today]->hour == circuits[i]->offTimes[today]->hour &&  circuits[i]->onTimes[today]->minute == circuits[i]->offTimes[today]->minute)  //if the start and end time are equal
          continue; //do nothing
        if (circuits[i]->onTimes[today]->hour == hour() &&  circuits[i]->onTimes[today]->minute == minute() && circuits[i]->isOn == false)
        {
          circuits[i]->isOn = true;
          pinMode(circuits[i]->physicalCircuit,HIGH);
        }
        if (circuits[i]->offTimes[today]->hour == hour() &&  circuits[i]->offTimes[today]->minute == minute() && circuits[i]->isOn == true)
        {
          circuits[i]->isOn = false;
          pinMode(circuits[i]->physicalCircuit,LOW);
        }
      }
      else
      {
        if (circuits[i]->onTimes[SHABBOS]->hour == circuits[i]->offTimes[SHABBOS]->hour &&  circuits[i]->onTimes[SHABBOS]->minute == circuits[i]->offTimes[SHABBOS]->minute)  //if the start and end time are equal
          continue; //do nothing
        if (circuits[i]->onTimes[SHABBOS]->hour == hour() &&  circuits[i]->onTimes[SHABBOS]->minute == minute() && circuits[i]->isOn == false)
        {
          circuits[i]->isOn = true;
          pinMode(circuits[i]->physicalCircuit,HIGH);
        }
        if (circuits[i]->offTimes[SHABBOS]->hour == hour() &&  circuits[i]->offTimes[SHABBOS]->minute == minute()&& circuits[i]->isOn == true)
        {
          circuits[i]->isOn = false;
          pinMode(circuits[i]->physicalCircuit,LOW);
        }
      }
    }
    lastChecked = second();
    }
}

/*! function decides if transformer should be on
*/
bool checkTransformer()
{
  //iterate through circuits to see if an AC needs power
  for (byte i = 1; i < NUMCIRCS; i++)
  {
      if (circuits[i]->isAC && circuits[i]->isOn && circuits[i]->shouldBeOn)
        return true;
  }
  return false;
}
void setup(){
  lcd.begin(16, 2);
  //Serial.begin(9600);
  setCircuits();
  setSyncProvider(RTC.get);
  setTimeScreen(NULL,MAIN,7);
  setDay(MAIN);
  setThresh(&tempThresh);
  setThresh(&humThresh);
  seconds = second();
    
}
  
void loop(){
        // read the pushbutton input pin:
  buttonState = analogRead(A0);
    if (UP) {
      lcd.noCursor();
      cursorMarker = 0;
      scrollControl = decrement(scrollControl, NUMCIRCS);
      seconds = second();
      displayrefresh();
      canDisplay = false;
      lastDisplayed = 0;
      canSelect = true;
    }
    if (DOWN) {
       // if button is pressed down 
      //buttonPushCounter++;
      lcd.noCursor();
      cursorMarker = 1;
      scrollControl = increment(scrollControl, NUMCIRCS);
      seconds = second();
      displayrefresh();
      canDisplay = false;
      lastDisplayed = 0;
      canSelect = true;
   }    
      TimeDiff = second() - seconds;
  if (SELECT && canSelect)
  {
         clearSELECT();  //clear previous SELECT input
    if (scrollControl == 0)
   {
          setTimeScreen(NULL,MAIN,7);
          setDay(MAIN);
          setThresh(&tempThresh);
          setThresh(&humThresh);
   }
    else if (scrollControl > 0 && scrollControl < NUMCIRCS)
    {
      for(byte i = 0; i < 7; i++)
     { 
      setTimeScreen(circuits[scrollControl],ON,i);
      setTimeScreen(circuits[scrollControl],OFF,i);
     }
    }
    else  //TODO implement for NUMCIRCS 
    {
      setDay(YOMTOV);
      setTimeScreen(circuits[scrollControl],ON,YTREQUEST);  //set start time of yomtov
      setTimeScreen(circuits[scrollControl],OFF,YTREQUEST); //set end time of yomtov
    }
         TimeDiff = 6;  //allow display of time
  } 
   if (TimeDiff < 0)
   {
     TimeDiff = 0;
   }
   if (TimeDiff > 5)
   {
     canDisplay = true;
   }
   if (canDisplay)
   {
     showTime();
     canSelect = false;
   }
   checkCircuits();
   bool ACneedsPower = checkTransformer(); 
   if (ACneedsPower && !transOn)
   {
      pinMode(TRANSFORMER,HIGH);
      transOn = true;
   }
   if(!ACneedsPower)
   {
     pinMode(TRANSFORMER,LOW);
      transOn = false;
   }
} 

