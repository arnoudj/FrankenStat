#include <DS1307RTC.h>
#include <Time.h>
#include <Wire.h>
#include <Bounce.h>
#include <TimerOne.h>
#include <LiquidCrystal.h>

#define DS1307_I2C_ADDRESS 0x68  // This is the I2C address
#define BURNING 1
#define IDLE 0

int   ThermPin    = 0;
int   ButtonDown  = 6;
int   ButtonUp    = 7;
int   Burner      = 8;
int   nsamp       = 50;
float TargetTemp  = 21;
float cur         = 0;
int   lastTemptr  = 0;
int   lastTempsz  = 15;
float lastTemps[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                      0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
char *days[]      = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
boolean time      = true;
boolean timeState = true;
tmElements_t tm;
int     state     = IDLE;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Initialize debouncer
Bounce up = Bounce(ButtonUp, 50);
Bounce down = Bounce(ButtonDown, 50);

// Read the temperature from the sensor.
float getTemp() {
  float ThermValue = analogRead(ThermPin);
  float mVout=(float) ThermValue*5000.0/1023.0; //3.0V = 3000mV
  //float TempC=(mVout-400.0)/19.5; //Ta = (Vout-400mV)/19.5mV //Original
  float TempC=(mVout-400.0)/19.5; //Ta = (Vout-400mV)/19.5mV //Modified

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
  lcd.setCursor(0, 0);
  lcd.print("T:      ");
  lcd.setCursor(2, 0);
  lcd.print(TargetTemp);
  lcd.setCursor(0, 1);
  lcd.print("C:      ");
  lcd.setCursor(2, 1);
  lcd.print(cur);
  
  lcd.setCursor(11,0);
  int hh = tm.Hour;
  int mm = tm.Minute;
  int yyyy = tm.Year;
  int mon = tm.Month;
  int day = tm.Day;
  if(hh < 10) lcd.print('0');
  lcd.print(hh);
  if(timeState) {
    timeState=false;
    lcd.print(" ");
  }
  else {
    timeState=true;
    lcd.print(":");
  }
  if(mm < 10) lcd.print('0');
  lcd.print(mm);
  lcd.setCursor(8,0);
  lcd.print(days[tm.Wday]);
}

void intTemp() {
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
  //Serial.begin(9600);
  lcd.begin(16, 2);
  pinMode(ButtonUp, INPUT);
  pinMode(ButtonDown, INPUT);
  pinMode(ThermPin, INPUT);
  pinMode(Burner, OUTPUT);
  digitalWrite(Burner, HIGH);
  delay(500);
  digitalWrite(Burner, LOW);
  for(int i = 0; i < lastTempsz; i++) {
    lastTemps[i] = getTemp();
  }
  getTime();
  Timer1.initialize(1000000);    // 1 second timer interrupt
  Timer1.attachInterrupt(intTemp);
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
  
  if(time == true) {
    time=false;
    getTime();
  }
}
