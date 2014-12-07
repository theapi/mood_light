/**
 * Mood light with IR remote control.
 */

//#define RX_ADDRESS "AAAAA"
//#define RX_ADDRESS "BBBBB"
#define RX_ADDRESS "CCCCC"
//#define RX_ADDRESS "DDDDD"
//#define RX_ADDRESS "EEEEE"
// @todo: hardware defined address

#define BASE_ADDRESS "3BASE"

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
// https://github.com/shirriff/Arduino-IRremote
#include <IRremote.h>


#define PIN_CE  7
#define PIN_CSN 8

RF24 radio(PIN_CE, PIN_CSN);

// The address that this node listens on
byte address[6] = RX_ADDRESS;
byte address_base[6] = BASE_ADDRESS;


// the pin used for the infrared receiver 
int RECV_PIN = 2;

// Create an instance of the IRrecv library
IRrecv irrecv(RECV_PIN);

// Structure containing received data
decode_results results;

// Used to store the last code received. Used when a repeat code is received
unsigned long LastCode;

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
}

void loop(void)
{

  // Check for a message from the controller
  if (radio.available()) {
    // Get the payload

    int got_val; 
    radio.read( &got_val, sizeof(got_val));     
    if (got_val > 0) {
      // Create the ack payload.
      radio.writeAckPayload(1, &got_val, sizeof(got_val));
      
      printf("Got: %d \n\r", got_val);
      //@todo: process commands...
    }
  }
  
  // Check for a new IR code
  if (irrecv.decode(&results)) {
    // Cet the button name for the received code
    int send_val = irGetButton(results.value);
    
    if (send_val > 0) {
      radio.stopListening();

      printf("sending %d\n\r", send_val);    
      if (!radio.write( &send_val, sizeof(int))) {  
        // Mmm always says fail but it's not actually failing.
        // Something to do with ACK maybe?
        printf(" failed.\n\r"); 
      }
      
      radio.startListening(); 
    }
    // Start receiving codes again
    irrecv.resume();
  }

}

/**
 * The button (int code)
 */
int irGetButton(unsigned long code)
{
  
  int val = 0;
  
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
  Serial.println(code, HEX);

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
  
  Serial.println(CodeName);
  return val;
}

