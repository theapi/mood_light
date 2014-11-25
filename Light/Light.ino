/*
    ** Hardware configuration **
    ATtiny25/45/85 Pin map with CE_PIN 3 and CSN_PIN 4
                                 +-\/-+
                   NC      PB5  1|o   |8  Vcc --- nRF24L01  VCC, pin2 --- LED --- 5V
    nRF24L01  CE, pin3 --- PB3  2|    |7  PB2 --- nRF24L01  SCK, pin5
    nRF24L01 CSN, pin4 --- PB4  3|    |6  PB1 --- nRF24L01 MOSI, pin7
    nRF24L01 GND, pin1 --- GND  4|    |5  PB0 --- nRF24L01 MISO, pin6
                                 +----+

*/

// CE and CSN are configurable, specified values for ATtiny85 as connected above
#define CE_PIN 7 // Fake pin as CE is tied high to always be a primary receiver.
#define CSN_PIN 4

#include <Adafruit_NeoPixel.h>

#define PIN 0
Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);


#include "RF24.h"

RF24 radio(CE_PIN, CSN_PIN);

byte addresses[][6] = {"1Node","2Node"};

void setup() 
{
  strip.begin();
  strip.setPixelColor(0, 0, 0, 0); // Off (only one NeoPixel)
  strip.show(); // Initialize all pixels to 'off'
  
  // Setup and configure rf radio
  radio.begin(); // Start up the radio
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
    radio.stopListening();
    int got_val;       // Variable for the received timestamp                   
    radio.read( &got_val, sizeof(int) );    // Get the payload
      
    if (got_val > 0) {
      
      radio.powerDown();
      
      pinMode(PIN, OUTPUT);
      rainbow(100);
      strip.setPixelColor(0, 0, 0, 0); // Off (only one NeoPixel)
      strip.show();
      pinMode(PIN, INPUT);
      
      radio.powerUp();
    }
    radio.startListening();
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
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
