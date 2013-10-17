// Turn on or off certain functions.
#undef DEBUG
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

// I2C addresses
#define DS1307_I2C_ADDRESS 0x68
#define LCD_I2C_ADDRESS 0x27

// Pin assignment
#define PIN_THERM 0
#define PIN_DOWN 6
#define PIN_UP 7
#define PIN_ETH_CS 8
#define PIN_RFTX 9
#define PIN_ETH_SI 11
#define PIN_ETH_SO 12
#define PIN_ETH_SCK 13

float cur          = 0.0;
float tgt          = 16.0;  // Current target temperature
float tmptgt       = 16.0;  // Target temperature when a temporary
int   lastTemptr   = 0;
int   lastTempsz   = 15;
float lastTemps[]  = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
char *days[]       = { "", "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
//char *modes[]       = { "AUTO", "TEMP", "MANUAL", "HOLIDAY", "NOFROST" };
tmElements_t tm;
//char    state      = IDLE;
unsigned long last = millis();
char  mode = 0;

typedef struct schedulepointer_ {
  unsigned char day : 4;
  unsigned char line : 4;
} schedulepointer_t;

// sp_now holds the current schedule, sp_tmp holds the schedule at which
// the temporary temperature was set.
schedulepointer_t sp_now, sp_tmp;

#ifdef HAS_PROWL
Stash stash;
#endif

// initialize the library with the numbers of the interface pins
#ifdef HAS_LCD
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
#endif

#ifdef HAS_ETH
static byte myip[] = { 192,168,2,128 };
static byte gwip[] = { 192,168,2,254 };
static byte dnsip[] = { 192,168,2,254 };
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
byte Ethernet::buffer[250];
BufferFiller bfill;
#endif

// Initialize the schedule.
schedule_t schedule[7][4] = {
  // Monday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 12 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // Tuesday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 12 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // Wednesday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 12 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // Thursday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 50 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // Friday
  {
    { 1, 8, 0, 19 },
    { 1, 8, 2, 12 },
    { 1, 18, 2, 21 },
    { 1, 23, 2, 12 }
  },
  // Saturday
  {
    { 1, 10, 0, 21 },
    { 1, 23, 2, 12 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 }
  },
  // Sunday
  {
    { 1, 10, 0, 21 },
    { 1, 23, 2, 12 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 }
  }
};

// Initialize debouncer
Bounce up = Bounce(PIN_UP, 50);
Bounce down = Bounce(PIN_DOWN, 50);

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
    if(schedule[d-1][i].active == 1) {
      sp_now.day = tm.Wday;
      sp_now.line = i;
      return schedule[d-1][i].temp;
    }
  }
  return 0;
}

float scheduledTemp() {
  float target;

  for(int i = 3; i >= 0; i--) {
    if(schedule[tm.Wday-1][i].active == 0) {
      continue;
    }
    if(hq2min(schedule[tm.Wday-1][i].hour, schedule[tm.Wday-1][i].quarters) <
       hm2min(tm.Hour,tm.Minute)) {
      sp_now.day = tm.Wday;
      sp_now.line = i;
      target = (float)schedule[tm.Wday-1][i].temp / 2 + 10;
    }
  }
  target = (float)lastTemp(tm.Wday-1) / 2 + 10;

  switch(mode) {
    case MODE_TEMP:
      if(sp_now.day == sp_tmp.day and sp_now.line == sp_tmp.line) {
        // Still on the same setpoint on which we set the temporary
        // temperature.
        tgt = tmptgt;
      }
      else {
        mode = MODE_AUTO;
        break;
      }
    case MODE_HOLIDAY:
      return tmptgt;
  }

  tgt = target;
}

// Read the temperature from the sensor.
float getTemp() {
  // We're measuring against 3.3V (which is actually 3.4V), in stead of the
  // expected 5V. So we will multiply by 0.68.
  float ThermValue = analogRead(PIN_THERM) * 0.68;
  float mVout=(float) ThermValue*5000.0/1023.0; //3.0V = 3000mV
  //float TempC=(mVout-400.0)/19.5; //Ta = (Vout-400mV)/19.5mV //Original
  float TempC=(mVout-390.0)/19.5; //Ta = (Vout-400mV)/19.5mV //Modified

  return TempC;
}

void getTime() {
  RTC.read(tm);
}

// Return the average temperature for the last minute.
void getAvgTemp() {
  lastTemps[lastTemptr] = getTemp();
  if(++lastTemptr == lastTempsz) {
    lastTemptr = 0;
  }
  
  float avg = 0;
  for(int i = 0; i < lastTempsz; i++) {
    avg += lastTemps[i];
  }
  avg = avg / lastTempsz;
  
  //Serial.println(avg);
  
  cur = avg;
}

// Update the display.
void updateDisplay() {
  int hh = tm.Hour;
  int mm = tm.Minute;

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
  Serial.print(days[tm.Wday]);
  Serial.print(" ");
  if(hh < 10) Serial.print('0');
  Serial.print(hh);
  Serial.print(":");
  if(mm < 10) Serial.print('0');
  Serial.println(mm);
  Serial.print(F("tgt: "));
  Serial.println(tgt);
  Serial.print(F("cur: "));
  Serial.println(cur);
  Serial.print(F("mod: "));
  Serial.println((int)mode);
  Serial.println();
#endif
}

#ifdef HAS_PROWL
void sendProwl() {
  if(ether.dnsLookup(prowl_domain)) {
    byte sd = stash.create();
    stash.print("apikey=");
    stash.print(PROWL_APIKEY);
    stash.print("&application=FrankenTherm&event=");
    if(bit.burn) {
      stash.print("Idle");
    }
    else {
      stash.print("Burning");
    }
    stash.save();
    /*
    stash.print("&description=");
    if(bit.burn) {
      stash.print("Started burning.");
    }
    else {
      stash.print("Stopped burning.");
    }
    */

    Stash::prepare(PSTR("POST /publicapi/add HTTP/1.1" "\r\n"
      "User-Agent: FrankenStat/0.1.0" "\r\n"
      "Host: $F" "\r\n"
      "Content-Type: application/x-www-form-urlencoded" "\r\n"
      "Content-Length: $D" "\r\n"
      "\r\n"
      "$H"
    ),
    prowl_domain, stash.size(), sd);
    int session = ether.tcpSend();
    const char* reply = ether.tcpReply(session);
    if(reply != 0) {
      Serial.println(F(">>> RESPONSE RECEIVED ---"));
      Serial.println(reply);
    }
    else {
      Serial.println(F(">>> Prowl OK"));
    }
  }
  else {
    mode=99;
  }
}
#endif

void checkTemp() {
  scheduledTemp();
  getAvgTemp();
  updateDisplay();

  switch (bit.burn) {
    case BURNING:
      if(cur > tgt + 0.5) {
        if(bit.burn != IDLE) {
#ifdef DEBUG
          Serial.println("Off");
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
        if(bit.burn != BURNING) {
#ifdef DEBUG
          Serial.println("On");
#endif
#ifdef HAS_PROWL
          sendProwl();
#endif
          bit.burn = BURNING;
        }
      }
      break;
  }
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
    "{\"target\":$S, \"current\":$S, \"burn\":$S, \"mode\":$D}\r\n"),
    dtostrf(tgt,2,1,tempbuf),
    dtostrf(cur,2,1,curbuf),
    bit.burn ? "true" : "false",
    mode
  );
  return bfill.position();
}

// Temporarily set a new temperature until the next setpoint.
void setModeTemp() {
  if(mode == MODE_AUTO) {
    mode = MODE_TEMP;
    tmptgt = tgt;
    sp_tmp.day = sp_now.day;
    sp_tmp.line = sp_now.line;
  }
}

//
// Read the HTTP request and returns the corresponding reply.
//
int16_t process_request(char *str)
{
  if (strncmp("GET /up ", str, 8)==0){
    setModeTemp();
    tmptgt += 0.5;
  }
  if (strncmp("GET /down ", str, 10)==0){
    setModeTemp();
    tmptgt -= 0.5;
  }
  scheduledTemp();
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
  Serial.begin(115200);
#endif

  pinMode(PIN_UP, INPUT);
  pinMode(PIN_DOWN, INPUT);
  pinMode(PIN_THERM, INPUT);

#ifdef HAS_RF
  // Setting up the RF Transmitter
  vw_set_tx_pin(PIN_RFTX);
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
  while (ether.clientWaitingGw())
    ether.packetLoop(ether.packetReceive());
  Serial.println(F("Ethernet ready!"));
#endif

  for(int i = 0; i < lastTempsz; i++) {
    lastTemps[i] = getTemp();
  }
  getTime();
  last = millis();
}

void loop() {
  //
  // Check if buttons are pressed
  //
  up.update();
  if(up.fallingEdge()) {
    setModeTemp();
    tmptgt += 0.5;
    updateDisplay();
  }
  
  down.update();
  if(down.fallingEdge()) {
    setModeTemp();
    tmptgt -= 0.5;
    updateDisplay();
  }

  //
  // Every second check the temperature, update the display, and send
  // message via RF to BIT.
  //
  if(millis() - last > 1000) {
    last = millis();
    getTime();
    checkTemp();
    updateDisplay();
#ifdef HAS_RF
    vw_send((uint8_t *)&bit, sizeof(bit));
    vw_wait_tx();
#endif
  }

  //
  // Handle HTTP requests
  //
#ifdef HAS_ETH
  uint16_t dat_p;

  if (dat_p = ether.packetLoop(ether.packetReceive())) {
    dat_p = process_request((char *)&(Ethernet::buffer[dat_p]));
    if( dat_p ) ether.httpServerReply(dat_p);
  }
#endif
}
