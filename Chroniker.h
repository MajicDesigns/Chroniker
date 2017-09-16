#pragma once
/*
Chroniker.h

Chroniker is a LED neopixel ring clock.

See main program file for more extensive comments and dependencies
*/

#define DEBUG 0 // Switch debug output on and off by 1 or 0

// Set the hardware choices
#define USE_LDR_SENSOR    1   // Use an LDR sensor for auto brightness adjustment
#define HW_USE_IR         0   // Use the Infrared remote control
#define HW_USE_BLUETOOTH  1   // Use a Bluetooth interface - need to define type below
// Define the type of BT hardware being used.
// Only one of these options is enabled at any time.
// The HM-10 comes in two versions - JHHuaMao (HMSoft) version and Bolutek - AT command line ending differ.
#define HW_USE_HC05  0
#define HW_USE_HC06  1
#define HW_USE_HM10_HMSOFT 0
#define HW_USE_HM10_OTHER	 0

// Miscellaneous --------
#define	ARRAY_SIZE(x)	(sizeof(x)/sizeof(x[0]))
const uint16_t BLINK_DELAY = 100;

// Clock Pixel colours --
// Colour table mapping in case someone changes their mind
const CRGB::HTMLColorCode COL_OFF     = CRGB::Black;        // 'off' colour
const CRGB::HTMLColorCode COL_MMARK   = CRGB::Black;        // minute mark
const CRGB::HTMLColorCode COL_HMARK   = CRGB::OrangeRed;    // hour mark
const CRGB::HTMLColorCode COL_12HMARK = CRGB::Orange;       // 12 hour mark
const CRGB::HTMLColorCode COL_HHAND   = CRGB::YellowGreen;  // hour hand
const CRGB::HTMLColorCode COL_MHAND   = CRGB::Green;        // minute hand
const CRGB::HTMLColorCode COL_SHAND   = CRGB::Blue;         // second hand
// ----------------------

// FastLED --------------
const uint8_t NUM_LEDS = 60;      // number of LEDS in the circle

#define LED_TYPE    WS2812
#define COLOR_ORDER GRB

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
const uint8_t LED_PIN = 6;
// const uint8_t CLOCK_PIN = 13;
// ----------------------

// MD_Keyswitch ---------
const uint8_t MODE_SWITCH_PIN = 4;
const uint8_t MODE_SWITCH_ACTIVE = LOW;    // active or 'ON' state for the switch
// ----------------------

// LDR bright control ---
const uint8_t MIN_BRIGHTNESS = 128;  // minimum brightness allowed
const uint8_t MAX_BRIGHTNESS = 255; // maximum brightness allowed
const uint8_t DEF_BRIGHTNESS = (MIN_BRIGHTNESS + ((MAX_BRIGHTNESS - MIN_BRIGHTNESS)/2));
const uint8_t STEP_BRIGHTNESS = 4;  // adjustment steps for brightness controls

#if USE_LDR_SENSOR
const uint8_t LDR_SENSOR = A3;    // light sensitive resistor for brightness
#endif
// ----------------------

// Bluetooth Interface --
#if HW_USE_HC05
const uint8_t HC05_SETUP_ENABLE = 7;
#endif

// Bluetooth Serial comms hardware and parameters
const uint8_t BT_RECV_PIN = 8;   // Arduino receive -> Bluetooth TxD pin
const uint8_t BT_SEND_PIN = 9;   // Arduino send -> Bluetooth RxD pin
const char BT_NAME[] = "Chroniker";
// ----------------------

// IR Interface ---------
const uint8_t IR_RECV_PIN = 2;   // pin for the demodulated IR signal - must support IRQ
// ----------------------

//=====================================================
//======= END OF USER CONFIGURATION PARAMETERS ========
//=====================================================
// Debugging switches and macros
#if DEBUG
#define PRINTS(s)     { Serial.print(F(s)); }
#define PRINT(s,v)    { Serial.print(F(s)); Serial.print(v); }
#define PRINTX(s,v)   { Serial.print(F(s)); Serial.print(F("0x")); Serial.print(v, HEX); }
#define PRINTCMD(s,c) { Serial.print(F(s)); \
                        Serial.print(F("[")); \
                        Serial.print((char)c.cmd); \
                        Serial.print(F(",")); \
                        Serial.print(isalnum(c.data) ? (char)c.data : c.data); \
                        Serial.print(F("] ")); \
                      }
#define PRINTFSM(s,f) { static uint8_t lastState = 255; \
                        if (f != lastState) \
                        { \ 
                          Serial.print(F(s)); \
                          lastState = f; \
                        } \
                      }
#else
#define PRINTS(s)
#define PRINT(s,v)
#define PRINTX(s,v)
#define PRINTCMD(s,c)
#define PRINTFSM(s,f)
#endif

// Loop() fsm states
enum runState_e { RUN_INIT, RUN_NORMAL, RUN_SETUP, RUN_DEMO };

// Commands for the cmd queue. Each command may have associated with it a data value that
// indicates what to do (eg, on or off).
// These commands are placed in the command queue by user actions (physical switches or remote
// operation (eg, Bluetooth) and read by the execution code to enact the required mode.

const char CMD_LAMPTEST = 'L';  // do a lamp test
const char CMD_RESET    = 'Y';  // reset the system (soft reboot)
const char CMD_SETUP    = 'X';  // step through setup mode for the clock
const char CMD_BRIGHT   = 'B';  // set specified brightness - data = brightness level (3 digits 000-255)
const char CMD_SELECT   = 'S';  // select command - data 0 = next, 1 = previous
const char CMD_VALUE    = 'V';  // change value command - data 0 = DOWN, 1 = UP
const char CMD_TIME     = 'T';  // set the time directly - data = HHMMSS
const char CMD_DEMO     = 'D';  // cool light demo - data 0 = off, 9 to cycle
const char CMD_CLKFACE  = 'C';  // clock face - data 9 to cycle 

// command SELECT data
const uint8_t CS_NEXT = '0';    // select next
const uint8_t CS_PREV = '1';    // select previous

// command VALUE data
const uint8_t CV_DOWN = '0';    // value decrease
const uint8_t CV_UP   = '1';    // value increase

// command DEMO data
const uint8_t CD_OFF  = '0';    // demo off
const uint8_t CD_CYCLE = '9';   // demo cycle

// command CLKFACE data
const uint8_t CC_CYCLE = '9';   // demo cycle

typedef struct
{
  uint8_t cmd;    // on of the commands
  uint32_t data;   // associated data if needed
} cmdQ_t;

#define CIR_QUEUE_SIZE 4
#define ENQUEUE(z)      { Q.push((uint8_t *)&z); }
#define ENQUEUE_C(c, d) { cmdQ_t cq; cq.cmd=c; cq.data=d; Q.push((uint8_t *)&cq); }
#define DEQUEUE(z)      { Q.pop((uint8_t *)&z); }
#define DEQUEUE_C(c, d) { cmdQ_t cq; Q.pop((uint8_t *)&cq); c=cq.cmd; d=cq.data; }

// Container class for interface definitions
class iChroniker
{
public:
  cmdQ_t  c;    // command received

  virtual void begin(void) { PRINTS("\niCroniker begin"); };
  virtual bool getCommand(void) { PRINTS("\niCroniker getCommand"); };
};
