BOARD_TAG    = uno
MONITOR_PORT = /dev/cu.usbmodem*
ARDUINO_LIBS = DS1307RTC Time Wire Bounce TimerOne LiquidCrystal

include ../arduino-mk/Arduino.mk
