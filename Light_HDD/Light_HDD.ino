/**

 Use hard drive rotary encoders to send RGB values to the NRF24 network.


 As the arduino ide can only have one serial monitor open, use minicom if needed:
  minicom --baudrate 57600 --device /dev/ttyUSB0

 Using the RF24 library from https://github.com/TMRh20/RF24

*/

#include <avr/sleep.h>    // Sleep Modes
#include <avr/power.h>    // Power management

// Whether serial out put should be created & sent to Processing
#define PROCESSING 1

// Whether tthe NRF24 radio is attached
#define RADIO 0

#define ENC_A 14 // A0 (PC0 - PCINT8)
#define ENC_B 15 // A1 (PC1 - PCINT9)
#define ENC_PORT PINC


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

#include <Nrf24Payload.h>

#define PIN_CE  9
#define PIN_CSN 10
#define PIN_LED_RED 3
#define PIN_LED_GREEN 5
#define PIN_LED_BLUE 6
#define PIN_SWITCH_A 16 // A2 (PC2 - PCINT10)
#define PIN_SWITCH_B 17 // A3 (PC3 - PCINT11)
#define PIN_SWITCH_C 18 // A4 (PC4 - PCINT12)

#define BATTERY_CHECK_TIME 30000 // How often to check the battery level

#define MODE_HUE 0
#define MODE_SATURATION 1
#define MODE_INTENSITY 2
#define MODE_LOW_BATTERY 3

RF24 radio(PIN_CE, PIN_CSN);

Nrf24Payload rx_payload = Nrf24Payload();
uint8_t rx[Nrf24Payload_SIZE];
// The address that this node listens on
byte address[6] = RX_ADDRESS;
byte address_base[6] = BASE_ADDRESS;
uint16_t msg_id = 0;

// HSI colour space
// @see http://blog.saikoled.com/post/43693602826/why-every-led-light-should-be-using-hsi
int8_t mode = MODE_HUE;

long vcc = 0;
float hue = 0.0;
float saturation = 1.0;
float intensity = 1.0;

volatile int enc_counter = 0; // changed by encoder input
volatile byte enc_ab = 0; // The previous & current reading

void setup()
{
  // Setup encoder pins as inputs with pull up resistor
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  // RGB indicator led pins as output
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);

  // common anode, ensure leds are off
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_BLUE, LOW);

  // Switches to change modes as input
  pinMode(PIN_SWITCH_A, INPUT_PULLUP);
  pinMode(PIN_SWITCH_B, INPUT_PULLUP);
  pinMode(PIN_SWITCH_C, INPUT_PULLUP);

  // Listen to the encoder with interrupts
  setupPinInterrupt();

  // Ensure twi is not wasting power
  power_twi_disable();

  // Turn on ADC to monitor the battery level
  batteryAdcOn();

  if (PROCESSING) {
    Serial.begin(57600);
    Serial.println("Setup");
  }

  if (RADIO) {
    // Setup and configure rf radio
    radio.begin(); // Start up the radio
    radio.setPayloadSize(Nrf24Payload_SIZE);
    radio.setAutoAck(1); // Ensure autoACK is enabled
    radio.setRetries(0,15); // Max delay between retries & number of retries

    // Pipe for talking to the base
    radio.openWritingPipe(address_base);
  }

}

void loop(void)
{
  int rgb[3] = {0, 0, 0};

  static unsigned long battery_last_check = 0;
  static unsigned long battery_warning_last = 0;
  static int last_enc_count = 0;

  // http://www.labbookpages.co.uk/electronics/debounce.html
  static uint8_t debounce_switches[3] = {0xFF, 0xFF, 0xFF};
  static unsigned long debounce_sample_last = 0;

  unsigned long now = millis();

  if (mode == MODE_LOW_BATTERY) {

    if (now - battery_warning_last > 250) {
      battery_warning_last = now;
      // blink
      digitalWrite(PIN_LED_RED, !digitalRead(PIN_LED_RED));
    }

  } else {

    // Check the battery level (non blocking)
    if (now - battery_last_check > BATTERY_CHECK_TIME) {
      battery_last_check = now;
      batteryStartReading();
    }

    if (batteryReadComplete()) {
      vcc = batteryRead();
      if (vcc < 3700) {
        // Warning
        mode = MODE_LOW_BATTERY;
        digitalWrite(PIN_LED_RED, LOW);
        digitalWrite(PIN_LED_GREEN, HIGH);
        digitalWrite(PIN_LED_BLUE, HIGH);
      }
    }

    if (last_enc_count != enc_counter) {
      last_enc_count = enc_counter;

      // Set the new colour
      int val = enc_counter;
      setColour(val, rgb);
      showColour(rgb[0], rgb[1], rgb[2]);

      if (PROCESSING) {
        Serial.println(enc_counter, DEC);
      }

      if (RADIO) {
        // Prepare the message.
        Nrf24Payload tx_payload = Nrf24Payload();
        tx_payload.setDeviceId(DEVICE_ID);
        tx_payload.setType('l'); // light command
        tx_payload.setId(msg_id++);
        tx_payload.setA(rgb[0]);
        tx_payload.setB(rgb[1]);
        tx_payload.setC(rgb[2]);
        tx_payload.setVcc(vcc);
        uint8_t tx_buffer[Nrf24Payload_SIZE];
        tx_payload.serialize(tx_buffer);
        if (!radio.write( &tx_buffer, Nrf24Payload_SIZE)) {
          // no ack
        }
      }

    }

    // Poll the switches for setting the mode.
    if (now - debounce_sample_last > 2) {
      // Debounce, then set the mode.
      debounce_sample_last = now;

      uint8_t port_c = PINC;

      // Shift the variable towards the most significant bit
      debounce_switches[0] << 1;
      // Set the least significant bit to the current switch value
      //bitWrite(debounce_switches[0], 0, digitalRead(PIN_SWITCH_A));
      bitWrite(debounce_switches[0], 0, bitRead(port_c, PINC2));

      // Shift the variable towards the most significant bit
      debounce_switches[1] << 1;
      // Set the least significant bit to the current switch value
      //bitWrite(debounce_switches[1], 0, digitalRead(PIN_SWITCH_A));
      bitWrite(debounce_switches[1], 0, bitRead(port_c, PINC3));

      // Shift the variable towards the most significant bit
      debounce_switches[2] << 1;
      // Set the least significant bit to the current switch value
      //bitWrite(debounce_switches[1], 0, digitalRead(PIN_SWITCH_A));
      bitWrite(debounce_switches[2], 0, bitRead(port_c, PINC4));

      if (debounce_switches[0] == 0) {
        mode = MODE_HUE;
      } else if (debounce_switches[1] == 0) {
        mode = MODE_SATURATION;
      } else if (debounce_switches[2] == 0) {
        mode = MODE_INTENSITY;
      }
    }

  }

}

void setColour(int val, int* rgb)
{
  float tmp = (float) abs(val);
  float mapped = 1.0;

  switch (mode) {
    case MODE_HUE: // Hue (0-360 degrees)
      hsi2rgb((float) tmp, saturation, intensity, rgb);
      // Store in the global for use in the other modes.
      hue = tmp;
      break;
    case MODE_SATURATION: // Saturation (0 - 1)
      // float version of map(val, 0, 360, 0, 1)
      mapped = (tmp - 0.0) * (1.0 - 0.0) / (360.0 - 0.0);
      hsi2rgb(hue, mapped, intensity, rgb);
      // Store in the global for use in the other modes.
      saturation = tmp;
      break;
    case MODE_INTENSITY: // Intensity (0 - 1)
      // float version of map(val, 0, 360, 0, 1)
      mapped = (tmp - 0.0) * (1.0 - 0.0) / (360.0 - 0.0);
      hsi2rgb(hue, saturation, mapped, rgb);
      // Store in the global for use in the other modes.
      intensity = tmp;
      break;
  }
}

/**
 * Show the colour on the local led (common anode)
 */
void showColour(byte r, byte g, byte b) {
  analogWrite(PIN_LED_RED, map(r, 0, 255, 255, 0));
  analogWrite(PIN_LED_GREEN, map(g, 0, 255, 255, 0));
  analogWrite(PIN_LED_BLUE, map(b, 0, 255, 255, 0));
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
  // listen for interrupts on A1 & A0
  PCMSK1 =  ((1 << PCINT9) | (1 << PCINT8));
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

/**
 *
 * Function example takes H, S, I, and a pointer to the
 * returned RGB colorspace converted vector. It should
 * be initialized with:
 *
 * int rgb[3];
 *
 * in the calling function. After calling hsi2rgb
 * the vector rgb will contain red, green, and blue
 * calculated values.
 *
 * @see http://blog.saikoled.com/post/43693602826/why-every-led-light-should-be-using-hsi
 */
void hsi2rgb(float H, float S, float I, int* rgb) {
  int r, g, b;
  H = fmod(H,360); // cycle H around to 0-360 degrees
  H = 3.14159*H/(float)180; // Convert to radians.
  S = S>0?(S<1?S:1):0; // clamp S and I to interval [0,1]
  I = I>0?(I<1?I:1):0;

  // Math! Thanks in part to Kyle Miller.
  if(H < 2.09439) {
    r = 255*I/3*(1+S*cos(H)/cos(1.047196667-H));
    g = 255*I/3*(1+S*(1-cos(H)/cos(1.047196667-H)));
    b = 255*I/3*(1-S);
  } else if(H < 4.188787) {
    H = H - 2.09439;
    g = 255*I/3*(1+S*cos(H)/cos(1.047196667-H));
    b = 255*I/3*(1+S*(1-cos(H)/cos(1.047196667-H)));
    r = 255*I/3*(1-S);
  } else {
    H = H - 4.188787;
    b = 255*I/3*(1+S*cos(H)/cos(1.047196667-H));
    r = 255*I/3*(1+S*(1-cos(H)/cos(1.047196667-H)));
    g = 255*I/3*(1-S);
  }
  rgb[0]=r;
  rgb[1]=g;
  rgb[2]=b;
}

/**
 * Sleep and do not wake untill reset.
 */
void powerDown()
{

  if (RADIO) {
    // Turn on the nrf24 radio
    radio.powerDown();
  }

  power_adc_disable();
  power_spi_disable();

}


/**
 * Battery power management funtions
 */

/**
 * Checks to see if the ADC is on.
 */
byte batteryAdcIsOn()
{
  return bit_is_set(ADCSRA, ADEN);
}

/**
 * Get the ADC ready.
 * needs to "warm up" (datasheet: 23.5.2) as we are using the bandgap reference.
 * "The first ADC conversion result after switching reference voltage source may be inaccurate"
 */
void batteryAdcOn()
{
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  ADMUX = (1 << REFS0) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1);

  // Power up the ADC, default values: no interrupts
  ADCSRA = (1 << ADEN);
}


/**
 * Start a non blocking read of the internal voltage ref.
 */
void batteryStartReading()
{
  // Start conversion
  ADCSRA |= (1 << ADSC);
}

/**
 * ADSC will read as one as long as a conversion is in progress. (Datasheet: 23.9.2)
 */
byte batteryIsReading()
{
  return bit_is_set(ADCSRA, ADSC);
}

/**
 * One when a conversion has completed.
 */
byte batteryReadComplete()
{
  // This bit is set when an ADC conversion completes and the Data Registers are updated
  // (datasheet: 23.9.2)
  return bit_is_set(ADCSRA, ADIF);
}

/**
 * Read the result, if available.
 * Returns 0 if still measuring.
 */
long batteryRead()
{
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both
  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000

  // Clear the conversion complete flag.
  // "ADIF is cleared by writing a logical one to the flag" (datasheet: 23.9.2)
  ADCSRA |= (1 << ADIF); // write 1 to flag to clear it

  return result; // Vcc in millivolts
}

void batteryEnsureAdcOff() {
  ADCSRA = 0;
}

