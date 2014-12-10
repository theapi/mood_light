/**

 Arduino/ATmega328 listening for commands from the central controller, the Raspberry Pi.
 

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

#define BASE_ADDRESS "1BASE"

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
// https://github.com/shirriff/Arduino-IRremote
#include <IRremote.h>


#define PIN_CE  7
#define PIN_CSN 8

// Fixed size payload
#define PAYLOAD_SIZE 18

RF24 radio(PIN_CE, PIN_CSN);

// The address that this node listens on
byte address[6] = RX_ADDRESS;
byte address_base[6] = BASE_ADDRESS;

int ack = 0;

uint8_t msg_type = 'I';
uint8_t msg_id = 0;

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

    while (radio.available()) {   
      // Create the ack payload for the NEXT message.
      radio.writeAckPayload(1, &ack, sizeof(ack));
      ack++;
      
      uint8_t msg[PAYLOAD_SIZE];
      radio.read( &msg, PAYLOAD_SIZE);    
      
   
      printf("Got: %s \n\r", msg);
      processMessage(msg);
      
    }
  }
  
  
  // Check for a new IR code
  if (irrecv.decode(&results)) {
    // Cet the button name for the received code
    uint8_t send_val = irGetButton(results.value);
    
    if (send_val > 0) {
      radio.stopListening();
      
      // Prepare the message.
      uint8_t msg[PAYLOAD_SIZE];
      // byte 0 = Message type
      msg[0] = msg_type;
      // byte 1 = Message id
      msg[1] = msg_id;
      // byte 2 = high byte of the int
      msg[2] = 0;
      // byte 3 = low byte of the int
      msg[3] = send_val;
      // Rest of the bytes are junk

      printf("sending %s\n\r", msg);    
      if (!radio.write( &msg, PAYLOAD_SIZE)) { 
        printf(" failed.\n\r"); 
      }
      
      msg_id++; // Let it overflow
      radio.startListening(); 
    }
    // Start receiving codes again
    irrecv.resume();
    
  }
  
}

void processMessage(uint8_t msg[PAYLOAD_SIZE])
{
  // byte 0 = Message type
  // byte 1 = Message id (not unique as it is 0 to 254)
  // byte 2 & 3 = uint16_t (int)
  // rest ignored
  int cmd = (msg[2] << 8) | msg[3];
  printf("Got: %hd, %hd, %d \n", msg[0], msg[1], cmd);    
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
  
  //Serial.println(CodeName);
  return val;
}

