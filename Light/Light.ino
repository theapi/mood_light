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

#define RX_ADDRESS "AAAAA"
//#define RX_ADDRESS "BBBBB"
//#define RX_ADDRESS "CCCCC"
//#define RX_ADDRESS "DDDDD"
//#define RX_ADDRESS "EEEEE"
#define BASE_ADDRESS "1BASE"

#include "RF24.h"
#include <Adafruit_NeoPixel.h>

#define PIN_CSN 4
#define PIN_CE  9 // Fake pin as CE is tied high to always be a primary receiver.
#define PIN_NEO 3

Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, PIN_NEO, NEO_GRB + NEO_KHZ800);
RF24 radio(PIN_CE, PIN_CSN);

byte address[6] = RX_ADDRESS;
byte address_base[6] = BASE_ADDRESS;
int ack = 0;

/**
 * Be carefull to ensure the struct size is the same as on the Pi.
 * Just having the same size variables is not enough.
 * @see http://www.delorie.com/djgpp/v2faq/faq22_11.html
 */
typedef struct{
  int32_t timestamp;
  uint16_t msg_id;
  uint16_t vcc;
  uint16_t a;
  uint16_t b;
  uint16_t c;
  uint16_t d;
  uint8_t type;
  uint8_t device_id;
  int8_t y;
  int8_t z;
}
payload_t;
payload_t payload;

byte wheel_pos; // the current colour wheel position 

byte current_cmd = 0;

void setup() 
{
  strip.begin();
  strip.setPixelColor(0, 0, 0, 0); // Off (only one NeoPixel)
  strip.show(); // Initialize all pixels to 'off'
  
  // Setup and configure rf radio
  radio.begin(); // Start up the radio
  radio.setPayloadSize(sizeof(payload_t));
  radio.setAutoAck(1); // Ensure autoACK is enabled
  radio.setRetries(0,15); // Max delay between retries & number of retries
  // Allow optional ack payloads
  radio.enableAckPayload();
  
  // Pipe for talking to the base
  radio.openWritingPipe(address_base);
  
  // Pipe for listening to the base
  radio.openReadingPipe(1, address);
  radio.startListening(); // Start listening
}

void loop(void)
{
  
  if (current_cmd > 0) {
    switch (current_cmd) {
      case 52: // 4
        breath(5000.0, 22, 255, 22);
        break;
      case 53: // 5
        breath(5000.0, 15, 15, 255);
        break;
      case 54: // 6
        breath(5000.0, 255, 255, 25);
        break;
    }
  }
  
  // Check for a message from the controller
  if (radio.available()) {
    // Get the payload
    // Create the ack payload for the NEXT message.
    radio.writeAckPayload(1, &ack, sizeof(ack));
    ack++; 

    radio.read( &payload, sizeof(payload));

    if (payload.a > 0) {
      // Do something...
      handleCommand(payload.a);
    }
  }
}

void handleCommand(uint16_t cmd)
{ 
  // NB one pixel for now
  uint16_t i = 0;
  current_cmd = 0;
  
  switch(cmd) {
    case 48: // 0
    case 99: // C- (c)
      strip.setPixelColor(i, 0, 0, 0);
      strip.show();
      break;
    case 67: // C+ (C)
      strip.setPixelColor(i, Wheel((wheel_pos) & 255));
      strip.show();
      break;
    case 49: // 1
      rainbow(5);
      break;
    case 52: // 4
    case 53: // 5
    case 54: // 6
      current_cmd = cmd;
      break;
    case 45: // -
      strip.setPixelColor(i, Wheel((wheel_pos--) & 255));
      strip.show();
      break;
    case 43: // +
      strip.setPixelColor(i, Wheel((wheel_pos++) & 255));
      strip.show();
      break;
    case 70: // FF (f)
      wheel_pos+=10;
      strip.setPixelColor(i, Wheel((wheel_pos) & 255));
      strip.show();
      break;
    case 82: // RW (R)
      wheel_pos-=10;
      strip.setPixelColor(i, Wheel((wheel_pos) & 255));
      strip.show();
      break;
    case 57: // 9
      rainbow(250);
      break;
      
    // @todo A bunch of predifined effects...
      
    default:
      // do nothing
      break;
  }
}


/**
 * Gently change the led
 */
void breath(float breath_speed, byte red, byte green, byte blue)
{
  // http://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  
  float val = (exp(sin(millis()/ breath_speed *PI)) - 0.36787944)*108.0;
  val = map(val, 0, 255, 50, 255);
  
  
  strip.setPixelColor(0, map(val, 0, 255, 0, red), map(val, 0, 255, 0, green), map(val, 0, 255, 0, blue));
  strip.show();
    
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
