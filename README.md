# An Arduino Thermostat

The FrankenStat conists of 2 arduino's. The first is the MCP, which has the
controls, display and the logic. The 2nd Arduino, the BC, controls the CV
burner. Both Arduino's communicate through a 433MHz link.

Parts:

- 2x Arduino Nano
- 2x MCP9701 Thermistor
- Hitachi HD44780 compatible LCD screen
- DS1307 RTC
- 2x Buttons
- LED
- Relay

This uses the [Arduino makefile](https://github.com/sudar/Arduino-Makefile.git).

# Schematic

<img src="FrankenStat.png">

# Sensor Accuracy

I noticed that the temerature would go up, when the LED was turned on by about 0.25 degrees. Measuring the voltage showed a small drop in voltage. This was solved by using the 3.3V power, and hooking the 3.3V power to the AREF pin. See the sensor guide on [Adafruit](http://learn.adafruit.com/tmp36-temperature-sensor/using-a-temp-sensor).

# API

To read the status and control the temperature the following URL's can be used:

## /

Returns the status of the thermostat.

    {
      "target": 21,
      "current": 23.15,
      "burner": false
    }

## /up

Increase the temporary target temperature. Returns the new target temperature.

    {
      "target": 21.5,
      "current": 23.15,
      "burner": false
    }

## /down

Decrease the temporary target temperature. Returns the new target temperature.

    {
      "target": 21.5,
      "current": 23.15,
      "burner": false
    }

## /set/16

Set a new temporary target temperature. Returns the new target temperature.

    {
      "target": 16.0,
      "current": 23.15,
      "burner": false
    }

# Useful URL\'s

[EtherCard Examples](https://github.com/thiseldo/EtherCardExamples)
