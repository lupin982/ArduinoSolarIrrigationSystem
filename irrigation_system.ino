#include <LiquidCrystal.h>
#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <RTC_DS3231.h>
#include <LowPower.h>

// if STANDALONE is defined will be used RTC otherwise will be used RTC_DS3231
//#define STANDALONE 1

// CONNECTIONS:
// DS3231 SDA --> A4    Don't forget to pullup (4.7k to VCC)
// DS3231 SCL --> A5    Don't forget to pullup (4.7k to VCC)

//
#define NUM_STATES				5
//#define STATE_OFF 0
#define STATE_ON				0
#define STATE_SET_TIME			1
#define STATE_SET_START_TIME	2
#define STATE_SET_DURATION		3
#define STATE_OUT_ON			4

#define STANDBY_DELAY_MS	10000

LiquidCrystal lcd(11, 12, 7, 8, 9, 10);

//Global variable
const int relayPin = 6;
//const int buttonStatePin = 11;
const int buttonIncreaseHourPin = 4;
const int buttonIncreaseMinutePin = 5;
// Use pin 2 as wake up pin and change state pin
const int buttonChangeStatePin = 2;

volatile int state = 0;
int relay_state = 0;
long lastDebounceTimeState = 0;
long lastDebounceTimeIncreaseHour = 0;
long lastDebounceTimeIncreaseMin = 0;
long lastDebounceChangeTime = 0;

int internal_state = STATE_ON;

#ifdef STANDALONE
	RTC_Millis RTC;
#else
	RTC_DS3231 RTC;
#endif

// support for read date
DateTime currentDateTimeRead;

// start irrigation variables
int start_hour = 19;
int start_minute = 0;

// irrigation duration variables
int duration_hours = 0;
int duration_minutes = 30;

// support for string of the lcd
char lcd_string[16];

// time used to control if the system is in standby
unsigned long standby_time;

// if true the system is in standby
bool standby_state = false;

// used for STATE_OUT_ON state
bool state_on_enabled = false;

// function called on standby exit
void wakeUp()
{
	// Just a handler for the pin interrupt.
	//state = NUM_STATES - 1;
	standby_state = false;
	standby_time = millis();
	lcd.display();
	detachInterrupt(0);
}

void setup(void)
{
	// initialize LCD
	lcd.begin(16,2);                //define the columns (16) and rows (2)
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print(" ");
	lcd.setCursor(0,1);
	lcd.print(" ");
	
	sprintf(lcd_string, "");

	//--------RTC SETUP ------------
	
	
	#ifndef STANDALONE
		Wire.begin();
		RTC.begin();
	// set the correct time
		if (! RTC.isrunning()) {
			lcd.print(" RTC is NOT run");
			// following line sets the RTC to the date & time this sketch was compiled
			RTC.adjust(DateTime(__DATE__, __TIME__));
		}
	#endif
	
	DateTime now = RTC.now();
	DateTime compiled = DateTime(__DATE__, __TIME__);
	if (now.unixtime() < compiled.unixtime()) {
		//Serial.println("RTC is older than compile time!  Updating");
		RTC.adjust(DateTime(__DATE__, __TIME__));
	}

	// set pin in out
	pinMode(relayPin, OUTPUT);

	pinMode(buttonIncreaseHourPin, INPUT);
	pinMode(buttonIncreaseMinutePin, INPUT);
	pinMode(buttonChangeStatePin, INPUT);

	standby_time = millis();
}

void loop(void)
{
	// read the state button
	int buttonChangeState = digitalRead(buttonChangeStatePin);
	
	if(buttonChangeState == HIGH)
	{
		//debouncing
		if((millis() - lastDebounceChangeTime) > 100)
		{
			changeState();
		}
		lastDebounceChangeTime = millis();
	}
	
	// check the state, this is the system processing code
	switch(state)
	{
		int buttonInscreaseHourState;
		int buttonInscreaseMinuteState;
		
		case STATE_ON:
		
		
			if(standby_state)
			{
				// Enter power down state for 8 s with ADC and BOD module disabled
				#ifndef STANDALONE
					LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
				#endif

			}
		
			// check for standby
			if((millis() - standby_time) > STANDBY_DELAY_MS)
			{
				lcd.noDisplay();
				standby_state = true;
				// Allow wake up pin to trigger interrupt on low.
				attachInterrupt(0, wakeUp, CHANGE );
			}
			else
			{
				lcd.display();
				standby_state = false;
			}
		
			// read the current time
			currentDateTimeRead = RTC.now();
			
			int mins, hours;
			// set the time elapsed from the start of irrigation
			hours = currentDateTimeRead.hour() - start_hour;
			mins = currentDateTimeRead.minute() - start_minute;
			
			
			
			// variable for the line 1 of the LCD
			char lcd_string1[16];
		
			// check if enable or not the irrigation
			if(((hours <= duration_hours) && (hours >= 0)) && ((mins < duration_minutes) && (mins >= 0)))
			{	
				digitalWrite(relayPin, LOW);
				standby_time = millis();
				sprintf(lcd_string, " %02d:%02d -- ON    ", RTC.now().hour(), RTC.now().minute());
			}
			else
			{
				digitalWrite(relayPin, HIGH);
				sprintf(lcd_string, " %02d:%02d -- OFF   ", RTC.now().hour(), RTC.now().minute());
			}
			
			// write the lcd only if the system is not in the standby
			if(!standby_state)
			{
				lcd.setCursor(0,0);
				lcd.print(lcd_string);
				sprintf(lcd_string1, " start:%02d:%02d", start_hour, start_minute);
				lcd.setCursor(0,1);
				lcd.print(lcd_string1);
			}
		
		break;
		case STATE_SET_TIME:
			// set LCD information
			lcd.setCursor(0,0);
			lcd.print(" Set Time:       ");
		
			//turn off relay
			digitalWrite(relayPin, HIGH);
			//read the hour button state
			buttonInscreaseHourState = digitalRead(buttonIncreaseHourPin);
		
			if(buttonInscreaseHourState == HIGH)
			{
				//debouncing
				if((millis() - lastDebounceTimeIncreaseHour) > 100)
				{
					// increase the current time of 1 hour
					DateTime currentDateTimeSet(RTC.now().unixtime() + 3600);
					RTC.adjust(currentDateTimeSet);
				
				}
				lastDebounceTimeIncreaseHour = millis();
			}
		
			// read the minute button state
			buttonInscreaseMinuteState = digitalRead(buttonIncreaseMinutePin);
		
			if(buttonInscreaseMinuteState == HIGH)
			{
				//debouncing
				if((millis() - lastDebounceTimeIncreaseMin) > 100)
				{
					// increase the current time of 1 minute
					DateTime currentDateTimeSet(RTC.now().unixtime() + 60);
					RTC.adjust(currentDateTimeSet);
				}
				lastDebounceTimeIncreaseMin = millis();
			}
			
			// set the LCD information
			sprintf(lcd_string, " -- %02d:%02d --", RTC.now().hour(), RTC.now().minute());
			lcd.setCursor(0,1);
			lcd.print(lcd_string);
			standby_time = millis();
			delay(50);
		break;
		case STATE_SET_START_TIME:
			lcd.setCursor(0,0);
			lcd.print(" Set Start Time: ");
		
			//turn off relay
			digitalWrite(relayPin, HIGH);
			//read the hour button state
			buttonInscreaseHourState = digitalRead(buttonIncreaseHourPin);
		
			if(buttonInscreaseHourState == HIGH)
			{
				//debouncing
				if((millis() - lastDebounceTimeIncreaseHour) > 100)
				{
					// increase start hour
					start_hour = (start_hour + 1) % 24;
				}
				lastDebounceTimeIncreaseHour = millis();
			}
		
			//read the minute button state
			buttonInscreaseMinuteState = digitalRead(buttonIncreaseMinutePin);
		
			if(buttonInscreaseMinuteState == HIGH)
			{
				//debouncing
				if((millis() - lastDebounceTimeIncreaseMin) > 100)
				{
					// increase start minute
					start_minute = (start_minute + 1) % 60;
				}
				lastDebounceTimeIncreaseMin = millis();
			}
			
			// set the info on the LCD
			sprintf(lcd_string, " -- %02d:%02d --", start_hour, start_minute);
			lcd.setCursor(0,1);
			lcd.print(lcd_string);
			standby_time = millis();
			delay(50);
		
		break;
		case STATE_SET_DURATION:
			lcd.setCursor(0,0);
			lcd.print(" Set Duration:   ");
		
			//turn off relay
			digitalWrite(relayPin, HIGH);
			//read the hour button state
			buttonInscreaseHourState = digitalRead(buttonIncreaseHourPin);
		
			if(buttonInscreaseHourState == HIGH)
			{
				//debouncing
				if((millis() - lastDebounceTimeIncreaseHour) > 100)
				{
					// increase hours duration
					duration_hours = (duration_hours + 1) % 24;
				}
				lastDebounceTimeIncreaseHour = millis();
			}
		
			//read the minute button state
			buttonInscreaseMinuteState = digitalRead(buttonIncreaseMinutePin);
		
			if(buttonInscreaseMinuteState == HIGH)
			{
				//debouncing
				if((millis() - lastDebounceTimeIncreaseMin) > 100)
				{
					// increase minutes duration
					duration_minutes = (duration_minutes + 1) % 60;
				}
				lastDebounceTimeIncreaseMin = millis();
			}
			// set info on the LCD
			sprintf(lcd_string, " -- %02d:%02d --", duration_hours, duration_minutes);
			lcd.setCursor(0,1);
			lcd.print(lcd_string);
			standby_time = millis();
			delay(50);
		break;
		case STATE_OUT_ON:
		lcd.setCursor(0,0);
		lcd.print(" Set OUT ON:");
		
		//turn off relay
		digitalWrite(relayPin, HIGH);
		state_on_enabled = false;
		
		//read the hour button state
		buttonInscreaseHourState = digitalRead(buttonIncreaseHourPin);
		
		if(buttonInscreaseHourState == HIGH)
		{
			//debouncing
			if((millis() - lastDebounceTimeIncreaseHour) > 100)
			{
				// disable out
				state_on_enabled = false;
			}
			lastDebounceTimeIncreaseHour = millis();
		}
		
		//read the minute button state
		buttonInscreaseMinuteState = digitalRead(buttonIncreaseMinutePin);
		
		if(buttonInscreaseMinuteState == HIGH)
		{
			//debouncing
			if((millis() - lastDebounceTimeIncreaseMin) > 100)
			{
				// enable out
				state_on_enabled = true;
			}
			lastDebounceTimeIncreaseMin = millis();
		}
		
		if(state_on_enabled)
		{
			digitalWrite(relayPin, LOW);
			sprintf(lcd_string, " %02d:%02d -- ON", RTC.now().hour(), RTC.now().minute());
		}
		else
		{
			digitalWrite(relayPin, HIGH);
			sprintf(lcd_string, " %02d:%02d -- OFF", RTC.now().hour(), RTC.now().minute());
		}
		// set info on the LCD
		lcd.setCursor(0,1);
		lcd.print(lcd_string);
		standby_time = millis();
		delay(50);
		break;		
		default:
			lcd.setCursor(0,1);
			lcd.print(" STATE ERROR!!!");
		break;
	}
}

// fanction called when button state is pressed
void changeState()
{
	//debouncing
	if((millis() - lastDebounceTimeState) > 200)
	{
		state = (state + 1) % NUM_STATES;
		//attachInterrupt(0, chageState, RISING);
	}

	lastDebounceTimeState = millis();
	standby_time = millis();
	//
}
