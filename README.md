Mood Light 
==========

Wireless remote controlled light, using NeoPixel &amp; Nrf24L01-2.4GHz

Hardware
--------
Attiny85
Nrf24L01-2.4GHz wireless radio transceiver http://arduino-info.wikispaces.com/Nrf24L01-2.4GHz-HowTo
NeoPixel https://learn.adafruit.com/adafruit-neopixel-uberguide/overview

Software
--------
RF24 library https://github.com/TMRh20/RF24
NeoPixel library https://github.com/adafruit/Adafruit_NeoPixel

Setup
-----
All pins are used as described in the RF24 Attiny example rf24ping85.
The MOSI pin (digital 0, PB0) is shared with the NeoPixel data line. 
