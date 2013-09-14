#include <VirtualWire.h>
#include "../include/FrankenStat.h"

void setup()
{
    pinMode(13, OUTPUT);
    pinMode(2, OUTPUT);
    Serial.begin(9600); // Debugging only
    Serial.println("setup");
    // Initialise the IO and ISR
    vw_set_ptt_inverted(true); // Required for DR3100
    vw_setup(2000);      // Bits per sec
    vw_rx_start();       // Start the receiver PLL running

}
void loop()
{
  bit_t bit;

  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;
  if (vw_get_message((uint8_t *)&bit, &buflen)) // Non-blocking
  {
    //if(buflen == sizeof(bit)) {
      digitalWrite(13, bit.burn);
      Serial.print("Serial: ");
      Serial.println(bit.serial);
      Serial.print("Burner: ");
      Serial.println(bit.burn);
      Serial.println();
    //}
  }
}
