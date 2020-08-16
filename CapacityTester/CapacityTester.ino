// Arduino Battery Capacity Tester 3.0
// ===================================
//
// Originally taken from Hesam Moshiri 
// (https://www.pcbway.com/blog/technology/Battery_capacity_measurement_using_Arduino.html)
// 
// Modified by Debasish Dutta (deba168)
// (https://www.opengreenenergy.com/post/arduino-battery-capacity-tester-v2-0) 
//
// Cleaned up, back-ported to 1602 displays, and further modified by J. R. Schmid (sixtyfive)
// (https://github.com/sixtyfive/arduino-battery-capacity-tester-3.0)
//
// To the best of my knowledge the original idea for the hardware, and the very first
// sketch, came from Adam Welch, who had used a Nokia LCD and some protoboard for it.
// (http://static.adamwelch.co.uk/2016/01/lithium-ion-18650-battery-capacity-checker/)

#include <Arduino.h>
#include <Wire.h> 
#include <LiquidCrystal.h>
#include <JC_Button.h>
#include <EasyBuzzer.h>
 
// Hisham's current steps with a 3R load (R7):
// const int current[] = {0, 37, 70, 103, 136, 169, 202, 235, 268, 301, 334,  367, 400, 440, 470, 500, 540};
// deba168's current steps with a 1R load (R3):
// const int current[] = {0, 110, 210, 300, 390, 490, 580, 680, 770, 870, 960, 1000};
// sixtyfive's measurements (rounded up/down) with 1R (R3):
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
 
const byte D2 = 2, D3 = 3;
const byte D9 = 9, D10 = 10;
const byte RS = 11, EN = 12, D4 = 4, D5 = 5, D6 = 6, D7 = 7;

const int bat_pin = A0;
Button btn_up(D2, 25, false, true);
Button btn_dn(D3, 25, false, true);
const byte buzzer_pin = D9, opamp_pin = D10;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

#define SIXTEEN_SPACES "                "
byte pipe_char[] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
#define PIPE_CHAR 0
byte backslash_char[] = {0x00, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00};
#define BACKSLASH_CHAR 1

#define ADC_MAX 1024
#define PWM_MAX  255
#define VCC        5.160 // TODO: measure and adjust before uploading!
#define VBAT_LOW   3.200

int pwm_val = 0;
unsigned long capacity = 0;
int adc_val = 0;
float vbat = 0;
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

void buzz_hello()
{
  EasyBuzzer.beep(NOTE_G6); delay(FULL_NOTE);
  EasyBuzzer.stopBeep(); delay(SIXTEENTH_NOTE);
  EasyBuzzer.beep(NOTE_G7); delay(QUARTER_NOTE);
  EasyBuzzer.stopBeep();
}

void buzz_start()
{
  EasyBuzzer.beep(NOTE_C7); delay(EIGHTH_NOTE);
  EasyBuzzer.stopBeep(); delay(HALF_NOTE);
  EasyBuzzer.beep(NOTE_C7); delay(EIGHTH_NOTE);
  EasyBuzzer.stopBeep();
}

void buzz_error()
{
  EasyBuzzer.beep(NOTE_CFLAT4); delay(FULL_NOTE);
  EasyBuzzer.stopBeep();
}

void buzz_done()
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

void buzz_button_pressed()
{
  EasyBuzzer.beep(NOTE_C1); delay(SIXTEENTH_NOTE);
  EasyBuzzer.stopBeep();
}

void lcd_line1_clear()
{
  lcd.setCursor(0, 1); lcd.print(SIXTEEN_SPACES); lcd.setCursor(0, 1);
}

void setup()
{
  // Serial.begin(9600);
  // Serial.println("Hello there!");

  EasyBuzzer.setPin(buzzer_pin);

  analogWrite(opamp_pin, pwm_val); // 0 => no flow of electricity through the MOSFET
 
  btn_up.begin();
  btn_dn.begin();
 
  lcd.begin(16, 2);
	lcd.createChar(PIPE_CHAR, pipe_char);
	lcd.createChar(BACKSLASH_CHAR, backslash_char);
	lcd.home();

  buzz_hello();

  lcd.print("Lithium Battery"); lcd.setCursor(0, 1);
  lcd.print("Capacity Tester"); delay(850);
  lcd.clear();

  lcd.print("Load adj: UP/DN");
  lcd.setCursor(0, 1); lcd.print("0mA");
}
 
void loop() {
  btn_up.read();
  btn_dn.read();

  if (btn_up.wasReleased() && pwm_val <= 70 && working == false) {
    buzz_button_pressed();
    pwm_val = pwm_val + 5;
    // analogWrite(opamp_pin, pwm_val);

    lcd_line1_clear();
    lcd.print(String(current[pwm_val / 5]) + "mA");
  }

  if (btn_dn.wasReleased() && pwm_val >= 5 && working == false) {
    buzz_button_pressed();
    pwm_val = pwm_val - 5;
    // analogWrite(opamp_pin, pwm_val);

    lcd_line1_clear();
    lcd.print(String(current[pwm_val / 5]) + "mA");
  }
  
  if (btn_up.pressedFor(850) && working == false) {
    lcd.clear();
    buzz_start();

    analogWrite(opamp_pin, pwm_val); // open up the MOSFET
    timerInterrupt();
  }
}
 
void timerInterrupt()
{
  working = true;
  
  uint8_t hour = 0, minute = 0, second = 0;
  char c[16]; // string formatting helper
  char spinner = '/';
  
  while (done == false) {
    second++;
    
    if (second == 60) {
      second = 0;
      minute++;
      lcd.clear();
    }
    
    if (minute == 60) {
      minute = 0;
      hour++;
    }
    
    sprintf(c, "%d:%02d:%02d", hour, minute, second);
    lcd.home(); lcd.print(c);

    adc_val = analogRead(bat_pin);
    vbat = adc_val * (VCC / (float)ADC_MAX);

    lcd.setCursor(11, 0); lcd.print(String(vbat) + "V");
    switch (spinner) {
      case '/':            spinner = '-';            break;
      case '-':            spinner = BACKSLASH_CHAR; break;
      case BACKSLASH_CHAR: spinner = PIPE_CHAR;      break;
      case PIPE_CHAR:      spinner = '/';            break; }
    lcd.setCursor(0, 1); lcd.write(spinner);
 
    if (vbat == (float)0) {
      lcd_line1_clear();
      lcd.print("Err: no battery.");
      lcd.print("> Insert, reset!");

      delay(200); buzz_error();
    } else if (vbat < VBAT_LOW) {
      // Serial.println(vbat);
      lcd_line1_clear();

      capacity =  (hour * 3600) + (minute * 60) + second;
      capacity = (capacity * current[pwm_val / 5]) / 3600;

      lcd.print("C-bat: " + String(capacity) + "mAh");

      done = true;
      pwm_val = 0;
      analogWrite(opamp_pin, pwm_val); // again, no flow of electricity through the MOSFET

      delay(1000); buzz_done();
    }
 
    delay(1000); // one second
  }
}
