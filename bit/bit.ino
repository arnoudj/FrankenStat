#include <VirtualWire.h>
#include "../include/FrankenStat.h"

unsigned long last;

void setup()
{
    pinMode(13, OUTPUT);
    Serial.begin(9600); // Debugging only
    Serial.println("setup");
    // Initialise the IO and ISR
    vw_set_rx_pin(2);
    vw_set_ptt_inverted(true); // Required for DR3100
    vw_setup(2000);      // Bits per sec
    vw_rx_start();       // Start the receiver PLL running

    last = millis();
}
void loop()
{
  bit_t *bit;

  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;
  if (vw_get_message(buf, &buflen)) // Non-blocking
  {
    if(buflen-1 == sizeof(bit)) {
      bit=(bit_t *)buf;
      Serial.println("Message received");
      digitalWrite(13, (*bit).burn);
      Serial.print("Serial: ");
      Serial.println((*bit).serial);
      Serial.print("Burner: ");
      Serial.println((*bit).burn);
      Serial.println();
    }
    else {
      Serial.print("Invalid message size ");
      Serial.print(buflen);
      Serial.print(". Expected ");
      Serial.println(sizeof(bit));
    }
  }

  if(millis() > last + 2000) {
    last = millis();
    Serial.print("Bad: ");
    Serial.println(vw_get_rx_bad());
    Serial.print("Good: ");
    Serial.println(vw_get_rx_good());
  }
}
