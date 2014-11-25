/*
    ** Hardware configuration **
    ATtiny25/45/85 Pin map
                                 +-\/-+
                   NC      PB5  1|o   |8  Vcc --- nRF24L01  VCC, pin2
             NEO_PIXEL --- PB3  2|    |7  PB2 --- nRF24L01  SCK, pin5
    nRF24L01 CSN, pin4 --- PB4  3|    |6  PB1 --- nRF24L01 MOSI, pin7
    nRF24L01 GND, pin1 --- GND  4|    |5  PB0 --- nRF24L01 MISO, pin6
                                 +----+
    nRF24L01 CE, pin3 -- VCC

*/

#include "RF24.h"
#include <Adafruit_NeoPixel.h>

#define PIN_CSN 4
#define PIN_CE  9 // Fake pin as CE is tied high to always be a primary receiver.
#define PIN_NEO 3

Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, PIN_NEO, NEO_GRB + NEO_KHZ800);
RF24 radio(PIN_CE, PIN_CSN);

byte addresses[][6] = {"1Node","2Node"};

void setup() 
{
  strip.begin();
  strip.setPixelColor(0, 0, 0, 0); // Off (only one NeoPixel)
  strip.show(); // Initialize all pixels to 'off'
  
  // Setup and configure rf radio
  radio.begin(); // Start up the radio
  radio.setPayloadSize(2);                // Only two byte payload gets sent (int)
  radio.setAutoAck(1); // Ensure autoACK is enabled
  radio.setRetries(15,15); // Max delay between retries & number of retries
  radio.openWritingPipe(addresses[1]); // "ping back"
  radio.openReadingPipe(1,addresses[0]);
  radio.startListening(); // Start listening
}

void loop(void)
{
  
  // Check for a message from the controller
  if (radio.available()) {
    // Get the payload
    int got_val;                   
    radio.read( &got_val, sizeof(int) );    
      
    if (got_val > 0) {
      // Do something...
      rainbow(10);
    }
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait); // @todo remove the delay.
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}
