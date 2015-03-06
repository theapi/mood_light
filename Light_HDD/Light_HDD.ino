
/**

 Use hard drive rotary encoders to send RGB values to the NRF24 network.
 

 As the arduino ide can only have one serial monitor open, use minicom if needed:
  minicom --baudrate 57600 --device /dev/ttyUSB0
  
 Using the RF24 library from https://github.com/TMRh20/RF24
 
 */
 
#define ENC_A 14
#define ENC_B 15
#define ENC_PORT PINC
#define PROCESSING 1
 
#define DEVICE_ID 'B'
 
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

#include <Nrf24Payload.h>

#define PIN_CE  7
#define PIN_CSN 8
#define PIN_LED_RED 3
#define PIN_LED_GREEN 5
#define PIN_LED_BLUE 6


RF24 radio(PIN_CE, PIN_CSN);

Nrf24Payload rx_payload = Nrf24Payload();
uint8_t rx[Nrf24Payload_SIZE];


// The address that this node listens on
byte address[6] = RX_ADDRESS;
byte address_base[6] = BASE_ADDRESS;

uint16_t msg_id = 0;

volatile int enc_counter = 0; // changed by encoder input
volatile byte enc_ab = 0; // The previous & current reading

void setup() 
{
  
  // Setup encoder pins as inputs
  pinMode(ENC_A, INPUT);
  digitalWrite(ENC_A, HIGH);
  pinMode(ENC_B, INPUT);
  digitalWrite(ENC_B, HIGH);
  
  
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  
  // common anode
  digitalWrite(PIN_LED_RED, HIGH);
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_BLUE, HIGH);

  
  setupPinInterrupt();
  
  Serial.begin(57600);
  printf_begin();
  printf("\n\r Light_HDD on address: %s \n\r", RX_ADDRESS);


  // Setup and configure rf radio
  radio.begin(); // Start up the radio
  radio.setPayloadSize(Nrf24Payload_SIZE);               
  radio.setAutoAck(1); // Ensure autoACK is enabled
  radio.setRetries(0,15); // Max delay between retries & number of retries

  
  // Pipe for talking to the base
  radio.openWritingPipe(address_base);
  
  // Pipe for listening to the base
  //radio.openReadingPipe(1, address);
  
  // Start listening
  //radio.startListening(); 
  // Dump the configuration of the rf unit for debugging
  radio.printDetails();  
  
  Serial.print("Size of payload = ");
  Serial.println(radio.getPayloadSize());

  
}

void loop(void)
{

  static int last_enc_count = 0;
  static byte last_enc_ab = 0;
  
  if (last_enc_ab != enc_ab) {
    last_enc_ab = enc_ab;
    if (!PROCESSING) {
      Serial.print("Red: ");
      Serial.print(( enc_ab & 0x0f ), DEC);
      Serial.print(" : ");
      Serial.print(( enc_ab & 0x0f ), BIN);
      Serial.print(" : ");
      Serial.println(enc_counter, DEC);
    }
  }
  
  if (last_enc_count != enc_counter) {
    last_enc_count = enc_counter;
    
    // Set the local light to the new colour
    // let it overflow for now
    byte r = enc_counter;
    byte g = 0;
    byte b = 0;
    setLedColour(r, g, b);
    
    if (PROCESSING) {
      Serial.println(enc_counter, DEC);
    }
    
    
    // Prepare the message.
    Nrf24Payload tx_payload = Nrf24Payload();      
    tx_payload.setDeviceId(DEVICE_ID);
    tx_payload.setType('l'); // light command
    tx_payload.setId(msg_id++);
    tx_payload.setA(r);
    uint8_t tx_buffer[Nrf24Payload_SIZE];
    tx_payload.serialize(tx_buffer);
    if (!radio.write( &tx_buffer, Nrf24Payload_SIZE)) { 
      //printf(" no ack.\n\r"); 
    }
    
  }
  
}

void setLedColour(byte r, byte g, byte b)
{
  r = 255 - abs(r);
  analogWrite(PIN_LED_RED, r);
}

// Pin interrupt on port C == A0 -> A5 
// Any change on any enabled PCINT[14:8] pin will cause an interrupt.
ISR(PCINT1_vect)
{
  char tmpdata;
  tmpdata = read_encoder();
  if (tmpdata) {
    enc_counter += tmpdata;
  }
}

void setupPinInterrupt()
{
  // 13.2.4 PCICR – Pin Change Interrupt Control Register
  // Bit 1 – PCIE1: Pin Change Interrupt Enable 1
  PCICR = (1 << PCIE1); // 0x02  00000010
  
  // 13.2.7 PCMSK1 – Pin Change Mask Register 1
  PCMSK1 =  ((1 << PCINT9) | (1 << PCINT8)); // listen for interrupts on A1 and A0 
}

/**
 * returns change in encoder state (-1,0,1) 
 */
int8_t read_encoder()
{
  // enc_states[] array is a look-up table; 
  // it is pre-filled with encoder states, 
  // with “-1″ or “1″ being valid states and “0″ being invalid. 
  // We know that there can be only two valid combination of previous and current readings of the encoder 
  // – one for the step in a clockwise direction, 
  // another one for counterclockwise. 
  // Anything else, whether it's encoder that didn't move between reads 
  // or an incorrect combination due to bouncing, is reported as zero.
  static int8_t enc_states[] = {
    0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0
  };
  
  /*
   The lookup table of the binary values represented by enc_states 
     ___     ___     __
   A    |   |   |   |
        |___|   |___|
      1 0 0 1 1 0 0 1 1
      1 1 0 0 1 1 0 0 1
     _____     ___     __
   B      |   |   |   |
          |___|   |___|
   
   A is represented by bit 0 and bit 2
   B is represented by bit 1 and bit 3
   With previous and current values stored in 4 bit data there can be
   16 possible combinations.
   The enc_states lookup table represents each one and what it means:
   
   [0] = 0000; A & B both low as before: no change : 0
   [1] = 0001; A just became high while B is low: reverse : -1
   [2] = 0010; B just became high while A is low: forward : +1
   [3] = 0011; B & A are both high after both low: invalid : 0
   [4] = 0100; A just became low while B is low: forward : +1
   [5] = 0101; A just became high after already being high: invalid : 0
   [6] = 0110; B just became high while A became low: invalid : 0
   [7] = 0111; A just became high while B was already high: reverse : -1
   [8] = 1000; B just became low while A was already low: reverse : -1
   etc...
   
   Forward: 1101 (13) - 0100 (4) - 0010 (2) - 1011 (11)
   Reverse: 1110 (14) - 1000 (8) - 0001 (1) - 0111 (7)
   
  */

  // ab gets shifted left two times 
  // saving previous reading and setting two lower bits to “0″ 
  // so the current reading can be correctly ORed.
  enc_ab <<= 2;
  
  // ENC_PORT & 0×03 reads the port to which encoder is connected 
  // and sets all but two lower bits to zero 
  // so when you OR it with ab bits 2-7 would stay intact. 
  // Then it gets ORed with ab. 
  enc_ab |= ( ENC_PORT & 0x03 );  //add current state
  // At this point, we have previous reading of encoder pins in bits 2,3 of ab, 
  // current readings in bits 0,1, and together they form index of (AKA pointer to) enc_states[]  
  // array element containing current state.
  // The index being the the lowest nibble of ab (ab & 0x0f)
  return ( enc_states[( enc_ab & 0x0f )]);
}

 
