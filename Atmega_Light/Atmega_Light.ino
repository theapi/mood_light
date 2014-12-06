
/**

 Arduino/ATmega328 listening for commands from the central controlle, the Raspberry Pi.
 

 As the arduino ide can only have one serial monitor open, use minicom if needed:
  minicom --baudrate 57600 --device /dev/ttyUSB0
  
 Using the RF24 library from https://github.com/TMRh20/RF24
 
 */

//#define RX_ADDRESS "AAAAA"
#define RX_ADDRESS "BBBBB"
//#define RX_ADDRESS "CCCCC"
//#define RX_ADDRESS "DDDDD"
//#define RX_ADDRESS "EEEEE"
// @todo: hardware defined address

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#define PIN_CE  7
#define PIN_CSN 8
 
// The address that this node listens on
byte address[6] = RX_ADDRESS;

RF24 radio(PIN_CE, PIN_CSN);

void setup() 
{
  Serial.begin(57600);
  printf_begin();
  printf("\n\r RF24_Receiver on address: %s \n\r", RX_ADDRESS);

  // Setup and configure rf radio
  radio.begin(); // Start up the radio
  radio.setPayloadSize(2);                // Only two byte payload gets sent (int on arduino) (short on 32bit rpi)
  radio.setAutoAck(1); // Ensure autoACK is enabled
  radio.setRetries(0,15); // Max delay between retries & number of retries

  radio.openReadingPipe(1, address);
  radio.startListening(); // Start listening
  
  radio.printDetails();                   // Dump the configuration of the rf unit for debugging
}

void loop(void)
{
  
  // Check for a message from the controller
  if (radio.available()) {
    // Get the payload

    int got_val; 
    radio.read( &got_val, 2);    
      
    if (got_val > 0) {
      printf("Got: %d \n\r", got_val);
      //@todo: process commands...
    }
  }
}

