#undef HAS_LCD
#define HAS_RF
#define HAS_ETH

#include <DS1307RTC.h>
#include <Time.h>
#include <Bounce.h>
#ifdef HAS_LCD
#include <LiquidCrystal.h>
#endif
#ifdef HAS_RF
#include <VirtualWire.h>
#endif
#ifdef HAS_ETH
#include <EtherCard.h>
#endif
#include "../include/FrankenStat.h"

#define DS1307_I2C_ADDRESS 0x68  // This is the I2C address
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
#ifdef HAS_LCD
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
#endif

#ifdef HAS_ETH
static byte myip[] = { 192,168,2,128 };
static byte gwip[] = { 192,168,2,1 };
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
byte Ethernet::buffer[500];
BufferFiller bfill;
#endif

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
  int hh = tm.Hour;
  int mm = tm.Minute;
  int yyyy = tm.Year;
  int mon = tm.Month;
  int day = tm.Day;

#ifdef HAS_LCD
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
#else
  Serial.print("##### ");
  Serial.print(days[tm.Wday]);
  Serial.print(" ");
  if(hh < 10) Serial.print('0');
  Serial.print(hh);
  Serial.print(":");
  if(mm < 10) Serial.print('0');
  Serial.println(mm);
  Serial.print("Target:  ");
  Serial.println(TargetTemp);
  Serial.print("Current: ");
  Serial.println(cur);
  Serial.println();
#endif
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

#ifdef HAS_ETH
//
// Return a JSON string with the thermostat status.
//
uint16_t JSON_status() {
  static char tempbuf[5];
  static char curbuf[5];

  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "{\"target\":$S, \"current\":$S, \"burn\":$S}"),
    dtostrf(TargetTemp,2,1,tempbuf),
    dtostrf(cur,2,1,curbuf),
    state ? "true" : "false");
  return bfill.position();
}

//
// Read the HTTP request and returns the corresponding reply.
//
int16_t process_request(char *str)
{
  int index = 0;
  int plen = 0;
  char ch = str[index];

  Serial.println(str);
  if (strncmp("GET ", str, 4)==0){
    Serial.println("GET");
  }
  return JSON_status();
}
#endif

void setup() {
  analogReference(EXTERNAL);
#ifdef HAS_LCD
  lcd.begin(16, 2);
#else
  Serial.begin(9600);
#endif
  pinMode(ButtonUp, INPUT);
  pinMode(ButtonDown, INPUT);
  pinMode(ThermPin, INPUT);
  pinMode(Burner, OUTPUT);

#ifdef HAS_RF
  // Setting up the RF Transmitter
  vw_set_tx_pin(RFTX);
  vw_set_ptt_inverted(true);
  vw_setup(2000);
#endif

#ifdef HAS_ETH
  if (ether.begin(sizeof Ethernet::buffer, mymac)) {
    ether.staticSetup(myip, gwip);
    Serial.println("Ethernet initialized!");
  }
  else {
    Serial.println("Ethernet not initialized!");
  }
#endif

  for(int i = 0; i < lastTempsz; i++) {
    lastTemps[i] = getTemp();
  }
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
#ifdef HAS_RF
    // Testcode. Sends a boolean true and false every second.
    onoff = !onoff;
    vw_send((uint8_t *)onoff, 1);
    vw_wait_tx();
#endif
  }

#ifdef HAS_ETH
  uint16_t plen, dat_p;

  if (ether.packetLoop(ether.packetReceive())) {
    dat_p = process_request((char *)&(Ethernet::buffer[dat_p]));
    if( dat_p ) ether.httpServerReply(dat_p);
  }
#endif
}
