/*
Name:		Sketch1.ino
Created:	6/7/2017 10:29:23 AM
Author:	Mohamed Osman
*/
/*
Universal 8bit Graphics Library, https://github.com/olikraus/u8glib/
*/

#define USE_SPECIALIST_METHODS

#include <DS3232RTC.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <Wire.h>
#include "U8glib.h"

#define KEY_NONE 0
#define KEY_PREV 1
#define KEY_NEXT 2
#define KEY_SELECT 3
#define KEY_BACK 4

#undef dtNBR_ALARMS       //important
#define dtNBR_ALARMS 15   // max is 255


//Global Variables
uint8_t uiKeyPrev = 4;
uint8_t uiKeyNext = 3;
uint8_t uiKeySelect = 2;
uint8_t uiKeyBack = 5;
bool callSetToggleOff = 0;
uint8_t uiKeyCodeFirst = KEY_NONE;
uint8_t uiKeyCodeSecond = KEY_NONE;
uint8_t uiKeyCode = KEY_NONE;

#define Month_no
const char *monthWeek[Month_no] = { "", "Jan", "Feb", "Mar", "Apr", "Maj", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dec" };

const char *Weekday[8] = { "", "Sun" , "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

#define MENU_ITEMS 6
const char *menu_strings[MENU_ITEMS] = { "Current Date and Time", "Set Date", "Set Time", "Set Trigger Time", "Set Trigger Duration", "Set Frequency" };

uint8_t menu_current = 0;
uint8_t menu_redraw_required = 0;
uint8_t last_key_code = KEY_NONE;
uint8_t drawcode = 0;
uint8_t realtime = 0;
uint8_t root = 0;   
uint8_t item3 = 0;
uint8_t redraw = 0;
//stores the duration, trigger time of the alarm
uint8_t trigTime[4];
uint8_t tempTime[4];
uint8_t durTime[3];
//stores the alarm ids of alarms to be used (used to free the alarms and reuse them)
uint8_t alarmID;
uint8_t alarmIDoff;
uint8_t alarmIDs[7];
bool dispTrig = 0;
bool shiftMenu = 0;

unsigned long timeCheckVar;
uint8_t alarmCounter;
bool dateTicked[8];//stores the dates ticked in the frequency menu
bool keyChanged;//checks if a key is pressed
bool callTriggerFunction = 0;//calls the funtion to start an alarm, set to zero so that no alarm is set by default 

#define LINE_PIXEL_HEIGHT 7

 // setup u8g object
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE | U8G_I2C_OPT_DEV_0);  // I2C / TWI 

void abc(void) {
	// graphic commands to redraw the complete screen should be placed here  
	u8g.setFont(u8g_font_unifont);
	u8g.drawStr(0, 22, "Hello World!");
}


void setup(void) {
	// configure input keys 
	dateTicked[0] = 1;//by default everyday in the frequency menu is ticked
	pinMode(uiKeyPrev, INPUT_PULLUP);           // set pin to input with pullup
	pinMode(uiKeyNext, INPUT_PULLUP);           // set pin to input with pullup
	pinMode(uiKeySelect, INPUT_PULLUP);           // set pin to input with pullup
	pinMode(uiKeyBack, INPUT_PULLUP);           // set pin to input with pullup

	//Serial.begin(9600);

	pinMode(9, OUTPUT);//relay pin
	digitalWrite(9, LOW);

	u8g.firstPage();//to draw black screen at start
	do {
		
	} while(u8g.nextPage());
	
	setSyncProvider(RTC.get); // the function to get the time from the RTC
	setSyncInterval(1);
	Serial.println(dtNBR_ALARMS);

	//default times for the menus
	durTime[2] = 10;
	trigTime[0] = hourFormat12();
	trigTime[1] = minute();
	trigTime[2] = second();
	trigTime[3] = isPM();


	if (timeStatus() != timeSet) {
		Serial.println("Unable to sync with the RTC");
	}

	else
		Serial.println("RTC has set the system time");

}

void loop(void) {


	if (menu_redraw_required != 0) {//draw menu
		// picture loop
		u8g.firstPage();
		do {
			drawMenu();
		} while (u8g.nextPage());

		menu_redraw_required = 0;
	}

	if (drawcode != 0 || redraw != 0) {
		switch (drawcode) {
		case 1: draw(); break;//current date and time
		case 2: draw_2(); break;//set date 
		case 3: draw_3(0); break;//set time
		case 4: draw_3(1); break;//set trigger time
		case 5: draw_3(2); break;//set duration
		case 6: draw_4();break;//set frequency
		}
		if (uiKeyCode < KEY_BACK && realtime == 1) { drawcode = 1;Alarm.delay(50); }//refresh time display 
		else { drawcode = 0; realtime = 0; }
		if (root != 0 && redraw == 1) { drawcode = root; redraw = 0; }//r

	}
	keypress();//get keypress
	updateMenu();  // update menu bar

	if (dispTrig) {//show alarm triggered on screen
		if (callSetToggleOff) {//call next alarm first
			callSetToggleOff = 0;
			setToggleOff();
		}
		dispTrig = 0;
		dispTriggered();
		drawcode = 0;
		root = 0;
		menu_redraw_required = 1;
	}

	if (callTriggerFunction) {
		callTriggerFunction = 0;
		triggerFunction();
	}
	if (callSetToggleOff) {
		callSetToggleOff = 0;
		setToggleOff();
	}
	// rebuild the picture after some delay
	Alarm.delay(10);//always use alarm.delay
}


///////////////////////

//function definitions
void triggerFunction(void) {
	freeAlarms();//to allow overwriting alarms
	digitalWrite(9, LOW);	//ensure that output is low 
	for (int i = 0;i < 7;i++) {
		if(dateTicked[1+i]||dateTicked[0])//trigger alarms according to  if they were ticked
		alarmIDs[i] = Alarm.alarmRepeat((timeDayOfWeek_t)i+1, change12to24(trigTime[0], trigTime[3]), trigTime[1], trigTime[2], toggleOn);
	}
	return;
}
void freeAlarms() {
	Alarm.free(alarmIDoff);
	for (int i = 0;i < 7;i++) {
		Alarm.free(alarmIDs[i]);
	}
}
void setToggleOff(void) {//set toggle off alarm
	Alarm.free(alarmIDoff);
	uint8_t timeH, timeM, timeS;
	if ((change12to24(trigTime[0], trigTime[3]) + durTime[0]) > 23) {
		timeH = (change12to24(trigTime[0], trigTime[3]) + durTime[0]) - 24;
	}
	else timeH = (change12to24(trigTime[0], trigTime[3]) + durTime[0]);

	if (trigTime[1] + durTime[1] > 59) {
		timeM = trigTime[1] + durTime[1] - 60;
		timeH++;
	}
	else timeM = trigTime[1] + durTime[1];

	if (trigTime[2] + durTime[2] > 59) {
		timeS = trigTime[2] + durTime[2] - 60;
		timeM++;
	}
	else timeS = trigTime[2] + durTime[2];

	Serial.println(timeH);
	Serial.println(":");
	Serial.println(timeM);
	Serial.println(":");
	Serial.println(timeS);

	alarmIDoff = Alarm.alarmOnce(timeH, timeM, timeS, toggleOff);
}

void dispTriggered() {//show alarm triggered
	uint8_t oldUiKeyCode;
	unsigned long nextAlarmTime = Alarm.getNextTrigger();
	bool keyChanged = 0;
	u8g.firstPage();
	do {
		u8g.setFont(u8g_font_unifont);
		u8g.drawStr(4, 40, "Alarm Triggered");
		Alarm.delay(1);
	} while (u8g.nextPage());

	while (now() < nextAlarmTime &&!keyChanged) {
		oldUiKeyCode = uiKeyCode;
		keypress();
		if (oldUiKeyCode == 0 && uiKeyCode > 0)keyChanged = 1;
		Alarm.delay(1);//alarms dont work if you dont keep calling alarm.delay 
	}
	keyChanged = 0;
}

void toggleOn(void) {
	dispTrig = 1;
	digitalWrite(9, HIGH);
	callSetToggleOff = 1;
	return;
}

void toggleOff(void) {
	digitalWrite(9, LOW);
	return;
}

void keypress(void) {
	uiKeyCodeSecond = uiKeyCodeFirst;
	if (digitalRead(uiKeyPrev) == LOW)
		uiKeyCodeFirst = KEY_PREV;
	else if (digitalRead(uiKeyNext) == LOW)
		uiKeyCodeFirst = KEY_NEXT;
	else if (digitalRead(uiKeySelect) == LOW)
		uiKeyCodeFirst = KEY_SELECT;
	else if (digitalRead(uiKeyBack) == LOW)
		uiKeyCodeFirst = KEY_BACK;
	else
		uiKeyCodeFirst = KEY_NONE;

	if (uiKeyCodeSecond == uiKeyCodeFirst) {
		uiKeyCode = uiKeyCodeFirst;
	}
	else
		uiKeyCode = KEY_NONE;


}


void draw(void) {
	Alarm.delay(1);

	u8g.firstPage();
	do {
		u8g.setFont(u8g_font_unifont);
		u8g.drawStr(0, 11, "Current time is");
		keypress();
		u8g.setPrintPos(5, 24);
		u8g.print(hourFormat12());
		u8g.print(":");
		if (minute() < 10)u8g.print(0);
		u8g.print(minute());
		u8g.print(":");
		if (second() < 10)u8g.print(0);
		u8g.print(second());
		if (isPM() == true)
			u8g.print("PM");
		else u8g.print("AM");

		u8g.drawStr(0, 49, "Today's Date:");
		u8g.setPrintPos(5, 62);
		u8g.print(Weekday[weekday()]);

		u8g.setPrintPos(35, 62);
		u8g.print(day());
		u8g.print("-");
		u8g.print(monthWeek[month()]);
		u8g.print("-");
		u8g.print(year());

		realtime = 1;
	} while (u8g.nextPage());


}

void draw_2(void) {
	uint8_t itemSelected = 0;
	uint8_t oldUiKeyCode;
	keyChanged = 1;
	bool drawOnce = 0;
	timeCheckVar = millis();

	uint8_t xCorrection = 2;
	uint8_t xCorrection1 = 0;
	while (1) {
		if (dispTrig) {
			if (callSetToggleOff) {
				callSetToggleOff = 0;
				setToggleOff();
			}
			dispTrig = 0;
			dispTriggered();
			keyChanged = 1;

		}
		Alarm.delay(1);
		if (callTriggerFunction) {
			callTriggerFunction = 0;
			triggerFunction();
		}
		if (callSetToggleOff) {
			callSetToggleOff = 0;
			setToggleOff();
		}
		xCorrection = 2;
		xCorrection1 = 0;
		if (keyChanged || drawOnce) {//draw once used to rerender current screen when just out of the change date field function
			drawOnce = 0;
			u8g.firstPage();
			do {
				uint8_t texth;//text height

				u8g_uint_t w, d;
				u8g.setFont(u8g_font_unifont);
				u8g.setDefaultForegroundColor();
				texth = u8g.getFontAscent() - u8g.getFontDescent();
				u8g.drawStr(0, 15, "Set Date:");
				u8g.setPrintPos(10, 50);
				u8g.print(day());
				u8g.print(" : ");
				u8g.print(month());
				u8g.print(" : ");
				u8g.print(year());
				if (day() > 9)xCorrection = 3;
				if (day() > 9 && month() > 9)xCorrection1 = 1;
				if (month() > 9)xCorrection1 = 1;
				switch (itemSelected) {
				case 0:
					u8g.drawBox(10, 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					u8g.setPrintPos(10, 50);
					u8g.print(day());
					u8g.setDefaultForegroundColor();
					break;
				case 1:
					u8g.setPrintPos(10, 50);
					u8g.print(day());
					u8g.print(" : ");
					u8g.drawBox(10 + (8 * (2 + xCorrection)), 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					u8g.print(month());
					u8g.setDefaultForegroundColor();
					break;
				case 2:
					u8g.setPrintPos(10, 50);
					u8g.print(day());
					u8g.print(" : ");
					u8g.print(month());
					u8g.print(" : ");
					u8g.drawBox(10 + (8 * (6 + xCorrection + xCorrection1)), 50 - texth + 1, 8 * 4, texth);
					u8g.setDefaultBackgroundColor();
					u8g.print(year());
					u8g.setDefaultForegroundColor();
					break;
				}

			} while (u8g.nextPage());
			keyChanged = 0;
		}
		oldUiKeyCode = uiKeyCode;
		keypress();
		if (oldUiKeyCode == 0 && uiKeyCode > 0)keyChanged = 1;

		if (millis() - timeCheckVar > 300000) {//return to main menu if user doesnt press anything
			drawcode =1;
			realtime = 1;
			callTriggerFunction = 1;
			return;
		}

		if (keyChanged) {
			switch (uiKeyCode) {
			case KEY_PREV:
				timeCheckVar = millis();
				if (itemSelected == 0)itemSelected = 3;
				itemSelected = (itemSelected - 1) % 3;
				break;
			case KEY_NEXT:
				timeCheckVar = millis();
				itemSelected = (itemSelected + 1) % 3;
				break;
			case KEY_BACK:
				timeCheckVar = millis();
				drawcode = 0;
				root = 0;
				menu_redraw_required = 1;
				callTriggerFunction = 1;
				return;
			case KEY_SELECT:
				timeCheckVar = millis();
				changeDateField(itemSelected);
				keyChanged = 0;
				Alarm.delay(100);
				drawOnce = 1;
				uiKeyCode = 1;//to prevent false keyChange high
				break;
			}
		}
	}
}

void changeDateField(uint8_t item) {//changes the values of specific date fields
	timeCheckVar = millis();
	int newDate[3];
	newDate[0] = day();
	newDate[1] = month();
	newDate[2] = year();

	uint8_t oldUiKeyCode;
	keyChanged = 1;
	uint8_t xCorrection = 2;
	uint8_t xCorrection1 = 0;
	int modVar = 0;
	while (1) {
		if (dispTrig) {//incase alarms trigger in here
			if (callSetToggleOff) {
				callSetToggleOff = 0;
				setToggleOff();
			}
			dispTrig = 0;
			dispTriggered();
			keyChanged = 1;

		}
		Alarm.delay(1);
		xCorrection = 2;
		xCorrection1 = 0;
		if (keyChanged) {
			u8g.firstPage();
			do {
				uint8_t texth;//text height
				u8g_uint_t w, d;

				u8g.setFont(u8g_font_unifont);
				u8g.setDefaultForegroundColor();
				texth = u8g.getFontAscent() - u8g.getFontDescent();
				u8g.drawStr(0, 15, "Set Date:");
				u8g.setPrintPos(10, 50);
				u8g.print(newDate[0]);
				u8g.print(" : ");
				u8g.print(newDate[1]);
				u8g.print(" : ");
				u8g.print(newDate[2]);
				u8g.drawStr(10, 34, "D   M    Y");
				if (newDate[0] > 9)xCorrection = 3;
				if (newDate[0] > 9 && newDate[1] > 9) { xCorrection1 = 1; }
				if (newDate[1] > 9)xCorrection1 = 1;
				switch (item) {
				case 0:
					u8g.drawBox(10, 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					u8g.setPrintPos(10, 50);
					u8g.print(newDate[0]);
					u8g.setDefaultForegroundColor();
					break;
				case 1:
					u8g.setPrintPos(10, 50);
					u8g.print(newDate[0]);
					u8g.print(" : ");
					u8g.drawBox(10 + (8 * (2 + xCorrection)), 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					u8g.print(newDate[1]);
					u8g.setDefaultForegroundColor();
					break;
				case 2:
					u8g.setPrintPos(10, 50);
					u8g.print(newDate[0]);
					u8g.print(" : ");
					u8g.print(newDate[1]);
					u8g.print(" : ");
					u8g.drawBox(10 + (8 * (6 + xCorrection + xCorrection1)), 50 - texth + 1, 8 * 4, texth);
					u8g.setDefaultBackgroundColor();
					u8g.print(newDate[2]);
					u8g.setDefaultForegroundColor();
					break;
					//u8g.print(menu_current);
				}

			} while (u8g.nextPage());
			keyChanged = 0;
		}
		oldUiKeyCode = uiKeyCode;
		keypress();
		if (oldUiKeyCode == 0 && uiKeyCode > 0)keyChanged = 1;

		if (millis() - timeCheckVar > 300000) {
			callTriggerFunction = 1;
			goto changeDate;
		}

		if (keyChanged) {
			switch (item) {
			case 0: {
				bool isLeapYear = (newDate[2] % 4) || ((newDate[2] % 100 == 0) &&
					(newDate[2] % 400)) ? 0 : 1;
				modVar = ((newDate[1] == 2) ? (28 + isLeapYear) : 31 - ((newDate[1] - 1) % 7 % 2)) + 1;
			}
					break;
			case 1:
				modVar = 13;
				break;
			case 2:modVar = 2037;
				break;
			}
			switch (uiKeyCode) {
			case KEY_PREV:
				timeCheckVar = millis();
				if (newDate[item] == 1)newDate[item] = modVar;
				newDate[item] = (newDate[item] - 1) % modVar;
				break;
			case KEY_NEXT:
				timeCheckVar = millis();
				if (newDate[item] == modVar - 1)newDate[item] = 0;
				newDate[item] = (newDate[item] + 1) % modVar;
				break;
			case KEY_BACK:
				timeCheckVar = millis();
				changeDate:
				freeAlarms();
				setTime(hour(), minute(), second(), newDate[0], newDate[1], newDate[2]); 
				RTC.set(now());                     //set the RTC from the system time
				callTriggerFunction = 1;
				return;
			case KEY_SELECT:
				timeCheckVar = millis();
				break;
			}
		}
	}
}

void draw_3(uint8_t isTriggerTime) {//draws the 3 different menus depending on the argument
	short int itemSelected = 0;
	uint8_t oldUiKeyCode;
	timeCheckVar = millis();
	keyChanged = 1;
	bool drawOnce = 0;
	uint8_t xCorrection = 2;

	switch (isTriggerTime) {//default times for first entry into the menus

	case 0:
		tempTime[0] = hourFormat12();
		tempTime[1] = minute();
		tempTime[2] = second();
		tempTime[3] = isPM();
		break;
	case 1:
		tempTime[0] = trigTime[0];
		tempTime[1] = trigTime[1];
		tempTime[2] = trigTime[2];
		tempTime[3] = trigTime[3];
		break;
	case 2:
		tempTime[0] = durTime[0];
		tempTime[1] = durTime[1];
		tempTime[2] = durTime[2];
		tempTime[3] = durTime[3];
		break;
	}

	while (1) {
		if (dispTrig) {//incase alarms trigger in here
			if (callSetToggleOff) {
				callSetToggleOff = 0;
				setToggleOff();
			}
			dispTrig = 0;
			dispTriggered();
			keyChanged = 1;

		}
		Alarm.delay(1);
		xCorrection = 2;
		if (callTriggerFunction) {
			callTriggerFunction = 0;
			triggerFunction();
		}	if (callSetToggleOff) {
			callSetToggleOff = 0;
			setToggleOff();
		}

		if (keyChanged || drawOnce) {
			drawOnce = 0;
			u8g.firstPage();
			do {
				uint8_t texth;//text height

				u8g_uint_t w, d;
				u8g.setFont(u8g_font_unifont);
				u8g.setDefaultForegroundColor();
				texth = u8g.getFontAscent() - u8g.getFontDescent();
				u8g.setPrintPos(0, 15);
				u8g.print("Set ");
				if (isTriggerTime == 1)u8g.print("trigger ");
				if (isTriggerTime == 2)u8g.print("Duration:");
				else u8g.print("Time:");
				u8g.setPrintPos(10, 50);
				u8g.print(tempTime[0]);
				u8g.print(":");
				if (tempTime[1] < 10)u8g.print(0);
				u8g.print(tempTime[1]);
				u8g.print(":");
				if (tempTime[2] < 10)u8g.print(0);
				u8g.print(tempTime[2]);

				if (isTriggerTime != 2) {
					if (tempTime[3] == true)
						u8g.print("PM");
					else u8g.print("AM");
				}
				if (tempTime[0]> 9) xCorrection = 3;
				switch (itemSelected) {
				case 0:
					u8g.drawBox(10, 50 - texth + 1, 8 * (xCorrection - 1), texth);
					u8g.setDefaultBackgroundColor();
					u8g.setPrintPos(10, 50);
					u8g.print(tempTime[0]);
					u8g.setDefaultForegroundColor();
					break;
				case 1:
					u8g.setPrintPos(10, 50);
					u8g.print(tempTime[0]);
					u8g.print(":");
					u8g.drawBox(10 + (8 * (xCorrection)), 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					if (tempTime[1] < 10)u8g.print(0);
					u8g.print(tempTime[1]);
					u8g.setDefaultForegroundColor();
					break;
				case 2:
					u8g.setPrintPos(10, 50);
					u8g.print(tempTime[0]);
					u8g.print(":");
					if (tempTime[1]< 10)u8g.print(0);
					u8g.print(tempTime[1]);
					u8g.print(":");
					u8g.drawBox(10 + (8 * (3 + xCorrection)), 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					if (tempTime[2]< 10)u8g.print(0);
					u8g.print(tempTime[2]);
					u8g.setDefaultForegroundColor();
					break;
				case 3:
					u8g.setPrintPos(10, 50);
					u8g.print(tempTime[0]);
					u8g.print(":");
					if (tempTime[1] < 10)u8g.print(0);
					u8g.print(tempTime[1]);
					u8g.print(":");
					if (tempTime[2] < 10)u8g.print(0);
					u8g.print(tempTime[2]);
					u8g.drawBox(10 + (8 * (3 + xCorrection + 2)), 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					if (tempTime[3] == true)
						u8g.print("PM");
					else u8g.print("AM");
					u8g.setDefaultForegroundColor();
					break;

				}

			} while (u8g.nextPage());
			keyChanged = 0;
		}
		oldUiKeyCode = uiKeyCode;
		keypress();
		if (oldUiKeyCode == 0 && uiKeyCode > 0)keyChanged = 1;
		if (millis() - timeCheckVar > 300000) {
			drawcode = 1;
			realtime = 1;
			callTriggerFunction = 1;
			return;
		}

		if (keyChanged) {
			switch (uiKeyCode) {
			case KEY_PREV:
				timeCheckVar = millis();
				if (isTriggerTime == 2 && itemSelected == 0)itemSelected = 3;
				else if (itemSelected == 0)itemSelected = 4;
				itemSelected = (itemSelected - 1) % 4;
				break;
			case KEY_NEXT:
				timeCheckVar = millis();
				if (isTriggerTime == 2 && itemSelected == 2)itemSelected = -1;
				itemSelected = (itemSelected + 1) % 4;
				break;
			case KEY_BACK:
				timeCheckVar = millis();
				drawcode = 0;
				root = 0;
				menu_redraw_required = 1;

				switch (isTriggerTime) {
				case 0:
					callTriggerFunction = 1;
					setTime(change12to24(tempTime[0], tempTime[3]), tempTime[1], tempTime[2], day(), month(), year());   
					RTC.set(now());                     //set the RTC from the system time
					break;
				case 1:
					callTriggerFunction = 1;
					trigTime[0] = tempTime[0];
					trigTime[1] = tempTime[1];
					trigTime[2] = tempTime[2];
					trigTime[3] = tempTime[3];
					break;
				case 2:
					callTriggerFunction = 1;
					durTime[0] = tempTime[0];
					durTime[1] = tempTime[1];
					durTime[2] = tempTime[2];
					break;
				}
				if (isTriggerTime == 1 || isTriggerTime == 2) {
					u8g.firstPage();
					do {
						u8g.setFont(u8g_font_unifont);
						u8g.setDefaultForegroundColor();

						u8g.setPrintPos(0, 15);
						u8g.print("Trigger Time:");

						u8g.setPrintPos(0, 15 + 13 * 1);
						u8g.print(trigTime[0]);
						u8g.print(":");
						if (trigTime[1] < 10)u8g.print(0);
						u8g.print(trigTime[1]);
						u8g.print(":");
						if (trigTime[2] < 10)u8g.print(0);
						u8g.print(trigTime[2]);
						if (trigTime[3] == true)
							u8g.print("PM");
						else u8g.print("AM");

						u8g.setPrintPos(0, 15 + 13 * 2);
						u8g.print("for duration:");

						u8g.setPrintPos(0, 15 + 13 * 3);
						u8g.print(durTime[0]);
						u8g.print(":");
						if (durTime[1] < 10)u8g.print(0);
						u8g.print(durTime[1]);
						u8g.print(":");
						if (durTime[2] < 10)u8g.print(0);
						u8g.print(durTime[2]);
						//u8g.print(" everyday");

					} while (u8g.nextPage());
					Alarm.delay(3500);
				}
				return;
			case KEY_SELECT:
				timeCheckVar = millis();
				changeTimeField(itemSelected, isTriggerTime);
				keyChanged = 0;
				drawOnce = 1;
				Alarm.delay(100);
				uiKeyCode = 1;//to prevent false keyChange high
				break;
			}
		}
	}
}
//
int change12to24(int hour12, bool isPM) {
	if (hour12 == 12 && !isPM)return 0;
	else if (hour12 == 12 && isPM)return 12;
	else return isPM ? hour12 + 12 : hour12;
}

void changeTimeField(uint8_t item, uint8_t isTriggerTime) {
	short int newTime[4];
	timeCheckVar = millis();
	newTime[0] = tempTime[0];
	newTime[1] = tempTime[1];
	newTime[2] = tempTime[2];
	newTime[3] = tempTime[3];

	uint8_t oldUiKeyCode;
	keyChanged = 1;
	uint8_t xCorrection = 2;
	//uint8_t xCorrection1 = 0;
	int modVar = 0;
	while (1) {
		if (dispTrig) {
			if (callSetToggleOff) {
				callSetToggleOff = 0;
				setToggleOff();
			}
			dispTrig = 0;
			dispTriggered();
			keyChanged = 1;

		}
		Alarm.delay(1);
		if (callTriggerFunction) {
			callTriggerFunction = 0;
			triggerFunction();
		}
		if (callSetToggleOff) {
			callSetToggleOff = 0;
			setToggleOff();
		}
		xCorrection = 2;
		if (keyChanged) {
			u8g.firstPage();
			do {
				uint8_t texth;//text height
				u8g_uint_t w, d;

				u8g.setFont(u8g_font_unifont);
				u8g.setDefaultForegroundColor();
				texth = u8g.getFontAscent() - u8g.getFontDescent();
				u8g.setPrintPos(0, 15);
				u8g.print("Set ");
				if (isTriggerTime == 1)u8g.print("trigger ");
				if (isTriggerTime == 2)u8g.print("Duration:");
				else u8g.print("Time:");
				u8g.setPrintPos(10, 50);
				u8g.print(newTime[0]);
				u8g.print(":");
				u8g.drawStr(10, 34, "H  M  S");
				if (newTime[1] < 10)u8g.print(0);
				u8g.print(newTime[1]);
				u8g.print(":");
				if (newTime[2] < 10)u8g.print(0);
				u8g.print(newTime[2]);
				if (isTriggerTime != 2) {
					if (newTime[3] == 1)
						u8g.print("PM");
					else u8g.print("AM");
				}
				if (newTime[0] > 9)xCorrection = 3;
				switch (item) {
				case 0:
					u8g.drawBox(10, 50 - texth + 1, 8 * (xCorrection - 1), texth);
					u8g.setDefaultBackgroundColor();
					u8g.setPrintPos(10, 50);
					u8g.print(newTime[0]);
					u8g.setDefaultForegroundColor();
					break;
				case 1:
					u8g.setPrintPos(10, 50);
					u8g.print(newTime[0]);
					u8g.print(":");
					u8g.drawBox(10 + (8 * (xCorrection)), 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					if (newTime[1] < 10)u8g.print(0);
					u8g.print(newTime[1]);
					u8g.setDefaultForegroundColor();
					break;
				case 2:
					u8g.setPrintPos(10, 50);
					u8g.print(newTime[0]);
					u8g.print(":");
					if (newTime[1]  < 10)u8g.print(0);
					u8g.print(newTime[1]);
					u8g.print(":");
					u8g.drawBox(10 + (8 * (3 + xCorrection)), 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					if (newTime[2]< 10)u8g.print(0);
					u8g.print(newTime[2]);
					u8g.setDefaultForegroundColor();
					break;
				case 3:
					u8g.setPrintPos(10, 50);
					u8g.print(newTime[0]);
					u8g.print(":");
					if (newTime[1]  < 10)u8g.print(0);
					u8g.print(newTime[1]);
					u8g.print(":");
					if (newTime[2] < 10)u8g.print(0);
					u8g.print(newTime[2]);
					u8g.drawBox(10 + (8 * (3 + xCorrection + 2)), 50 - texth + 1, 8 * 2, texth);
					u8g.setDefaultBackgroundColor();
					if (newTime[3] == 1)
						u8g.print("PM");
					else u8g.print("AM");
					u8g.setDefaultForegroundColor();
					break;

				}

			} while (u8g.nextPage());
			keyChanged = 0;
		}
		oldUiKeyCode = uiKeyCode;
		keypress();
		if (oldUiKeyCode == 0 && uiKeyCode > 0)keyChanged = 1;

		if (millis() - timeCheckVar > 300000) {
			callTriggerFunction = 1;
			goto changeTime;
		}

		if (keyChanged) {
			switch (item) {
			case 0: {
				modVar = 13;
			}
					break;
			case 1:
			case 2:modVar = 60;
				break;
			case 3: modVar = 2;
				break;
			}
			switch (uiKeyCode) {
			case KEY_PREV:
				timeCheckVar = millis();
				if (item == 0 && newTime[item] == 1)newTime[item] = modVar;
				else if (newTime[item] == 0)newTime[item] = modVar;
				newTime[item] = (newTime[item] - 1) % modVar;
				break;
			case KEY_NEXT:
				timeCheckVar = millis();
				if (item == 0 && newTime[item] == modVar - 1)newTime[item] = 0;
				else if (newTime[item] == modVar - 1)newTime[item] = -1;

				newTime[item] = (newTime[item] + 1) % modVar;
				break;
			case KEY_BACK:

				timeCheckVar = millis();
			changeTime:
				tempTime[0] = newTime[0];
				tempTime[1] = newTime[1];
				tempTime[2] = newTime[2];
				tempTime[3] = newTime[3];
				return;
			case KEY_SELECT:
				timeCheckVar = millis();
				break;
			}
		}
	}


}

void draw_4(void) {//draw the set frequency menu

	timeCheckVar = millis();
	keyChanged = 1;
	uint8_t oldUiKeyCode;
	Serial.println(Alarm.getNextTrigger());
	Serial.println(now());
	Serial.println(Alarm.count());
	bool drawOnce2 = 0;
	uint8_t i, h, menuCurrenti = 0;
	bool menuCurrentj = 1;
	const uint8_t weekdayLength = 20;

	while (1) {
		if (dispTrig) {//incase alarm triggers in here
			if (callSetToggleOff) {
				callSetToggleOff = 0;
				setToggleOff();
			}
			dispTrig = 0;
			dispTriggered();
			keyChanged = 1;
		}
		Alarm.delay(1);
			if (callTriggerFunction) {
		callTriggerFunction = 0;
		triggerFunction();
		}	
			if (callSetToggleOff) {
		callSetToggleOff = 0;
		setToggleOff();
		}
		if (keyChanged||drawOnce2) {
			drawOnce2 = 0;
			u8g.firstPage();

			do {
				u8g.setFont(u8g_font_6x13);
				u8g.setFontRefHeightText();
				u8g.setFontPosTop();

				h = u8g.getFontAscent() - u8g.getFontDescent();

				for (i = 0; i < 5; i++) {
					for (int j = 0;j < 2;j++) {
						if (i == 4 && j == 1)continue;
						u8g.setDefaultForegroundColor();
						if (menuCurrenti == 0 && i == 0 &&j==0) { //because menu number one takes the whole row 
							u8g.drawBox(h , i*h + 10, weekdayLength * 2 + 10, 12);//draw box on current item

							if (dateTicked[0])u8g.drawBox(h / 2, i*h + 14, 4, 4);

							u8g.setDefaultBackgroundColor();
						}
						else if (i == menuCurrenti&&menuCurrentj == j&&i!=0) {
								u8g.drawBox(h + j*(h + 1 + weekdayLength), i*h+10, weekdayLength, h);
								if (dateTicked[2 * i + j - 1]) {
									u8g.drawBox(h / 2 + j*(h + 1 + weekdayLength), i*h + 14, 4, 4);//draw box on current item
								}
								u8g.setDefaultBackgroundColor();
							}

						if (i == 0 && j == 0) {
							if (dateTicked[0]&&!(menuCurrenti == 0 && i == 0 && j == 0))u8g.drawBox(h / 2, i*h + 14, 4, 4);

							u8g.drawStr(h + 1, 10 + i*h, "Everyday"); 
							
						}
						else if(i!=0){ 
							u8g.drawStr(h + 1 + j*(h + 1 + weekdayLength), i*h+10, Weekday[2 * i + j - 1]); //draw the days of week strings
							if (dateTicked[2 * i + j - 1]&&!(i == menuCurrenti&&menuCurrentj == j&&i != 0)) {//draw the ticked boxes indicator
								u8g.drawBox(h/2 + j*(h + 1 + weekdayLength), i*h + 14, 4, 4);
							}
						}

					}
				}
			} while (u8g.nextPage());
			keyChanged = 0;
		}

		oldUiKeyCode = uiKeyCode;
		keypress();
		if (oldUiKeyCode == 0 && uiKeyCode > 0)keyChanged = 1;

		if (millis() - timeCheckVar > 300000) {
			drawcode = 1;
			realtime = 1;
			//menu_redraw_required = 1;
			callTriggerFunction = 1;
			return;
		}

		if (keyChanged) {
			switch (uiKeyCode) {
			case KEY_PREV://to warp the menu selection appropriately
				timeCheckVar = millis();
				if (menuCurrenti == 0 && (menuCurrentj == 0 || menuCurrentj == 1)) {
					menuCurrenti = 4;
					menuCurrentj = 0;
				}else
				if (menuCurrentj) {
					menuCurrentj = 0;
				}
				else if (menuCurrenti == 0) {
					menuCurrenti = 4;
				}
				else { 
					menuCurrenti = menuCurrenti - 1; 
					menuCurrentj = 1;
				}
				break;
			case KEY_NEXT:
				timeCheckVar = millis();
				if (menuCurrenti == 0 && (menuCurrentj == 0 || menuCurrentj == 1)) {
					menuCurrenti = 1;
					menuCurrentj = 0;
				}else
				if (menuCurrentj) {
					menuCurrentj = 0;
					if (menuCurrenti == 4)menuCurrenti = 0;
					menuCurrenti++;
				}
				else if (menuCurrenti == 4) {
					menuCurrenti = 0;
					menuCurrentj = 1;
				}
				else menuCurrentj=1;
				break;

			case KEY_BACK:
				timeCheckVar = millis();
				drawcode = 0;
				root = 0;
				menu_redraw_required = 1;
				callTriggerFunction = 1;
				return;
			case KEY_SELECT:
				timeCheckVar = millis();
				if (menuCurrenti == 0 && (menuCurrentj == 0 || menuCurrentj == 1)) {
					dateTicked[0] = !dateTicked[0];//toggle everyday
					if (dateTicked[0]) {
						for (int i = 1;i < 8;i++) {
							dateTicked[i] = 0;//zero the others if evereday is ticked
						}
					}
				}
				else {
					dateTicked[2 * menuCurrenti + menuCurrentj - 1] = !dateTicked[2 * menuCurrenti + menuCurrentj - 1]; //toggle tick
					dateTicked[0] = 0;
				}
				
				break;
			}

		}
	}
}


void drawMenu(void) {
	uint8_t i, h;
	u8g_uint_t w, d;

	u8g.setFont(u8g_font_6x13);
	u8g.setFontRefHeightText();
	u8g.setFontPosTop();

	h = u8g.getFontAscent() - u8g.getFontDescent();
	w = u8g.getWidth();

	Serial.println(h);
	if (menu_current == 5)shiftMenu = 1;
	else if (menu_current == 0)shiftMenu = 0;

		for (i = 0; i < MENU_ITEMS; i++) {
			d = (w - u8g.getStrWidth(menu_strings[i])) / 2;
			u8g.setDefaultForegroundColor();
			if (i == menu_current) {//draw a box on the current menu
				if(shiftMenu)u8g.drawBox(0, i*h - 2, w, h);
				else u8g.drawBox(0, i*h, w, h);
				u8g.setDefaultBackgroundColor();
			}
			if(shiftMenu)u8g.drawStr(d, i*h - 2, menu_strings[i]);
			else u8g.drawStr(d, i*h, menu_strings[i]);
		}
	
	}

void updateMenu(void) {
	if (uiKeyCode != KEY_NONE && last_key_code == uiKeyCode) {
		return;
	}
	last_key_code = uiKeyCode;

	switch (uiKeyCode) {
	case KEY_NEXT:

	{
		switch (root) {
		case 0:
		{
			menu_current++;
			if (menu_current >= MENU_ITEMS)
				menu_current = 0; //reset back to first item
			menu_redraw_required = 1;  //set the redraw flag
			if (drawcode == 1)menu_redraw_required = 0;
			break;
		}
		case 1:

			break;
			//not todo

		case 2: break;//todo

		case 3: item3++;
			if (item3 > 3)
			{
				item3 = 3;
			}
			redraw = 1;
			break;

		case 4: break;

		case 5: break;

		}
		break;
	}

	case KEY_PREV:
	{
		switch (root) {
		case 0:
		{
			if (menu_current == 0)
				menu_current = MENU_ITEMS; //goto bottom
			menu_current--;
			menu_redraw_required = 1; //set the flag up to redraw
			if (drawcode == 1)menu_redraw_required = 0;
			break;
		}
		case 1:
			break;

		case 2: break;

		case 3: item3--;
			if (item3 == 0) { item3 = 1; }
			redraw = 1;
			break;

		case 4: break;

		case 5: break;

		}
		break;
	}

	case KEY_SELECT: {
		switch (menu_current)
		{
		case 0:
			drawcode = 1;
			//Serial.print("drawcode=");
			//Serial.println(drawcode);
			//Serial.print("menu=");
			//Serial.println(menu_current);
			break;//Displaytime  
		case 1:
			drawcode = 2;
			//Serial.print("drawcode=");
			//Serial.println(drawcode);
			//Serial.print("menu=");
			//Serial.println(menu_current);
			break; //Displaydate
		case 2:
			drawcode = 3;
			//Serial.print("drawcode=");
			//Serial.println(drawcode);
			//Serial.print("menu=");
			//Serial.println(menu_current);
			break;  //Settime
		case 3:
			drawcode = 4;
			//Serial.print("drawcode=");
			//Serial.println(drawcode);
			//Serial.print("menu=");
			//Serial.println(menu_current);
			break; //Setdate
		case 4:
			drawcode = 5;
			//Serial.print("drawcode=");
			//Serial.println(drawcode);
			//Serial.print("menu=");
			//Serial.println(menu_current);
			break;  //SetTrigger
		case 5:
			drawcode = 6;
			//Serial.print("drawcode=");
			//Serial.println(drawcode);
			//Serial.print("menu=");
			//Serial.println(menu_current);

			break;  //Set Duration
		}
		break;
	}
	case KEY_BACK: {
		switch (root)
		{
		case 0: break;

		case 1: break;

		case 3: root = 0; break;//reset root

		case 4: break;
		}
		menu_redraw_required = 1;
		break;
	}
	}
}
