/**
 * Arduino controller for the mood lights.
 */

/**
 * Example for Getting Started with nRF24L01+ radios. 
 *
 * This is an example of how to use the RF24 class to communicate on a basic level.  Write this sketch to two 
 * different nodes.  Put one of the nodes into 'transmit' mode by connecting with the serial monitor and
 * sending a 'T'.  The ping node sends the current time to the pong node, which responds by sending the value
 * back.  The ping node can then see how long the whole cycle took. 
 * Note: For a more efficient call-response scenario see the GettingStarted_CallResponse.ino example.
 * Note: When switching between sketches, the radio may need to be powered down to clear settings that are not "un-set" otherwise
 */


#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

// Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 7 & 8 
RF24 radio(7,8);

byte addresses[][6] = {"1Node","2Node"};


void setup() 
{
  Serial.begin(57600);
  printf_begin();
  printf("\n\rRF24/examples/GettingStarted/\n\r");
  printf("*** Enter an INT then hit return to transmit it to the other node\n\r");

  // Setup and configure rf radio
  radio.begin();                          // Start up the radio
  radio.setPayloadSize(2);                // Only two byte payload gets sent (int)
  radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.setRetries(15,15);                // Max delay between retries & number of retries
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1,addresses[1]);
  
  //radio.startListening();                 // Start listening
  radio.printDetails();                   // Dump the configuration of the rf unit for debugging
}

void loop(void)
{

  // if there's any serial available, read it:
  while (Serial.available() > 0) {

    //radio.stopListening(); // First, stop listening so we can talk.
    
    // look for the next valid integer in the incoming serial stream:
    int send_val = Serial.parseInt(); 

    // look for the newline.
    if (Serial.read() == '\n') {
      radio.write( &send_val, sizeof(int) );
      printf(" sent %d\n\r", send_val);     
    }
  }
  

}

