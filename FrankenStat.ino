#include <DS1307RTC.h>
#include <Time.h>
#include <Wire.h>
#include <Bounce.h>
#include <LiquidCrystal.h>
#include <VirtualWire.h>

#define DS1307_I2C_ADDRESS 0x68  // This is the I2C address
#define BURNING 1
#define IDLE 0
#define RFTX 9

int   ThermPin     = 0;
int   ButtonDown   = 6;
int   ButtonUp     = 7;
int   Burner       = 13;
int   nsamp        = 50;
float TargetTemp   = 21;
float cur          = 0;
int   lastTemptr   = 0;
int   lastTempsz   = 15;
float lastTemps[]  = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                      0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
char *days[]       = { "", "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
boolean time       = true;
boolean onoff      = true;
tmElements_t tm;
char    state      = IDLE;
unsigned long last = millis();

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Initialize debouncer
Bounce up = Bounce(ButtonUp, 50);
Bounce down = Bounce(ButtonDown, 50);

// Read the temperature from the sensor.
float getTemp() {
  // We're measuring against 3.3V (which is actually 3.4V), in stead of the
  // expected 5V. So we will multiply by 0.68.
  float ThermValue = analogRead(ThermPin) * 0.68;
  float mVout=(float) ThermValue*5000.0/1023.0; //3.0V = 3000mV
  //float TempC=(mVout-400.0)/19.5; //Ta = (Vout-400mV)/19.5mV //Original
  float TempC=(mVout-390.0)/19.5; //Ta = (Vout-400mV)/19.5mV //Modified

  return TempC;
}

void getTime() {
  RTC.read(tm);
}

// Return the average temperature for the last minute.
float getAvgTemp() {
  float TempC = getTemp();
  lastTemps[lastTemptr] = TempC;
  if(++lastTemptr == lastTempsz) {
    lastTemptr = 0;
  }
  
  float avg = 0;
  for(int i = 0; i < lastTempsz; i++) {
    avg += lastTemps[i];
  }
  avg = avg / lastTempsz;
  
  //Serial.println(avg);
  
  return avg;
}

// Update the display.
void updateDisplay() {
  // Current and Target temperature
  lcd.setCursor(0, 0);
  lcd.print("T:      ");
  lcd.setCursor(2, 0);
  lcd.print(TargetTemp);
  lcd.setCursor(0, 1);
  lcd.print("C:      ");
  lcd.setCursor(2, 1);
  lcd.print(cur);
  
  // DoW
  lcd.setCursor(8,0);
  lcd.print(days[tm.Wday]);

  // Time
  int hh = tm.Hour;
  int mm = tm.Minute;
  int yyyy = tm.Year;
  int mon = tm.Month;
  int day = tm.Day;
  lcd.setCursor(11,0);
  if(hh < 10) lcd.print('0');
  lcd.print(hh);
  if(tm.Second % 2 == 0) {
    lcd.print(":");
  }
  else {
    lcd.print(" ");
  }
  if(mm < 10) lcd.print('0');
  lcd.print(mm);
}

void checkTemp() {
  cur = getAvgTemp();
  updateDisplay();

  switch (state) {
    case BURNING:
      if(cur > TargetTemp + 0.5) {
        digitalWrite(Burner, LOW);
        state = IDLE;
      }
      break;
    default:
      if(cur < TargetTemp - 0.5) {
        digitalWrite(Burner, HIGH);
        state = BURNING;
      }
      break;
  }
  
  time = true;
}

void setup() {
  analogReference(EXTERNAL);
  lcd.begin(16, 2);
  pinMode(ButtonUp, INPUT);
  pinMode(ButtonDown, INPUT);
  pinMode(ThermPin, INPUT);
  pinMode(Burner, OUTPUT);

  // Setting up the RF Transmitter
  //vw_setup(2000);
  //vw_set_tx_pin(RFTX);

  for(int i = 0; i < lastTempsz; i++) {
    lastTemps[i] = getTemp();
  }
  lcd.clear();
  getTime();
  last = millis();
}

void loop() {
  up.update();
  if(up.fallingEdge()) {
    TargetTemp += 0.5;
    updateDisplay();
  }
  
  down.update();
  if(down.fallingEdge()) {
    TargetTemp -= 0.5;
    updateDisplay();
  }
  
  if(millis() - last > 1000) {
    last = millis();
    getTime();
    checkTemp();
    updateDisplay();
    //onoff = ! onoff;
    //vw_send((uint8_t *)onoff, 1);
    //vw_wait_tx();
  }
}
