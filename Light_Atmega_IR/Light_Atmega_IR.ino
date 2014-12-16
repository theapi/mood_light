/**

 Arduino/ATmega328 listening for commands from the central controller, the Raspberry Pi.
 

 As the arduino ide can only have one serial monitor open, use minicom if needed:
  minicom --baudrate 57600 --device /dev/ttyUSB0
  
 Using the RF24 library from https://github.com/TMRh20/RF24
 
 */
 
#define NUM_PIXELS 16
 
#define DEVICE_ID 'B';
 
//#define RX_ADDRESS "AAAAA"
#define RX_ADDRESS "BBBBB"
//#define RX_ADDRESS "CCCCC"
//#define RX_ADDRESS "DDDDD"
//#define RX_ADDRESS "EEEEE"
// @todo: hardware defined address

#define BASE_ADDRESS "1BASE"

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
// https://github.com/shirriff/Arduino-IRremote
#include <IRremote.h>
#include <Adafruit_NeoPixel.h>


#define PIN_CE  7
#define PIN_CSN 8
#define PIN_NEO 6

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, PIN_NEO, NEO_GRB + NEO_KHZ800);
RF24 radio(PIN_CE, PIN_CSN);

// The address that this node listens on
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


uint16_t msg_id = 0;

// the pin used for the infrared receiver 
int RECV_PIN = 2;

// Create an instance of the IRrecv library
IRrecv irrecv(RECV_PIN);

// Structure containing received data
decode_results results;

// Used to store the last code received. Used when a repeat code is received
unsigned long LastCode;

byte wheel_pos; // the current colour wheel position 

void setup() 
{
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  
  Serial.begin(57600);
  printf_begin();
  printf("\n\r RF24_Receiver on address: %s \n\r", RX_ADDRESS);

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
  
  // Start listening
  radio.startListening(); 
  // Dump the configuration of the rf unit for debugging
  radio.printDetails();  
  
  /* Start receiving IR codes */
  irrecv.enableIRIn();
  
  Serial.print("Size of payload = ");
  Serial.println(sizeof(payload));
  
  
}

void loop(void)
{

  // Check for a message from the controller
  if (radio.available()) {
    // Get the payload
    // Create the ack payload for the NEXT message.
    radio.writeAckPayload(1, &ack, sizeof(ack));
    ack++; 

    radio.read( &payload, sizeof(payload));    
    // NB if the sent payload is too long, things go bad.
    // really need access to RF24::flush_rx(), but that is private.

    processPayload();

  }
  

  // Check for a new IR code
  if (irrecv.decode(&results)) {
    // Cet the button name for the received code
    uint8_t send_val = irGetButton(results.value);
    
    if (send_val > 0) {
      radio.stopListening();
      
      // Prepare the message.
      payload_t payload;
      payload.device_id = DEVICE_ID;
      payload.type = 'L'; // light command
      payload.timestamp = millis();
      payload.msg_id = msg_id;
      payload.vcc = 0; //@TODO vcc
      payload.a = send_val;
      payload.b = 0;
      payload.c = 0;
      payload.d = 0;
      payload.y = 0;
      payload.z = 0;
      
      printf("sending %d, %d \n", payload.msg_id, payload.a);
      if (!radio.write( &payload, sizeof(payload))) { 
        printf(" failed.\n\r"); 
      }
      
      msg_id++; // Let it overflow
      radio.startListening(); 
    }
    // Start receiving codes again
    irrecv.resume();
    
  }

  
}


void processPayload()
{
  printf ("Got: %c %c %ld %d %d %d %d %d %d %d %d \n",
    payload.device_id,
    payload.type,
    payload.timestamp,
    payload.msg_id,
    payload.vcc,
    payload.a,
    payload.b,
    payload.c,
    payload.d,
    payload.y,
    payload.z);
    
  if (payload.a > 0) {
    // Do something...
    handleCommand(payload.a);
  }
   
}

/**
 * The command for the button pressed
 */
uint8_t irGetButton(unsigned long code)
{
  
  uint8_t val = 0;
  
  /* Character array used to hold the received button name */
  char CodeName[3];
  
  /* Is the received code is a repeat code (NEC protocol) */
  if (code == 0xFFFFFFFF)
  {
    /* If so then we need to find the button name for the last button pressed */
    code = LastCode;
  }
  /* Save this code incase we get a repeat code next time */
  LastCode = code;
  //Serial.println(code, HEX);

  // ASCII
  
  /* Find the button name for the received code */
  switch (code) {
    /* Received code is for the POWER button */
  case 0xFFA25D:
    strcpy (CodeName, "C-");
    val = 99; // C
    break;
    /* Received code is for the MODE button */
  case 0xFF629D:
    strcpy (CodeName, "MO");
    val = 109; // m
    break;
    /* Received code is for the MUTE button */
  case 0xFFE21D:
    strcpy (CodeName, "C+");
    val = 67; // c
    break;
    /* Received code is for the PLAY/PAUSE button */
  case 0xFFC23D:
    strcpy (CodeName, "PL");
    val = 80; // P
    break;
    /* Received code is for the REWIND button */
  case 0xFF22DD:
    strcpy (CodeName, "RW");
    val = 82; // R
    break;
    /* Received code is for the FAST FORWARD button */
  case 0xFF02FD:
    strcpy (CodeName, "FF");
    val = 70; // F
    break;
    /* Received code is for the EQ button */
  case 0xFF906F:
    strcpy (CodeName, "EQ");
    val = 69; // E
    break;
    /* Received code is for the VOLUME - button */
  case 0xFFE01F:
    strcpy (CodeName, "-");
    val = 45;
    break;
    /* Received code is for the VOLUME + button */
  case 0xFFA857:
    strcpy (CodeName, "+");
    val = 43;
    break;
    /* Received code is for the number 0 button */
  case 0xFF6897:
    strcpy (CodeName, "0");
    val = 48;
    break;
    /* Received code is for the RANDOM button */
  case 0xFF9867:
    strcpy (CodeName, " <");
    val = 60; // <
    break;
    /* Received code is for the UD/SD button */
  case 0xFFB04F:
    strcpy (CodeName, " >");
    val = 62; // >
    break;
    /* Received code is for the number 1 button */
  case 0xFF30CF:
    strcpy (CodeName, "1");
    val = 49;
    break;
    /* Received code is for the number 2 button */
  case 0xFF18E7:
    strcpy (CodeName, "2");
    val = 50;
    break;
    /* Received code is for the number 3 button */
  case 0xFF7A85:
    strcpy (CodeName, "3");
    val = 51;
    break;
    /* Received code is for the number 4 button */
  case 0xFF10EF:
    strcpy (CodeName, "4");
    val = 52;
    break;
    /* Received code is for the number 5 button */
  case 0xFF38C7:
    strcpy (CodeName, "5");
    val = 53;
    break;
    /* Received code is for the number 6 button */
  case 0xFF5AA5:
    strcpy (CodeName, "6");
    val = 54;
    break;
    /* Received code is for the number 7 button */
  case 0xFF42BD:
    strcpy (CodeName, "7");
    val = 55;
    break;
    /* Received code is for the number 8 button */
  case 0xFF4AB5:
    strcpy (CodeName, "8");
    val = 56;
    break;
    /* Received code is for the number 9 button */
  case 0xFF52AD:
    strcpy (CodeName, "9");
    val = 57;
    break;
    /* Received code is an error or is unknown */
  default:
    strcpy (CodeName, "??");
    break;
  }
  
  //Serial.println(CodeName);
  return val;
}

void handleCommand(uint16_t cmd)
{ 

  switch(cmd) {
    case 48: // 0
    case 99: // C- (c)
      for (uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, 0, 0, 0);
        strip.show();
      }
      break;
    case 67: // C+ (C)
      for (uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel((wheel_pos) & 255));
        strip.show();
      }
      break;
    case 49: // 1
      //rainbow(5);
      break;
    case 50: // 2
      //rainbowCycle(5);
      break;
    case 45: // -
      for (uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel((wheel_pos--) & 255));
        strip.show();
      }
      break;
    case 43: // +
      for (uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel((wheel_pos++) & 255));
        strip.show();
      }
      break;
    case 70: // FF (f)
      wheel_pos+=10;
      for (uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel((wheel_pos) & 255));
        strip.show();
      }
      break;
    case 82: // RW (R)
      wheel_pos-=10;
      for (uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel((wheel_pos) & 255));
        strip.show();
      }
      break;
    case 57: // 9
      //rainbow(30);
      break;
    
      
    // @todo A bunch of predifined effects...
      
    default:
      // do nothing
      break;
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


// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
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
