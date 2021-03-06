/*
 Fish Tank Minder - aquarium control 

 project uses the 16x2 LCD display.  The LiquidCrystal
 library works with all LCD displays that are compatible with the
 Hitachi HD44780 driver. There are many of them out there, and you
 can usually tell them by the 16-pin interface.

 This sketch schedules regular water changes and interfaces with 2 pumps and 2 sensors.
 Pump one is a submersible pump inside a fresh (dechlorinated ) water reservoir.  The second pump is 
 a submerisble pump inside the fish tank that will be used to drain a portion of the water. 
 One sensor is located on the waste water tank and determines when the tank is full during 
 the drain step. 
 The other sensor is located inside the fish tank and is used to detect when the desired
 water level in the tank is reached during hte fill step.  
 The scheduler keeps track of the # of days since the last change (1-30) and the 
 desired change intervale (0-30).  A setting of 0 for "change days" turns off automatic
 water changes but the # of days since last change will still be tracked.  

  The display circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * LCD VSS pin to ground
 * LCD VCC pin to 5V
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)

 * relay to fill pump (pin 7) 
 * relay to drain pump (pin 8)
 * sensor for aquarium water level (pin 6)  
 * sensor for waste water level (pin 7)  
 * keypad input (A0)

 This example code is in the public domain.

 http://www.arduino.cc/en/Tutorial/LiquidCrystalHelloWorld

*/

// include the library code:
#include <LiquidCrystal.h>
#include <EEPROM.h>

// input keys
#define UP 2 
#define DOWN 0 
#define LEFT 4 
#define RIGHT 6 
#define SELECT 5 

// index into variables array
#define DAY 1 
#define CHG_DAY 0 
#define MAX_DRAIN_SEC 2
#define MAX_FILL_SEC 3

#define MAX_DAY 30
#define DAYINMILSECONDS 86400 

// pin outs
#define TANK_LEVEL_INPUT 6
#define WASTE_LEVEL_INPUT 7
#define FILL_PUMP 8
#define DRAIN_PUMP 9

#define FULL 1 
#define LOW 0 

#define ON LOW
#define OFF HIGH
#define MAX_VAR 4 

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

char menuOptions[][50] = {"set change day", "set day", "max drain", "max fill"};
char maxOption = 3; 
char minOption = 0; 

int variable[4] = {5, 1, 10, 10}; // default 10 days to change 
char option=01;
long lastRead = 0;
long started;

int tankLev; 
int wasteLev; 
int sensors[] = { TANK_LEVEL_INPUT, WASTE_LEVEL_INPUT };
char error[50];
bool isError = false;
char buff[20];
byte customChar[9];

// characters to show tank filling or draining
void createCustomChars () { 
  for (int y=9; y >0 ; y--) { 
      for (int x =1; x < 9; x++) { 
           if (y > x) { 
              customChar[x] = 0x00;

           } else if (y+1 > x ) { 
              // surface motion
              customChar[x] = (0x66 >> y) & 0x1f;

           }else { 
           
              customChar[x] = 0x1f;

           }
      }
      lcd.createChar(9-y, customChar);
      lcd.setCursor(0,0);
    /*  lcd.write(9-y);
      lcd.setCursor(0,1);
      lcd.print(9-y);
      delay(200); */
  }
}

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  Serial.begin(9600);

  pinMode(TANK_LEVEL_INPUT, INPUT_PULLUP);
  pinMode(WASTE_LEVEL_INPUT, INPUT_PULLUP);
  pinMode(13, OUTPUT);

  pinMode(FILL_PUMP, OUTPUT);
  pinMode(DRAIN_PUMP, OUTPUT);
  digitalWrite(FILL_PUMP, OFF);
  digitalWrite(DRAIN_PUMP, OFF); 

  readSettings();
  createCustomChars();
  // Print a message to the LCD.
  started = millis();
}

void readSensors() { 
    for (int x=0; x < 2; x++) {
       int level = digitalRead(sensors[x]);
       if (x == 0) { 
           tankLev = level;
          
       } 
       if (x == 1 ) {
           wasteLev = level;
       }
       
      
      Serial.print(" tankLev = ");
      Serial.print(tankLev);
      Serial.print(" wasteLev = ");
      Serial.println(wasteLev);
    }
}

int readInput () {
   int sensorValue = analogRead(A0); // lcd keypad input
   int ret = 0xffff; 

   if (sensorValue > 1000 and sensorValue < 1030 ) {
       return ret;
   }

   if (sensorValue < 100) { 
      ret = RIGHT;
   } else if (sensorValue < 150) { 
      ret = UP;
   } else if (sensorValue < 410) { 
      ret = DOWN;
   } else if (sensorValue < 510) { 
      ret = LEFT;
   } else if (sensorValue < 800) { 
      ret = SELECT;   
   }
   return ret;  
}
void processInput() {
  int input = readInput(); 
  if (input != 0xffff) {  //debounce the buttons
      if (millis() - lastRead < 300 ) {
        input = 0xffff; 
      } else { 
          Serial.println(input);
          lastRead = millis();
      }
  } else if ( (millis() - lastRead) > 7000) { // default screen 
    //  lcd.clear();
      sprintf(buff,"day %d              ", variable[DAY]);
      lcd.print(buff);
      lcd.setCursor(0, 1);

      sprintf(buff,"chg day %d    ", variable[CHG_DAY]);
      lcd.print(buff);
  }
  //Serial.println((millis() - lastRead));

  // Menu controls
  if (input == UP and option < maxOption) { 
      lcd.clear();
      option ++;

      lcd.print(menuOptions[option]);
      lcd.setCursor(0, 1);
      sprintf(buff, "%2d  ", variable[option]);
      lcd.print(buff);
      lastRead = millis();

  } else if (input == DOWN and option > minOption) { 
      lcd.clear();
      option --;

      lcd.print(menuOptions[option]);
      lcd.setCursor(0, 1);
      sprintf(buff, "%2d  ", variable[option]);
      lcd.print(buff);

      lastRead = millis();

  } else if (input == LEFT) {
      variable[option]--;
      if (variable[option] < 0 ) {
          variable[option] = 0; 
      }
      lcd.setCursor(0, 1); 
      char bf[10];
      sprintf(bf, "%2d     ",variable[option]);
      lcd.print(bf);
      //lcd.print("  "); 
      
          
      
  } else if (input == RIGHT) { 
      variable[option] ++;
      if (variable[option] > MAX_DAY) {
         variable[option] = MAX_DAY;
      }
      lcd.setCursor(0, 1);
      char bf[10];
      sprintf(bf, "%2d     ",variable[option]);
      lcd.print(bf);
      
  } else if (input == SELECT) { 
      lcd.clear();
      lcd.setCursor(0, 1);
      char bf[10];
      sprintf(bf, "Write settings");
      lcd.print(bf);
      for (int x=0; x < MAX_VAR; x++) {
         if (x != DAY) { 
            EEPROM.update(x, variable[x]);
         }
      }  
      delay(500); 
      for (int x=0; x < MAX_VAR; x++) {
         if (x != DAY) { 
            EEPROM.update(x, variable[x]);
         }
      }  
       for (int x=0; x < MAX_VAR; x++) {
            Serial.print(" x = ");
            Serial.print(x);
            Serial.print(" val = ");
            Serial.println(EEPROM.read(x));
       }
       lcd.clear();
  }
}

void readSettings() {
   char val;
   for (int x=0; x < MAX_VAR; x++) {
            Serial.print(" x = ");
            Serial.print(x);
            Serial.print(" val = ");
            val = EEPROM.read(x);
            if (val >= 0 and val < 30) {
                 variable[x] = val;

            } else { 
              Serial.print(" x = ");
              Serial.print(x);
              Serial.print(" val = ");
              Serial.print(val);
              Serial.print("  invalid!");
            }
       }
       lcd.clear();
       
}

void loop() {
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 0);
  // print the number of seconds since reset:

  readSensors();
  processInput();

  // Check for time to change !
  if (variable[DAY] == variable[CHG_DAY]) {
      if (wasteLev == FULL) { 
          isError = true;
          
      }  else { 
         lcd.clear();
         lcd.setCursor(0, 0);
         lcd.print("Change water");
         lcd.setCursor(0, 1);
         lcd.print("    drain");

         for(int x=variable[MAX_DRAIN_SEC], y=9; x > 0 && wasteLev != FULL ; x--, y--) { 
             digitalWrite(DRAIN_PUMP, ON);

             lcd.setCursor(13, 1);
             sprintf(buff,"%2d  ", x);
             lcd.print(buff);    
             lcd.setCursor(15,0);
             lcd.write(y );      
             delay(1000);
             readSensors();
             if (y == 2) { 
                y = 9;
             }
         }
         digitalWrite(DRAIN_PUMP, OFF);

         lcd.clear();
         lcd.setCursor(0, 0);

         lcd.print("Change water");

         lcd.setCursor(0, 1);
         lcd.print("    fill");
         for(int x=variable[MAX_FILL_SEC], y=2; x > 0 && tankLev != FULL ; x--, y++) { 
             
             digitalWrite(FILL_PUMP, ON);
             
             lcd.setCursor(13, 1);
             sprintf(buff,"%2d  ", x);
             lcd.print(buff);
             lcd.setCursor(15,0);
             lcd.write(y   );
             delay(1000);
             readSensors();  
             if (y == 9) { 
                y = 2;
             }
             
         }
         digitalWrite(FILL_PUMP, OFF);
         variable[DAY] = 1;
         started = millis();
         lcd.clear();
      } 
           
      
  }
  
   // increment current DAY 
  if ((millis() - started) > DAYINMILSECONDS) { 
     variable[DAY]++;
     started = millis(); 
     Serial.println(started);
     Serial.println(millis() - started) ; 
  
  }
  

  delay(100);
}
