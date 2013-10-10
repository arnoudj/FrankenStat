#define DEBUG
#define HAS_LCD
#define HAS_RF
#define HAS_ETH
#define HAS_PROWL

#include <DS1307RTC.h>
#include <Time.h>
#include <Bounce.h>
#ifdef HAS_LCD
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#endif
#ifdef HAS_RF
#include <VirtualWire.h>
#endif
#ifdef HAS_ETH
#include <EtherCard.h>
#endif
#include "../include/FrankenStat.h"
#ifdef HAS_PROWL
#include "../include/prowl.h"
#endif

#define DS1307_I2C_ADDRESS 0x68  // This is the I2C address
#define RFTX 9

int   ThermPin     = 0;
int   ButtonDown   = 6;
int   ButtonUp     = 7;
int   Burner       = 13;
int   nsamp        = 50;
float cur          = 0;
float tgt          = 16;
float tmptgt       = 16;
int   lastTemptr   = 0;
int   lastTempsz   = 15;
float lastTemps[]  = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
char *days[]       = { "", "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
boolean time       = true;
boolean onoff      = true;
tmElements_t tm;
//char    state      = IDLE;
unsigned long last = millis();

#ifdef HAS_PROWL
Stash stash;
#endif

// initialize the library with the numbers of the interface pins
#ifdef HAS_LCD
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
#endif

#ifdef HAS_ETH
static byte myip[] = { 192,168,2,128 };
static byte gwip[] = { 192,168,2,1 };
static byte dnsip[] = { 192,168,2,254 };
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
byte Ethernet::buffer[250];
BufferFiller bfill;
#endif

// Initialize the schedule.
schedule_t schedule[7][4] = {
  // 00 Sunday
  {
    { 1, 10, 0, 21 },
    { 1, 23, 2, 12 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 }
  },
  // 01 Monday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 12 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // 02 Tuesday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 12 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // 03 Wednesday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 12 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // 04 Thursday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 12 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // 05 Friday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 12 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // 06 Saturday
  {
    { 1, 10, 0, 21 },
    { 1, 23, 2, 12 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 }
  }
};

// Initialize debouncer
Bounce up = Bounce(ButtonUp, 50);
Bounce down = Bounce(ButtonDown, 50);

bit_t bit = { BITSERIAL, false };

int hm2min(int h, int m) {
  return h * 60 + m;
}

int hq2min(int h, int q) {
  return h * 60 + q * 15;
}

// Get the last set temperature for a given day.
int lastTemp(int d) {
  if(d < 0) {
    d = 6;
  }
  for(int i = 3; i >= 0; i--) {
    if(schedule[d][i].active == 1) {
      return schedule[d][i].temp;
    }
  }
  return 0;
}

float scheduledTemp() {
  for(int i = 3; i >= 0; i--) {
    if(schedule[tm.Wday][i].active == 0) {
      continue;
    }
    if(hq2min(schedule[tm.Wday][i].hour, schedule[tm.Wday][i].quarters) <
       hm2min(tm.Hour,tm.Minute)) {
      return (float)schedule[tm.Wday][i].temp / 2 + 10;
    }
  }
  return (float)lastTemp(tm.Wday-1) / 2 + 10;
}

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
  lcd.print("T:");
  lcd.print(tgt);
  lcd.setCursor(0, 1);
  lcd.print("C:");
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
#endif

#ifdef DEBUG
  Serial.print("##### ");
  Serial.print(days[tm.Wday]);
  Serial.print(" ");
  if(hh < 10) Serial.print('0');
  Serial.print(hh);
  Serial.print(":");
  if(mm < 10) Serial.print('0');
  Serial.println(mm);
  Serial.print("Target:  ");
  Serial.println(tgt);
  Serial.print("Current: ");
  Serial.println(cur);
  Serial.println();
#endif
}

#ifdef HAS_PROWL
void sendProwl() {
//  if(ether.dnsLookup(prowl_domain)) {
    byte sd = stash.create();
    stash.print("apikey=");
    stash.print(PROWL_APIKEY);
    stash.print("&application=FrankenTherm&event=i");
    stash.print(bit.burn ? "Burning" : "Idle");
    stash.print("&description=");
    stash.print(bit.burn ? "Started burning." : "Stopped burning.");

    Stash::prepare(PSTR("POST http://$F/api.prowlapp.com/publicapi/add" "\r\n"
      "Host: $F" "\r\n"
      "Content-Type: application/x-www-form-urlencoded" "\r\n"
      "Content-Length: $D" "\r\n"
      "\r\n"
      "$H"
    ),
    "api.prowlapp.com", stash.size(), sd);
    ether.tcpSend();
//  }
//  else {
//#ifdef DEBUG
//    Serial.println("Could not resolve domain.");
//#endif
//  }
}
#endif

void checkTemp() {
  tgt = scheduledTemp();
  cur = getAvgTemp();
  updateDisplay();

  switch (bit.burn) {
    case BURNING:
      if(cur > tgt + 0.5) {
        digitalWrite(Burner, LOW);
        if(bit.burn != IDLE) {
#ifdef DEBUG
          Serial.println(PSTR("Burner off"));
#endif
#ifdef HAS_PROWL
          sendProwl();
#endif
          bit.burn = IDLE;
        }
      }
      break;
    default:
      if(cur < tgt - 0.5) {
        digitalWrite(Burner, HIGH);
        if(bit.burn != BURNING) {
#ifdef DEBUG
          Serial.println(PSTR("Burner on"));
#endif
#ifdef HAS_PROWL
          sendProwl();
#endif
          bit.burn = BURNING;
        }
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
    dtostrf(tgt,2,1,tempbuf),
    dtostrf(cur,2,1,curbuf),
    bit.burn ? "true" : "false");
  return bfill.position();
}

//
// Read the HTTP request and returns the corresponding reply.
//
int16_t process_request(char *str)
{
  if (strncmp("GET /up ", str, 8)==0){
    tmptgt += 0.5;
  }
  if (strncmp("GET /down ", str, 10)==0){
    tmptgt -= 0.5;
  }
  return JSON_status();
}
#endif

void setup() {
  analogReference(EXTERNAL);
#ifdef HAS_LCD
  lcd.begin(16,2);
  lcd.clear();
#endif

#ifdef DEBUG
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
    ether.staticSetup(myip, gwip, dnsip);
    ether.printIp("IP:  ", ether.myip);
    ether.printIp("GW:  ", ether.gwip);  
    ether.printIp("DNS: ", ether.dnsip);
  }
  else {
    Serial.println(PSTR("Ethernet not initialized!"));
  }
  //while (ether.clientWaitingGw())
  //  ether.packetLoop(ether.packetReceive());
  //Serial.println("Gateway found");
  //if(!ether.dnsLookup(PSTR("api.prowlapp.com"))) {
  //  Serial.println("DNS fail!");
  //}
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
    tmptgt += 0.5;
    updateDisplay();
  }
  
  down.update();
  if(down.fallingEdge()) {
    tmptgt -= 0.5;
    updateDisplay();
  }

  if(millis() - last > 1000) {
    last = millis();
    getTime();
    checkTemp();
    updateDisplay();
#ifdef HAS_RF
    vw_send((uint8_t *)&bit, sizeof(bit));
    vw_wait_tx();
#ifdef DEBUG
    Serial.print("Message size: ");
    Serial.println(sizeof(bit));
#endif
#endif
  }

#ifdef HAS_ETH
  uint16_t dat_p;

  if (dat_p = ether.packetLoop(ether.packetReceive())) {
    dat_p = process_request((char *)&(Ethernet::buffer[dat_p]));
    if( dat_p ) ether.httpServerReply(dat_p);
  }
#endif
}
