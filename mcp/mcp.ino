// Turn on or off certain functions.
#define DEBUG
#define HAS_LCD
#define HAS_RF
#undef HAS_ETH
#undef HAS_PROWL

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

// Settings
#define MIN_TEMP  10.0
#define MAX_TEMP  25.0

// I2C addresses
#define DS1307_I2C_ADDRESS 0x68
#define LCD_I2C_ADDRESS 0x38

// Pin assignment
#define PIN_THERM 0
#define PIN_MODE 5
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
tmElements_t tm;
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
    { 1, 8, 2, 12 },
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
Bounce but_up = Bounce(PIN_UP, 50);
Bounce but_down = Bounce(PIN_DOWN, 50);
Bounce but_mode = Bounce(PIN_MODE, 50);

// Initialize the bit struct. This is send via RF to the BIT. And contains a
// serial number and a boolean indicating if the CV should be turned on or
// not.
bit_t bit = { BITSERIAL, false };

// ------------------------------------------------------------------------

//
// Convert hours and minutes to minutes since 00:00.
//
int hm2min(int h, int m) {
  return h * 60 + m;
}

//
// Convert hours and quarters to minutes since 00:00.
//
int hq2min(int h, int q) {
  return h * 60 + q * 15;
}

//
// Get the temperature from the last active time interval of a give day.
//
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
  // We should never reach this point. All Days should have at least one
  // active time mark.
  return 0;
}

//
// Set the target temperature using the schedule or the temperatures set
// for temporary or holiday mode.
//
void scheduledTemp() {
  float target = -1.0;

  // Look up current schedule
  for(int i = 3; i >= 0; i--) {
    if(schedule[tm.Wday-1][i].active == 0) {
      continue;
    }
    // Current time is measured in hours and minutes, and the times in the
    // schedule are stored as hours and quarters. Both need to be converted
    // to minutes since 00:00 so we can compare them.
    if(hq2min(schedule[tm.Wday-1][i].hour, schedule[tm.Wday-1][i].quarters) <
       hm2min(tm.Hour,tm.Minute)) {
      sp_now.day = tm.Wday;
      sp_now.line = i;
      target = (float)schedule[tm.Wday-1][i].temp / 2 + 10;
      break;
    }
  }
  // When no time interval is found for this day, we use the last active
  // temperature setting from yesterday.
  if(target == -1.0) {
    target = (float)lastTemp(tm.Wday) / 2 + 10;
  }

  // If mode is other than MODE_AUTO, we use those temperatures.
  switch(mode) {
    case MODE_TEMP:
      if(sp_now.day == sp_tmp.day && sp_now.line == sp_tmp.line) {
        // Still on the same setpoint on which we set the temporary
        // temperature.
        tgt = tmptgt;
      }
      else {
        // We've reached a new time interval, set mode back to MODE_AUTO.
        mode = MODE_AUTO;
        break;
      }
    case MODE_HOLIDAY:
      tgt = tmptgt;
      return;
  }

  tgt = target;
}

//
// Read the time from the DS1307 RTC.
//
void getTime() {
  RTC.read(tm);
}

//
// Read the temperature from the sensor and convert it to degrees celcius.
//
float getTemp() {
  // We're comparing against 3.3V (which is actually 3.4V), in stead of the
  // expected 5V. So we will multiply the result by 0.68.
  float ThermValue = analogRead(PIN_THERM) * 0.68;
  float mVout=(float) ThermValue*5000.0/1023.0; //3.0V = 3000mV
  float TempC=(mVout-450.0)/19.5; //Ta = (Vout-400mV)/19.5mV //Modified

  return TempC;
}

//
// The temperature is measured each second. The temperatures from the last 15
// seconds are stored in an array and the average of the is used as the
// current temperature.
//
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

//
// Update the LCD display and write some data to serial if debugging is
// enabled.
//
void updateDisplay() {
  // 16x2 display:
  // [T:21.1 Tu 20:00 ]
  // [C:16.4 AUTO     ]

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
  if(tm.Hour < 10) lcd.print('0');
  lcd.print(tm.Hour);
  if(tm.Second % 2 == 0) {
    lcd.print(":");
  }
  else {
    lcd.print(" ");
  }
  if(tm.Minute < 10) lcd.print('0');
  lcd.print(tm.Minute);

  lcd.setCursor(8,1);
  switch (mode) {
    case 0:
      lcd.print(F("AUTO   "));
      break;
    case 1:
      lcd.print(F("TEMP   "));
      break;
    case 2:
      lcd.print(F("HOLIDAY"));
      break;
  }
#endif

#ifdef DEBUG
  Serial.print(days[tm.Wday]);
  Serial.print(" ");
  if(tm.Hour < 10) Serial.print('0');
  Serial.print(tm.Hour);
  Serial.print(":");
  if(tm.Minute < 10) Serial.print('0');
  Serial.println(tm.Minute);
  Serial.print(F("tgt: "));
  Serial.println(tgt);
  Serial.print(F("cur: "));
  Serial.println(cur);
  Serial.print(F("mod: "));
  Serial.println((int)mode);
  Serial.print(F("day: "));
  Serial.println((int)sp_now.day);
  Serial.print(F("line: "));
  Serial.println((int)sp_now.line);
  Serial.println();
#endif
}

#ifdef HAS_PROWL
//
// When the CV is turned on or off, send a Prowl notification.
//
void sendProwl() {
  if(ether.dnsLookup(prowl_domain)) {
    byte sd = stash.create();
    stash.print("apikey=");
    stash.print(PROWL_APIKEY);
    stash.print("&application=FrankenTherm&event=");
    /*
    if(bit.burn) {
      stash.print("Idle");
    }
    else {
      stash.print("Burning");
    }
    */
    stash.print("&description=");
    if(bit.burn) {
      stash.print("CV on");
    }
    else {
      stash.print("CV off");
    }
    stash.save();

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
  }
}
#endif

//
// Compare the target and current temperatures and turn the CV on or
// off when needed.
void checkTemp() {
  scheduledTemp();
  getAvgTemp();
  updateDisplay();

  switch (bit.burn) {
    case BURNING:
      // If the current temperature is above the target + 0.5 degrees, then
      // the CV should be switched off.
      if(cur > tgt + 0.5) {
        if(bit.burn != IDLE) {
#ifdef DEBUG
          Serial.println(F("Off"));
#endif
#ifdef HAS_PROWL
          sendProwl();
#endif
          bit.burn = IDLE;
        }
      }
      break;
    default:
      // If the current temperature is below the target - 0.5 degrees, then
      // the CV should be switched on.
      if(cur < tgt - 0.5) {
        if(bit.burn != BURNING) {
#ifdef DEBUG
          Serial.println(F("On"));
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
#endif

//
// Temporarily set a new temperature until the next setpoint.
//
void setModeTemp() {
  if(mode == MODE_AUTO) {
    mode = MODE_TEMP;
    tmptgt = tgt;
    sp_tmp.day = sp_now.day;
    sp_tmp.line = sp_now.line;
  }
}

#ifdef HAS_ETH
//
// Read the HTTP request and returns the corresponding reply.
//
int16_t process_request(char *str)
{
  if (strncmp("GET /up ", str, 8)==0){
    if(tmptgt < MAX_TEMP) {
      setModeTemp();
      tmptgt += 0.5;
      scheduledTemp();
    }
  }
  if (strncmp("GET /down ", str, 10)==0){
    if(tmptgt > MIN_TEMP) {
      setModeTemp();
      tmptgt -= 0.5;
      scheduledTemp();
    }
  }
  if (strncmp("GET /set/", str, 9)==0){
  }

  return JSON_status();
}
#endif

// ------------------------------------------------------------------------

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

// ------------------------------------------------------------------------

void loop() {
  //
  // Check if buttons are pressed
  //
  but_mode.update();
  if(but_mode.fallingEdge()) {
    switch(mode) {
      case MODE_AUTO:
        tmptgt = tgt;
        break;
      case MODE_TEMP:
        mode = MODE_HOLIDAY;
        tmptgt = 16;
        break;
      case MODE_HOLIDAY:
        mode = MODE_AUTO;
        break;
      default:
        mode = MODE_AUTO;
    }
    scheduledTemp();
    updateDisplay();
  }
  but_up.update();
  if(but_up.fallingEdge()) {
#ifdef DEBUG
    Serial.println(F("UP"));
#endif
    setModeTemp();
    tmptgt += 0.5;
    scheduledTemp();
    updateDisplay();
  }
  
  but_down.update();
  if(but_down.fallingEdge()) {
#ifdef DEBUG
    Serial.println(F("DOWN"));
#endif
    setModeTemp();
    tmptgt -= 0.5;
    scheduledTemp();
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
