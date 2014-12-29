Mood Light 
==========

Remote controlled light, using NeoPixel &amp; Nrf24L01-2.4GHz

Hardware
--------
Attiny85 8mhz 3.3v

Nrf24L01-2.4GHz wireless radio transceiver http://arduino-info.wikispaces.com/Nrf24L01-2.4GHz-HowTo

NeoPixel https://learn.adafruit.com/adafruit-neopixel-uberguide/overview

Raspberry Pi also with a Nrf24L01-2.4GHz radio module, see https://github.com/theapi/nrf24


Software
--------
RF24 library https://github.com/TMRh20/RF24

Nrf24Payload library https://github.com/theapi/nrf24

NeoPixel library https://github.com/adafruit/Adafruit_NeoPixel

Setup
-----

    ATtiny25/45/85 Pin map
                                 +-\/-+
                   NC      PB5  1|o   |8  Vcc --- nRF24L01  VCC, pin2
             NEO_PIXEL --- PB3  2|    |7  PB2 --- nRF24L01  SCK, pin5
    nRF24L01 CSN, pin4 --- PB4  3|    |6  PB1 --- nRF24L01 MOSI, pin6
    nRF24L01 GND, pin1 --- GND  4|    |5  PB0 --- nRF24L01 MISO, pin7
                                 +----+
    nRF24L01 CE, pin3 -- VCC
