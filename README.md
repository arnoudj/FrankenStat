# An Arduino Thermostat

Parts:

- MCP9701 Thermistor
- Hitachi HD44780 compatible LCD screen
- DS1307 RTC
- Buttons
- LED
- Relay

This uses the [Arduino makefile](https://github.com/sudar/Arduino-Makefile.git).

# Schematic

<img src="FrankenStat.png">

# Sensor Accuracy

I noticed that the temerature would go up, when the LED was turned on by about 0.25 degrees. Measuring the voltage showed a small drop in voltage. This was solved by using the 3.3V power, and hooking the 3.3V power to the AREF pin. See the sensor guide on [Adafruit](http://learn.adafruit.com/tmp36-temperature-sensor/using-a-temp-sensor).
