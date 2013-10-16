# An Arduino Thermostat

The FrankenStat conists of 2 arduino's. The first is the MCP, which has the
controls, display and the logic. The 2nd Arduino, the Bit, controls the CV
burner. Both Arduino's communicate through a 433MHz link.

Parts:

- 2x Arduino Nano
- 2x MCP9701 Thermistor
- Hitachi HD44780 compatible LCD screen with I2C interface adapter
- DS1307 RTC
- 2x Buttons
- LED
- Relay

This uses the [Arduino makefile](https://github.com/sudar/Arduino-Makefile.git).

Before compiling copy include/prowl.h-example to include/prowl.h and insert your Prowl API key (make one on the Prowl site, if you don't already have one).

# Schematic

<img src="FrankenStat.png">

TODO: Schematic is outdated, needs an update.

# Sensor Accuracy

I noticed that the temerature would go up, when the LED was turned on by about 0.25 degrees. Measuring the voltage showed a small drop in voltage. This was solved by using the 3.3V power, and hooking the 3.3V power to the AREF pin. See the sensor guide on [Adafruit](http://learn.adafruit.com/tmp36-temperature-sensor/using-a-temp-sensor).

# API

To read the status and control the temperature the following URL's can be used:

## /

Returns the status of the thermostat.

    {
      "target": 21,
      "current": 23.15,
      "burner": false,
      "mode": 0
    }

## /up

Increase the temporary target temperature. Returns the new target temperature.

    {
      "target": 21.5,
      "current": 23.15,
      "burner": false,
      "mode": 0
    }

## /down

Decrease the temporary target temperature. Returns the new target temperature.

    {
      "target": 21.5,
      "current": 23.15,
      "burner": false,
      "mode": 0
    }

## /set/16

Set a new temporary target temperature. Returns the new target temperature.

    {
      "target": 16.0,
      "current": 23.15,
      "burner": false,
      "mode": 0
    }

# Useful URL\'s

[EtherCard Examples](https://github.com/thiseldo/EtherCardExamples)
[Open Energy Monitoring](https://github.com/helxsz/Webinos---Open-energy-monitoring/blob/master/server2_4.pde): Nice example on how to use the EtherCard library.
