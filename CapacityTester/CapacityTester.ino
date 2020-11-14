// Arduino Battery Capacity Tester 3.0
// ===================================
//
// Originally taken from Hesam Moshiri.
// (https://www.pcbway.com/blog/technology/Battery_capacity_measurement_using_Arduino.html)
// 
// Modified by Debasish Dutta.
// (https://www.opengreenenergy.com/post/arduino-battery-capacity-tester-v2-0) 
//
// Cleaned up and ported to Aleksei Dynda's lcdgfx library by J. R. Schmid.
// (https://github.com/sixtyfive/arduino-battery-capacity-tester-3.0)
//
// To the best of my knowledge the original idea for the hardware, and the very first
// sketch, came from Adam Welch, who had used a Nokia LCD and some protoboard for it.
// (http://static.adamwelch.co.uk/2016/01/lithium-ion-18650-battery-capacity-checker/)

#include <Arduino.h>
#include <lcdgfx.h>
#include <JC_Button.h>
#include <EasyBuzzer.h>

// Hisham's current steps with a 3R load (R7):
// const int current[] = {0, 37, 70, 103, 136, 169, 202, 235, 268, 301, 334,  367, 400, 440, 470, 500, 540};
// Debasish's current steps with a 1R0 load (R3):
// const int current[] = {0, 110, 210, 300, 390, 490, 580, 680, 770, 870, 960, 1000};
// J.R.'s measurements (rounded up/down) with 1R0 (R3) as well:
const int current[] = {
/*   0 */    0,
/*   5 */  100,
/*  10 */  200,
/*  15 */  300,
/*  20 */  400,
/*  25 */  500,
/*  30 */  600,
/*  35 */  700,
/*  40 */  800,
/*  45 */  900,
/*  50 */ 1000,
/*  55 */ 1100,
/*  60 */ 1200,
/*  65 */ 1300,
/*  70 */ 1400,
/*  75 */ 1500};

#define D2 2
#define D3 3
#define D9 9
#define D10 10
#define bat_pin A0
#define buzzer_pin D9
#define opamp_pin D10

#define ADC_MAX 1024
#define PWM_MAX  255
#define VCC 4.952 // TODO: measure and adjust before uploading!
#define VBAT_LOW 3.200

int pwm_val = 5; // = 100mA
unsigned long capacity = 0;
int adc_val = 0;
double vbat = 0;
bool working = false, done = false;

#define NOTE_C1      33
#define NOTE_CFLAT4 277
#define NOTE_G6    1568
#define NOTE_A7    1760
#define NOTE_B7    1976
#define NOTE_C7    2093
#define NOTE_D7    2349
#define NOTE_G7    3136
#define NOTE_G8    6272

#define FULL_NOTE                 250
#define HALF_NOTE      FULL_NOTE /  2
#define QUARTER_NOTE   FULL_NOTE /  4
#define EIGHTH_NOTE    FULL_NOTE /  8
#define SIXTEENTH_NOTE FULL_NOTE / 16

Button btn_up(D2, 25, false, true);
Button btn_dn(D3, 25, false, true);

char string[128]; // string formatting helper
char floatval[5] = "0.00"; // voltage formatting helper

DisplaySSD1306_128x32_I2C display(-1);

// 128x32px display has 4 lines at 8px font height
#define L1 1
#define L2 8
#define L3 16
#define L4 24

void setup()
{
  analogWrite(opamp_pin, pwm_val); // 0 => no flow of electricity through the MOSFET
  
  display.setFixedFont(ssd1306xled_font6x8);
  display.begin();
  display.getInterface().invertMode();
  
  EasyBuzzer.setPin(buzzer_pin);
  buzzHello();

  sayHello();
  
  btn_up.begin();
  btn_dn.begin();
}

void loop()
{
  btn_up.read();
  btn_dn.read();

  if (btn_up.wasReleased() && pwm_val <= 70 && working == false) {
    buzzButtonPressed();
    pwm_val = pwm_val + 5;
    printSetCurrent(pwm_val);
  }

  if (btn_dn.wasReleased() && pwm_val >= 10 && working == false) {
    buzzButtonPressed();
    pwm_val = pwm_val - 5;
    printSetCurrent(pwm_val);
  }
  
  if (btn_up.pressedFor(850) && working == false) {
    display.clear();
    buzzStart();
    analogWrite(opamp_pin, pwm_val); // open up the MOSFET
    timerInterrupt();
  }
}

/* printFixed():
 * - x (1 px res),
 * - y (8 px res),
 * - string (21 chars, then it wraps to next line, same x loc),
 * - style (STYLE_NORMAL, BOLD, ITALIC),
 * overwrites, but doesn't clear line or screen! */
void print(int col, int line, const char* string)
{
  int x = ((col-1) * 6) + 1;
  display.printFixed(x, line, string, STYLE_NORMAL);
}

/* there is also printFixedN, which takes another argument:
 * - fontsize (FONT_SIZE_2X; ) */
void printBig(int col, int line, const char* string)
{
  int x = ((col-1) * 12);
  display.printFixedN(x, line, string, STYLE_NORMAL, FONT_SIZE_2X);
}

void sayHello()
{
  display.clear();
  print(1,L1, "Arduino Battery");
  print(1,L2, "Capacity Tester");
  print(1,L4, "                 v3.1");
  
  lcd_delay(1200);
  
  display.clear();
  print(1,L1, "Load adjust: UP/DOWN");
  printSetCurrent(pwm_val);
  
  lcd_delay(50);
}

void printSetCurrent(int pwm_val)
{
  sprintf(string, "%imA   ", current[pwm_val / 5]);
  printBig(1,L3, string);
}

void timerInterrupt()
{
  working = true;
  
  uint8_t hour = 0, minute = 0, second = 0;
  
  while (done == false) {
    second++;
    
    if (second == 60) {
      second = 0;
      minute++;
    }
    
    if (minute == 60) {
      minute = 0;
      hour++;
    }
    
    sprintf(string, "%02d:%02d:%02d", hour, minute, second);
    print(8,L4, string);

    adc_val = analogRead(bat_pin);
    vbat = adc_val * (VCC / (double)ADC_MAX);

    dtostrf(vbat, 4, 2, floatval);
    sprintf(string, "%sV ", floatval);
    printBig(4,L2, string);

    if ((int)vbat == 0) {
      display.clear();
      print(1,L1, "Connect battery and");
      print(1,L2, "press RESET button.");
      delay(200); buzzError();
    }
    
    else if (vbat < VBAT_LOW) {
      display.clear();

      capacity =  (hour * 3600) + (minute * 60) + second;
      capacity = (capacity * current[pwm_val / 5]) / 3600;

      sprintf(string, "%lumAh", capacity);
      print(1,L1, "Run complete.");
      print(1,L2, "Capacity:");
      printBig(10-3-sizeof(string),L3, string);

      done = true;
      pwm_val = 0;
      analogWrite(opamp_pin, pwm_val); // cut the MOSFET

      delay(1000); buzzDone();
    }
 
    delay(1000); // one second
  }
}

void buzzHello()
{
  EasyBuzzer.beep(NOTE_G6); delay(FULL_NOTE);
  EasyBuzzer.stopBeep(); delay(SIXTEENTH_NOTE);
  EasyBuzzer.beep(NOTE_G7); delay(QUARTER_NOTE);
  EasyBuzzer.stopBeep();
}

void buzzStart()
{
  EasyBuzzer.beep(NOTE_C7); delay(EIGHTH_NOTE);
  EasyBuzzer.stopBeep(); delay(HALF_NOTE);
  EasyBuzzer.beep(NOTE_C7); delay(EIGHTH_NOTE);
  EasyBuzzer.stopBeep();
}

void buzzError()
{
  EasyBuzzer.beep(NOTE_CFLAT4); delay(FULL_NOTE);
  EasyBuzzer.stopBeep();
}

void buzzDone()
{
  EasyBuzzer.beep(NOTE_A7); delay(HALF_NOTE);
  EasyBuzzer.stopBeep(); delay(SIXTEENTH_NOTE);
  EasyBuzzer.beep(NOTE_B7); delay(HALF_NOTE);
  EasyBuzzer.stopBeep(); delay(SIXTEENTH_NOTE);
  EasyBuzzer.beep(NOTE_C7); delay(HALF_NOTE);
  EasyBuzzer.stopBeep(); delay(SIXTEENTH_NOTE);
  EasyBuzzer.beep(NOTE_D7); delay(HALF_NOTE);
  EasyBuzzer.stopBeep();
}

void buzzButtonPressed()
{
  EasyBuzzer.beep(NOTE_C1); delay(SIXTEENTH_NOTE);
  EasyBuzzer.stopBeep();
}
