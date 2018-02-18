// include the library code:
#include <Print.h>
// LCD 1602 keypad shield
#include <LiquidCrystal.h>

// Timers
#include <SoftTimer.h>
#include <DelayRun.h>

// clock + 50 bytes ram
#include <DS1307.h>

#include "define.h"

/**
  Mem size:
  Odo - 4+4+4 = 12 bytes
  magnetCount + wheelLength = 1+2 = 3 bytes
  Oiler open time - 2 bytes

  total: 19 bytes
*/
byte screenMode = SCREEN_MODE_MAIN_TT_GAS;
char buffer[16];

/* Generic speedo variables*/
union uFloat {
  float value;
  byte parts[4];
};

volatile union uFloat ttOdo, ttGas, ttTrip; /* kilometers */

union uInt {
  unsigned int value;
  byte parts[2];
};
union uInt wheelLength; /* wheelLength, mm */
byte magnetCount = 0;

float wheelSectorLength = 0; /* length of 1 sector, kilometers*/
float currentSpeed;

/* Oiler variables */
/*
   Interval:
   1..6  : 100..6000 m
   0 - off
   1 - 0.100
   2 - 1.280
   3 - 2.460
   4 - 3.640
   5 - 4.820
   6 - 6.000
   7 - always on
*/
//   toDo: const table, because map() method not too clear 
byte oilerInterval;

union uInt oilerRunTime; // millis
float nextOilStart = 0;
boolean oilerRunning = false;

/* time variables */
// toDo: read time by timer, < 1Hz
byte hours, minutes;

/* Setup sreen variables */
byte currentMenuItem = MENU_TIME;

const char* const setupMenu[] = {
  "Time",
  "Wheel length",
  "Magnets count",
  "Odometer value",
  "Oiler run time",
};

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

void drawScreen(Task* self);
void readKey(Task* self);
void oilerCheck(Task* self);
boolean oilerStop(Task* self);

void saveData(Task* self);
void speedCalc(Task* self);

Task DrawScreen(SCREEN_REFRESH_PERIOD, drawScreen);
Task ReadKey(20, readKey);
Task SaveData(1000, saveData);

Task OilerCheck(200, oilerCheck);
Task SpeedCalc(800, speedCalc);

DelayRun OilerStop(oilerRunTime.value, oilerStop);

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(LCD_X_SIZE, LCD_Y_SIZE);

  
  for (byte i=0; i<8; i++ ) {
    byte symBuffer[8];
    memcpy_PF(symBuffer, (uint_farptr_t)symTable[i], 8);
    lcd.createChar(i, symBuffer);
  }
  
  // just to test screen - fill it with white bars and blink
  
  for (byte i = 0; i < 255; i++) {
    lcd.setCursor(random(LCD_X_SIZE), random(LCD_Y_SIZE));
    lcd.print(char(4));
    delay(5);
  }

  lcd.noDisplay();
  delay(300);
  lcd.display();

  lcd.clear();

  SoftTimer.add(&DrawScreen);
  SoftTimer.add(&ReadKey);

  SoftTimer.add(&OilerCheck);

  SoftTimer.add(&SaveData);
  SoftTimer.add(&SpeedCalc);

  readData();

  pinMode(3, INPUT_PULLUP);
  attachInterrupt(1, speedTicker, FALLING);

  getWheelSectorLength();
}

void speedCalc(Task* self) {
  static unsigned long lastTick = 0;
  static float lastOdo = ttOdo.value;
  
  unsigned long timeDiff = millis() - lastTick;
  float odoLen = ttOdo.value - lastOdo;
  
  lastTick = millis();
  lastOdo = ttOdo.value;
  
  if (timeDiff == 0 || timeDiff >= 1000) {
    currentSpeed = 0;
  } else {
    currentSpeed = 3600000.0 * odoLen/float(timeDiff);
  }
}

void getWheelSectorLength() {
  if (magnetCount == 0) {
    wheelSectorLength = 0; 
  } else {
  wheelSectorLength = float(float(wheelLength.value) / float(1000000 * magnetCount));
  }
}

void readData() {
  Wire.beginTransmission(DS1307_CTRL_ID);
  Wire.write(DS1307_RAM_START); // reset register pointer
  Wire.endTransmission();

  Wire.requestFrom(DS1307_CTRL_ID, 17);
  for (byte i = 0; i < 4; i++) {
    ttOdo.parts[i] = Wire.read();
  }
  for (byte i = 0; i < 4; i++) {
    ttGas.parts[i] = Wire.read();
  }
  for (byte i = 0; i < 4; i++) {
    ttTrip.parts[i] = Wire.read();
  }
  for (byte i = 0; i < 2; i++) {
    wheelLength.parts[i] = Wire.read();
  }
  for (byte i = 0; i < 2; i++) {
    oilerRunTime.parts[i] = Wire.read();
  }
  magnetCount = Wire.read();

  if (oilerRunTime.parts[0] == 255) oilerRunTime.value = 1000;
  if (oilerRunTime.value > 6000) oilerRunTime.value = 6000;
  OilerStop.delayMs = oilerRunTime.value;
  
  if ((ttOdo.parts[0] >> 7) == 1) ttOdo.value = 0;
  if (ttOdo.value < 0) ttOdo.value = 0;
  if (ttGas.value < 0) ttGas.value = 0;
  if (ttTrip.value < 0) ttTrip.value = 0;
}

void saveData(Task* self) {
  Wire.beginTransmission(DS1307_CTRL_ID);
  Wire.write(DS1307_RAM_START);
  for (byte i = 0; i < 4; i++) {
    Wire.write(ttOdo.parts[i]);
  }
  for (byte i = 0; i < 4; i++) {
    Wire.write(ttGas.parts[i]);
  }
  for (byte i = 0; i < 4; i++) {
    Wire.write(ttTrip.parts[i]);
  }
  for (byte i = 0; i < 2; i++) {
    Wire.write(wheelLength.parts[i]);
  }
  for (byte i = 0; i < 2; i++) {
    Wire.write(oilerRunTime.parts[i]);
  }
  Wire.write(magnetCount);
  
  Wire.endTransmission();
}

void drawMainScreen() {
  // 1st string
  // Print speed
  lcd.setCursor(0, 0);
  lcd.print(char(0));

  if (currentSpeed < 100) {
    dtostrf(currentSpeed, 4, 1, buffer);
  } else {
    dtostrf(currentSpeed, 4, 0, buffer);
  }
  lcd.print(buffer);

  // Print TT data
  lcd.setCursor(5, 0);

  switch (screenMode) {
    case SCREEN_MODE_MAIN:
      lcd.print (F("G"));
      dtostrf(ttOdo.value, 7, 0, buffer);
      break;
    case SCREEN_MODE_MAIN_TT_GAS:
      lcd.print (char(1));
      if (ttGas.value > 999) {
        ttGas.value = 0;
      }
      dtostrf(ttGas.value, 7, 3, buffer);
      break;
    case SCREEN_MODE_MAIN_TT_TRIP:
      lcd.print (char(2));
      if (ttTrip.value < 9999) {
        dtostrf(ttTrip.value, 7, 2, buffer);
      } else if (ttTrip.value < 99999) {
        dtostrf(ttTrip.value, 7, 1, buffer);
      } else {
        dtostrf(ttTrip.value, 7, 0, buffer);
      }
      break;
  }
  lcd.print (buffer);
  lcd.print (" ");

  lcd.setCursor(14, 0);
  if (!oilerRunning)lcd.print(F("O")); else lcd.print(char(3));
  if (oilerInterval == OILER_STOPPED) {
    lcd.print("-");
  } else if (oilerInterval == OILER_RUNNING) {
    lcd.print("+");
  } else {
    sprintf(buffer, "%X", oilerInterval);
    lcd.print(buffer);
  }

  // END 1st string

  char divider = ' ';
  if ((millis() & 1023) > 350) divider=':';
  hours = RTC.get(DS1307_HR, true);
  minutes = RTC.get(DS1307_MIN, true);
  lcd.setCursor(11, 1);
  sprintf(buffer, "%02d%c%02d", hours, divider, minutes);

  lcd.print(buffer);

  printTemp();
}

void printTemp() {
  static int tAvg[8];
  int mid = 0;
  
  int thInput = analogRead(2);
  byte i = 7;
  do {
    tAvg[i] = tAvg[i-1];
    mid += tAvg[i];
    i--;
  }while(i > 0);
  mid += thInput; mid = mid >> 3;
  tAvg[0] = thInput;
    
  float therm;
  therm = 1023.0 / float(mid) - 1;
  therm = SERIESRESISTOR / therm;
  therm = log(therm / THERMISTORNOMINAL) / BCOEFFICIENT;
  therm += 1.0 / (TEMPERATURENOMINAL + 273.15);
  therm = 1.0 / therm;
  therm -= 273.15;
  
  lcd.setCursor(0, 1);
  sprintf(buffer, "%3d", int(therm));
  lcd.print(buffer);
  lcd.print(char(0xDF));
}

void drawSetupScreen() {
  lcd.setCursor(0, 0);
  lcd.print(setupMenu[currentMenuItem]);
  lcd.setCursor(0, 1);
  lcd.print(char(126)); lcd.print(" ");

  switch (currentMenuItem) {
    case MENU_TIME:
      sprintf(buffer, "%02d.%02d", hours, minutes);
      lcd.print (buffer);
      break;
    case MENU_WLEN:
      lcd.print(wheelLength.value);
      lcd.print(F(" mm    "));
      break;
    case MENU_MCOUNT:
      lcd.print(magnetCount);
      lcd.print(F("    "));
      break;
    case MENU_ODO:
      lcd.print(long(ttOdo.value));
      lcd.print(F(" km      "));
      break;
    case MENU_OILER_RUNTIME:
      dtostrf(float(oilerRunTime.value) / 1000, 2, 1, buffer);
      lcd.print(buffer);
      lcd.print(F(" sec  "));
      break;
  }
}

void setupScreenKeypress(unsigned int keyPressed, unsigned long keyPressedTime, boolean keyStillPressed) {
  switch (keyPressed) {
    case KEY_RIGHT:
    case KEY_LEFT:
    case KEY_DOWN:
    case KEY_UP:
      switch (currentMenuItem) {
        case MENU_TIME:
          //hours, minutes
          if (keyPressedTime < KEY_LONG_PRESS_TIME && !keyStillPressed) {
            switch (keyPressed) {
              case KEY_DOWN:
                minutes --; if (minutes > 59) minutes = 59;
                break;
              case KEY_UP:
                hours ++; if (hours > 23) hours = 0;
                break;
              case KEY_RIGHT:
                minutes ++; if (minutes > 59) minutes = 0;
                break;
              case KEY_LEFT:
                hours --; if (hours > 23) hours = 0;
                break;
            }

            RTC.stop();
            RTC.set(DS1307_HR, hours);
            RTC.set(DS1307_MIN, minutes);
            RTC.start();
          }

          break;
        case MENU_WLEN:
          if (keyPressedTime > KEY_LONG_PRESS_TIME && keyStillPressed) {
            switch (keyPressed) {
              case KEY_DOWN: wheelLength.value -= 5; break;
              case KEY_UP: wheelLength.value += 5; break;
              case KEY_RIGHT: wheelLength.value += 10; break;
              case KEY_LEFT: wheelLength.value -= 10; break;
            }
          } else {
            switch (keyPressed) {
              case KEY_DOWN: wheelLength.value --; break;
              case KEY_UP: wheelLength.value ++; break;
              case KEY_RIGHT: wheelLength.value += 15; break;
              case KEY_LEFT: wheelLength.value -= 15; break;
            }

          }
          getWheelSectorLength();
          break;
        case MENU_MCOUNT:
          if (!keyStillPressed) {
            switch (keyPressed) {
              case KEY_DOWN: magnetCount -= 1; break;
              case KEY_UP: magnetCount += 1; break;
              case KEY_LEFT: magnetCount -= 10; break;
              case KEY_RIGHT: magnetCount += 10; break;
            }
          }
          getWheelSectorLength();
          break;
        case MENU_ODO:
          if (keyPressedTime > KEY_LONG_PRESS_TIME && keyStillPressed) {
            switch (keyPressed) {
              case KEY_DOWN: ttOdo.value -= 5; break;
              case KEY_UP: ttOdo.value += 5; break;
              case KEY_RIGHT: ttOdo.value += 50; break;
              case KEY_LEFT: ttOdo.value -= 50; break;
            }
          } else {
            switch (keyPressed) {
              case KEY_DOWN: ttOdo.value --; break;
              case KEY_UP: ttOdo.value ++; break;
              case KEY_RIGHT: ttOdo.value += 5; break;
              case KEY_LEFT: ttOdo.value -= 5; break;
            }
          }
          break;
        case MENU_OILER_RUNTIME:
          if (keyPressedTime < KEY_LONG_PRESS_TIME && !keyStillPressed) {
            switch (keyPressed) {
              case KEY_DOWN: oilerRunTime.value -= 100; break;
              case KEY_UP: oilerRunTime.value += 100; break;
              case KEY_RIGHT: oilerRunTime.value += 300; break;
              case KEY_LEFT: oilerRunTime.value -= 300; break;
            }

            if (oilerRunTime.value < 200) oilerRunTime.value = 200; else if (oilerRunTime.value > 3000) oilerRunTime.value = 3000;
            OilerStop.delayMs = oilerRunTime.value;
          }
          break;
      }
      break;
    case KEY_SELECT:
      if (keyPressedTime > KEY_LONG_PRESS_TIME && !keyStillPressed) {
        lcd.clear();
        screenMode = SCREEN_MODE_MAIN;
        currentMenuItem = MENU_TIME;
      } else if (keyPressedTime < KEY_LONG_PRESS_TIME && !keyStillPressed) {
        currentMenuItem ++;

        if (currentMenuItem > MENU_ITEMS_COUNT-1)currentMenuItem = 0;
        lcd.clear();
      }
      break;
  }
}

void drawScreen(Task* self) {
  switch (screenMode) {
    case SCREEN_MODE_MAIN:
    case SCREEN_MODE_MAIN_TT_GAS:
    case SCREEN_MODE_MAIN_TT_TRIP:
      drawMainScreen();
      break;
    case SCREEN_MODE_SETUP:
      drawSetupScreen();
      break;
    default:
      lcd.clear();
  }

}

void mainScreenKeypress(unsigned int keyPressed, unsigned long keyPressedTime, boolean keyStillPressed) {
  switch (keyPressed) {
    case KEY_RIGHT:
      break;
    case KEY_LEFT:
      switch (screenMode) {
        case SCREEN_MODE_MAIN:
          screenMode = SCREEN_MODE_MAIN_TT_GAS;
          break;
        case SCREEN_MODE_MAIN_TT_GAS:
          screenMode = SCREEN_MODE_MAIN_TT_TRIP;
          break;
        case SCREEN_MODE_MAIN_TT_TRIP:
          screenMode = SCREEN_MODE_MAIN;
          break;
      }
      break;
    case KEY_DOWN:
    case KEY_UP:
      break;
    case KEY_SELECT:
      if (keyPressedTime > KEY_LONG_PRESS_TIME && keyStillPressed) {
        switch (screenMode) {
          case SCREEN_MODE_MAIN_TT_GAS:
            ttGas.value = 0;
            break;
          case SCREEN_MODE_MAIN_TT_TRIP:
            ttTrip.value = 0;
            break;
        }
      } else if (keyPressedTime < KEY_LONG_PRESS_TIME && !keyStillPressed) {
        lcd.clear();
        screenMode = SCREEN_MODE_SETUP;
      }
      break;
  }
}

void processKeyPress(unsigned int keyPressed, unsigned long keyPressedTime, boolean keyStillPressed) {

  if (keyPressed < KEY_RIGHT) {
    keyPressed = KEY_RIGHT;
  } else if (keyPressed < KEY_UP) {
    keyPressed = KEY_UP;
  } else if (keyPressed < KEY_DOWN) {
    keyPressed = KEY_DOWN;
  } else if (keyPressed < KEY_LEFT) {
    keyPressed = KEY_LEFT;
  } else if (keyPressed < KEY_SELECT) {
    keyPressed = KEY_SELECT;
  }

  switch (screenMode) {
    case SCREEN_MODE_MAIN:
    case SCREEN_MODE_MAIN_TT_GAS:
    case SCREEN_MODE_MAIN_TT_TRIP:
      mainScreenKeypress(keyPressed, keyPressedTime, keyStillPressed);
      break;
    case SCREEN_MODE_SETUP:
      setupScreenKeypress(keyPressed, keyPressedTime, keyStillPressed);
      break;
  }
}

void readKey(Task* self) {
  static int lastKeyState = KEY_RELEASED;
  static unsigned long keyPressStarted = 0;
  unsigned long keyPressedTime = 0;

  int currentKeyState = analogRead(0);

  if (currentKeyState < KEY_SELECT && lastKeyState == KEY_RELEASED) { // key just pressed
    lastKeyState = currentKeyState;
    keyPressStarted = millis();
  } else {
    keyPressedTime = millis() - keyPressStarted;

    if (currentKeyState > KEY_SELECT && lastKeyState != KEY_RELEASED) { // key released

      processKeyPress(lastKeyState, keyPressedTime, false);

      lastKeyState = KEY_RELEASED;
      keyPressStarted = 0;
    } else if (currentKeyState < KEY_SELECT && lastKeyState == currentKeyState && keyPressedTime > KEY_LONG_PRESS_TIME) { // key still pressed more than KEY_LONG_PRESS_TIME

      processKeyPress(lastKeyState, keyPressedTime, true);
    }
  }

}

void speedTicker() {
  ttOdo.value += wheelSectorLength;
  ttGas.value += wheelSectorLength;
  ttTrip.value += wheelSectorLength;
}


float getOilerDiff() {
  return float(map(oilerInterval, 1 - OILER_STOPPED, OILER_RUNNING - 1 , OILER_INTERVAL_MIN, OILER_INTERVAL_MAX)) / 1000;
}

void oilerStart() {
  // write '1' to MOSFET
  digitalWrite(2, HIGH);
  
  oilerRunning = true;
  OilerStop.startDelayed();
  nextOilStart = ttOdo.value + getOilerDiff();
}

boolean oilerStop(Task* self) {
  oilerRunning = false;
  
  // write '0' to MOSFET
  digitalWrite(2, LOW);
  return true;
}

void oilerCheck(Task* self) {
  static int prevOilerInterval = -1;

  oilerInterval = int( analogRead(1) / OILER_DIV );
  if (prevOilerInterval != oilerInterval) {
    prevOilerInterval = oilerInterval;
    nextOilStart = ttOdo.value + getOilerDiff();
  }

  if (oilerInterval == OILER_STOPPED || screenMode == SCREEN_MODE_SETUP) { // off
    oilerStop(&OilerStop);
  } else if (oilerInterval == OILER_RUNNING && !oilerRunning) { // on
    nextOilStart = 0;
    oilerStart();
  } else if (!oilerRunning && ttOdo.value > nextOilStart) {
    oilerStart();
  }
}

