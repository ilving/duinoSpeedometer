#define KEY_RIGHT 60
#define KEY_UP 200
#define KEY_DOWN 400
#define KEY_LEFT 600
#define KEY_SELECT 800

#define KEY_RELEASED 1023
#define KEY_LONG_PRESS_TIME 2500

#define LCD_X_SIZE 16
#define LCD_Y_SIZE 2

#define SCREEN_MODE_MAIN 0
#define SCREEN_MODE_MAIN_TT_GAS 1
#define SCREEN_MODE_MAIN_TT_TRIP 2
#define SCREEN_MODE_SETUP 3

#define MENU_ITEMS_COUNT 5
#define MENU_TIME 0
#define MENU_WLEN 1
#define MENU_MCOUNT 2
#define MENU_ODO 3
#define MENU_OILER_RUNTIME 4

#define OILER_STOPPED 0
#define OILER_RUNNING 7
#define OILER_DIV 128

#define OILER_INTERVAL_MIN 100
#define OILER_INTERVAL_MAX 6000

#define DS1307_RAM_START 0x0F

#define THERMISTORNOMINAL 100000
#define TEMPERATURENOMINAL 25
#define BCOEFFICIENT 4600
#define SERIESRESISTOR 56000

const byte symTable[][8] PROGMEM = {
  {B10100, B11000, B10100, B00000, B00000, B01010, B01110, B01010}, // km/h sym
  {B00001, B11101, B10101, B10101, B10101, B10110, B11100, B11100}, // fuel sym
  {B11100, B01000, B01000, B01000, B00110, B00101, B00110, B00101}, // trip sym
  {B01110, B11111, B11111, B11111, B11111, B11111, B01110, B00000}, // oiler on sym
  {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111}, // test sym
  {B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00000}, // blank sym
  {B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00000}, // blank sym
  {B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00000}, // blank sym
};
