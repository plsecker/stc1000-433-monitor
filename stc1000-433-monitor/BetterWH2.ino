//WeatherSensorWH2 from https://github.com/lucsmall/BetterWH2 from https://github.com/lucsmall/WH2-Weather-Sensor-Library-for-Arduino
//WeatherSensorWH2 adapted for the ESP8266
//Note a timeout peculiarity of the STC-1000 temperature controller is included but this won't preclude working with other fine-offset stations

#include "pins_arduino.h"
#include "ESP8266WiFi.h"
extern "C" {
#include "user_interface.h"
}
#include "BetterWH2.h"

// tick counter (200uS)
volatile unsigned long counter = 0;

// NodeMCU pin D2
#define RF_IN D2
// NodeMCU pin D1 for monitoring
#define RF_OUT D1

// 1 is indicated by 500uS pulse
// wh2_accept from 2 = 400us to 3 = 600us
#define IS_HI_PULSE(interval)   (interval >= 2 && interval <= 5) //#define IS_HI_PULSE(interval) (interval >= 2 && interval <= 3) original

// 0 is indicated by ~1500us pulse
// wh2_accept from 7 = 1400us to 8 = 1600us
#define IS_LOW_PULSE(interval)  (interval >= 6 && interval <= 8) //#define IS_LOW_PULSE(interval) (interval >= 7 && interval <= 8) original

// worst case packet length
// 6 bytes x 8 bits x (1.5 + 1) = 120ms; 120ms = 200us x 600
#define HAS_TIMED_OUT(interval) (interval > 600)

// we expect 1ms of idle time between pulses
// so if our pulse hasn't arrived by 1.2ms, reset the wh2_packet_state machine
// 6 x 200us = 1.2ms
// need 8 for STC-1000
#define IDLE_HAS_TIMED_OUT(interval) (interval > 8)

// our expected pulse should arrive after 1ms
// we'll wh2_accept it if it arrives after
// 4 x 200us = 800us
#define IDLE_PERIOD_DONE(interval) (interval >= 4)
// Shorthand for inputs
#define RF_HI (digitalRead(RF_IN) == HIGH)
#define RF_LOW (digitalRead(RF_IN) == LOW)

// swap comments to replicate input to output to debug reception on a scope
#define DEBUG_LOW  
#define DEBUG_HIGH 
//#define DEBUG_LOW  digitalWrite(RF_OUT, LOW);
//#define DEBUG_HIGH  digitalWrite(RF_OUT, HIGH);


// wh2_flags
#define GOT_PULSE 0x01
#define LOGIC_HI  0x02
volatile byte wh2_flags = 0;
volatile byte wh2_packet_state = 0;
volatile int wh2_timeout = 0;
byte wh2_packet[5];
byte wh2_calculated_crc;

void ICACHE_RAM_ATTR inline handler (void) {
  timer0_write(ESP.getCycleCount() + INTERRUPT_INTERVAL * CPU_FREQUENCY);
  static byte sampling_state = 0;
  static byte count;
  static boolean was_low = false;

  counter++;

  switch (sampling_state) {
    case 0: // waiting
      wh2_packet_state = 0;
      if (RF_HI) {
        DEBUG_HIGH
        if (was_low) {
          count = 0;
          sampling_state = 1;
          was_low = false;
        }
      } else {
        DEBUG_LOW
        was_low = true;
      }
      break;
    case 1: // acquiring first pulse
      count++;
      // end of first pulse
      if (RF_LOW) {
        DEBUG_LOW
        if (IS_HI_PULSE(count)) {
          wh2_flags = GOT_PULSE | LOGIC_HI;
          sampling_state = 2;
          count = 0;
        } else if (IS_LOW_PULSE(count)) {
          wh2_flags = GOT_PULSE; // logic low
          sampling_state = 2;
          count = 0;
        } else {
          sampling_state = 0;
        }
      }
      else
          digitalWrite(RF_OUT, HIGH);
      break;
    case 2: // observe 1ms of idle time
      count++;
      if (RF_HI) {
        DEBUG_HIGH
        if (IDLE_HAS_TIMED_OUT(count)) {
          sampling_state = 0;
        } else if (IDLE_PERIOD_DONE(count)) {
          sampling_state = 1;
          count = 0;
        }
      }
      else
          DEBUG_LOW
      break;
  }

  if (wh2_timeout > 0) {
    wh2_timeout++;
    if (HAS_TIMED_OUT(wh2_timeout)) {

//      Serial.print("T");

      wh2_packet_state = 0;
      wh2_timeout = 0;
    }
  }
}


void setup_wh2() {

  pinMode(RF_IN, INPUT);
  pinMode(RF_OUT, OUTPUT);

  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(handler);
  timer0_write(ESP.getCycleCount() + INTERRUPT_INTERVAL * CPU_FREQUENCY);
  interrupts();
}

boolean wh2_process(struct Fineoffset *stc_values)
{

  static unsigned long old = 0, packet_count = 0, bad_count = 0, average_interval;
  unsigned long spacing, now;
  byte i;

  bool got_valid_packet = 0;


  if (wh2_flags) {
    if (wh2_accept()) {
      // calculate the CRC
      wh2_calculate_crc();

      now = millis();
      spacing = now - old;
      old = now;
      packet_count++;
      average_interval = now / packet_count;
      if (!wh2_valid()) {
        bad_count++;
      }

      // flash green led to say got packet
      Serial.println("");

      for (i = 0; i < 5; i++) {
        Serial.print("0x");
        Serial.print(wh2_packet[i], HEX);
        Serial.print("/");
        Serial.print(wh2_packet[i], DEC);
        Serial.print(" ");
      }
      Serial.print("| Sensor ID: ");
      Serial.print(wh2_sensor_id());
      Serial.print(" | ");

      Serial.print(wh2_humidity());//batt

      Serial.print("v | ");
      Serial.print(wh2_temperature(), DEC);

      Serial.print(" | ");
      Serial.print((wh2_valid() ? "OK" : "BAD"));
      Serial.print(" | packet_count ");
      Serial.println(packet_count);

      if (wh2_valid()) {
        got_valid_packet = true;
        stc_values -> valid_packet = true;
        stc_values -> temperature = wh2_temperature();
        stc_values -> humidity = wh2_humidity();
      }
      else
        stc_values -> valid_packet = false;
    }
    wh2_flags = 0x00;
  }

  return got_valid_packet;  //got_valid_packet;
}

// processes new pulse
boolean wh2_accept()
{
  static byte packet_no, bit_no, history;
  char  snum[5];

  // reset if in initial wh2_packet_state
  if (wh2_packet_state == 0) {
    // should history be 0, does it matter?
    history = 0xFF;
    wh2_packet_state = 1;
    // enable wh2_timeout
    wh2_timeout = 1;
  } // fall thru to wh2_packet_state one

  // acquire preamble
  if (wh2_packet_state == 1) {
    // shift history right and store new value
    history <<= 1;
    // store a 1 if required (right shift along will store a 0)
    if (wh2_flags & LOGIC_HI) {
      history |= 0x01;
    }
    // check if we have a valid start of frame
    // xxxxx110
    if ((history & B00000111) == B00000110) {
      // need to clear packet, and counters
      packet_no = 0;
      // start at 1 becuase only need to acquire 7 bits for first packet byte.
      bit_no = 1;
      wh2_packet[0] = wh2_packet[1] = wh2_packet[2] = wh2_packet[3] = wh2_packet[4] = 0;
      // we've acquired the preamble
      wh2_packet_state = 2;
    }
    return false;
  }
  // acquire packet
  if (wh2_packet_state == 2) {

    wh2_packet[packet_no] <<= 1;
    if (wh2_flags & LOGIC_HI) {
      wh2_packet[packet_no] |= 0x01;
    }

    bit_no ++;
    if (bit_no > 7) {
      bit_no = 0;
      packet_no ++;
    }

    // convert to string
    //itoa(packet_no, snum, 10);
    //Serial.print(snum);

    if (packet_no > 4) {
      // start the sampling process from scratch
      wh2_packet_state = 0;
      // clear wh2_timeout
      wh2_timeout = 0;
      return true;
    }
  }
  return false;
}


void wh2_calculate_crc()
{
  wh2_calculated_crc = crc8(wh2_packet, 4);
}

bool wh2_valid()
{
  return (wh2_calculated_crc == wh2_packet[4]);
}

int wh2_sensor_id()
{
  return (wh2_packet[0] << 4) + (wh2_packet[1] >> 4);
}

byte wh2_humidity()
{
  return wh2_packet[3];
}

/* Temperature in deci-degrees. e.g. 251 = 25.1 */
int wh2_temperature()
{
  int temperature;
  temperature = ((wh2_packet[1] & B00000111) << 8) + wh2_packet[2];
  // make negative
  if (wh2_packet[1] & B00001000) {
    temperature = -temperature;
  }
  return temperature;
}

uint8_t crc8( uint8_t *addr, uint8_t len)
{
  uint8_t crc = 0;

  // Indicated changes are from reference CRC-8 function in OneWire library
  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x80; // changed from & 0x01
      crc <<= 1; // changed from right shift
      if (mix) crc ^= 0x31;// changed from 0x8C;
      inbyte <<= 1; // changed from right shift
    }
  }
  return crc;
}
