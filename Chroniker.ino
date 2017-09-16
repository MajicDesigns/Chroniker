/*
Chroniker clock

by Marco Colli 
version 1.0 March 2016

Chroniker is a LED neopixel ring clock. It uses a Maxim DS3231M RTC
and a 60 pixel NeoPixel ring controlled by the FastLED libray.

Usage
-----
Brightness setting not curently enabled.

To set time, double press the setting button.
 - Press the button to increase the hour.
 - Long press the button to move to editing minute.
 - Press the button to increase the minutes.
 - Long press the button to end editing.
The seconds will be reset to zero and normal clock will start again.

Library Dependencies:
--------------------
* FastLED       https://github.com/FastLED/FastLED
Note: Refer to this FastLED wiki article for future interfaces that may be
interrupt driven https://github.com/FastLED/FastLED/wiki/Interrupt-problems
-> FastLED.show() should be called as little as possible - it turns off
interrupts when it runs and disables other functions!

* AltSoftSerial https://www.pjrc.com/teensy/td_libs_AltSoftSerial.html
Note: This library can be used as an alternative to SoftwareSerial based on the
defined value USE_ALTSOFTSERIAL in Lighthouse_BT.h

* IRReadOnlyRemote https://github.com/otryti/IRReadOnlyRemote

* MD_KeySwitch  http://github.com/MajicDesigns/MD_KeySwitch
* MD_CircQueue  http://github.com/MajicDesigns/MD_CirQueue
*/

#include <FastLED.h>
#include <Wire.h>       // I2C library for RTC comms
#include <MD_DS3231.h>
#include "Chroniker.h"
#include "Chroniker_UI.h"
#include "Chroniker_BT.h"
#include "Chroniker_IR.h"

// -------------------------------------
// Global data
static runState_e runState = RUN_INIT;
static int8_t curDemo = -1;     // current demo number
static uint8_t curClkFace = 0;  // current clock face

CRGB leds[NUM_LEDS];
MD_CirQueue Q(CIR_QUEUE_SIZE, sizeof(cmdQ_t));

UISwitch UI(MODE_SWITCH_PIN, MODE_SWITCH_ACTIVE);
#if HW_USE_BLUETOOTH
BTSerial BT(BT_RECV_PIN, BT_SEND_PIN, BT_NAME);
#endif
#if HW_USE_IR
IRemote IR(IR_RECV_PIN);
#endif

void(*hwReset) (void) = 0; //declare reset function @ address 0

// -------------------------------------
// Utility functions

void clearAll(void)
{
  // clear the display
  for (uint8_t i = 0; i < NUM_LEDS; i++)
    leds[i] = COL_OFF;
}

void displayTime()
{
#if DEBUG
  char szTime[9];

  sprintf(szTime, "%02d:%02d:%02d", RTC.h, RTC.m, RTC.s);
  PRINT("\nT: ", szTime);
#endif
}

void setBrightness(int16_t delta = 0, boolean newValue = false)
{
  static uint8_t curBright = DEF_BRIGHTNESS;   // brightness setpoint for the clock pixels
  uint8_t ambient = 0;        // ambient value

  //  PRINT("\nsetBright @", curBright);
  //  PRINT(" delta:", delta);

#if USE_LDR_SENSOR
  // check what ambient adjustment is needed
  ambient = analogRead(LDR_SENSOR) / 8;   // map 1024 to smaller number
#endif
  //  PRINT(" ambient:", ambient);

  if (newValue) // just set the new value
    curBright = delta;
  else if (delta > 0) // work out the adjusted delta now
  {
    if (curBright + delta - ambient > MAX_BRIGHTNESS)
      curBright = MAX_BRIGHTNESS;
    else
      curBright += delta;
  }
  else
  {
    if (curBright + delta - ambient < MIN_BRIGHTNESS)
      curBright = MIN_BRIGHTNESS;
    else
      curBright += delta; // delta already -ve
  }
  //  PRINT("  new setting:", curBright - ambient);

  // finally set the brightness inthe hardware
  FastLED.setBrightness(curBright - ambient);
}

// -------------------------------------
// Clock update and display

void showClock(bool bOnH = true, bool bOnM = true, bool bOnS = true)
{
  typedef struct 
  { 
    uint8_t x; 
    CRGB::HTMLColorCode col; 
  } tuple;

  #define swap(a, b) {tuple t; t=a; a=b; b=t; }

  tuple cx[3] = { 0 };

  displayTime();

  clearAll();

  // set up the pixel indices for the h[0], m[1], s[2] 
  cx[0].x = (RTC.h * (NUM_LEDS / 12)) + (RTC.m / (NUM_LEDS / 5));
  cx[0].col = (bOnH ? COL_HHAND : COL_OFF);
  cx[1].x = RTC.m;
  cx[1].col = (bOnM ? COL_MHAND : COL_OFF);
  cx[2].x = RTC.s;
  cx[2].col = (bOnS ? COL_SHAND : COL_OFF);

  // now draw the clock face depending on what is currently selected
  switch (curClkFace)
  {
  case 0: // standard analog clock face
  case 1: // minmalist clock face (hms dots only)
  {
    if (curClkFace == 0)  // put in the hour marks
    {
      // Set default 'mark' colours for the whole wheel
      for (uint8_t i = 0; i < NUM_LEDS; i++)
        leds[i] = COL_MMARK;
      for (uint8_t i = 0; i < NUM_LEDS; i += (NUM_LEDS / 12))
        leds[i] = (i == 0 ? COL_12HMARK : COL_HMARK);
    }

    // Now set the 'hand' colours. Colour viewing priority is
    // hour, minute, and then seconds, so update hands in reverse sequence
    leds[cx[2].x] = cx[2].col;
    leds[cx[1].x] = cx[1].col;
    leds[cx[0].x] = cx[0].col;
  }
  break;

  case 2: // 'pie chart' face
  {
    // sort the h, m, s in increasing order
    if (cx[0].x > cx[1].x) swap(cx[0], cx[1]);
    if (cx[0].x > cx[2].x) swap(cx[0], cx[2]);
    if (cx[1].x > cx[2].x) swap(cx[1], cx[2]);

    // now draw in the colours
    {
      uint8_t i = 0;

      while (i <= cx[0].x) leds[i++] = cx[0].col;
      while (i <= cx[1].x) leds[i++] = cx[1].col;
      while (i <= cx[2].x) leds[i++] = cx[2].col;
    }
  }
  break;

  default:
    curClkFace = 0;   // gone too far - reset this variable
  }

  // update the hardware
  setBrightness();
  FastLED.show();
}

void cbClock(void)
// Only execute callback when we are in simple display mode
{
  digitalWrite(13, !digitalRead(13));
  RTC.readTime();
  showClock();
}

boolean adjustTime(uint8_t cmd, uint8_t data)
// return true when adjustment cycle completed
{
  static enum { SET_IDLE, SET_HOUR, SET_MINUTE, SET_END } adjState = SET_IDLE;
  static uint32_t timeStart;  // tracking blink delay time
  static bool bBlink = false; // toggle for blink status

  // run the FSM for setup
  switch (adjState)
  {
  case SET_IDLE:     // idle
    PRINTS("\nSET_IDLE");
    timeStart = millis();
    adjState = SET_HOUR;
    break;

  case SET_HOUR: // setting hour
    switch (cmd)
    {
    case CMD_SELECT:
      if (data == CS_NEXT) adjState = SET_MINUTE;
      break;

    case CMD_VALUE: // hour +/-
      PRINTS("\nSET_HOUR");
      switch (data)
      {
      case CV_UP: 
        RTC.h = (RTC.h + 1) % 12;
        break;
      case CV_DOWN:
        if (RTC.h == 0)
          RTC.h = 11;
        else
          RTC.h--;
        break;
      }
      showClock(bBlink, true, true);
      break;
    }
    break;

  case SET_MINUTE: // setting minute
    switch (cmd)
    {
    case CMD_SELECT:
      if (data == CS_NEXT) adjState = SET_END;
      if (data == CS_PREV) adjState = SET_HOUR;
      break;

    case CMD_VALUE: // min +/-
      PRINTS("\nSET_MINUTE");
      switch (data)
      {
      case CV_UP:
        RTC.m = (RTC.m + 1) % 60;
        break;
      case CV_DOWN:
        if (RTC.m == 0)
          RTC.m = 59;
        else
          RTC.m--;
        break;
      }
      showClock(true, bBlink, true);
      break;
    }
    break;

  case SET_END:   // update clock and exit
    PRINTS("\nSET_END");
    RTC.s = 0;
    RTC.writeTime();
    adjState = SET_IDLE;
    break;
  }

  // toggle the blink status with the required delay
  if (millis() - timeStart >= BLINK_DELAY)
  {
    bBlink = !bBlink;
    timeStart = millis();

    switch (adjState)
    {
    case SET_HOUR:   showClock(bBlink, true, true); break;
    case SET_MINUTE: showClock(true, bBlink, true); break;
    }
  }

  return(adjState == SET_IDLE);
}

void lampTest(void)
// Do a lamp test
// Blocking until the test is completed
{
  const CRGB::HTMLColorCode colCycle[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};

  FastLED.setBrightness(200);   // pick something bright

  // Clear the display
  for (uint8_t i = 0; i < NUM_LEDS; i++)
    leds[i] = CRGB::Black;
  FastLED.show();

  // cycle through the colours, one per sector
  for (uint8_t c = 0; c < ARRAY_SIZE(colCycle); c++)
  {
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = colCycle[c];
      FastLED.show();
      delay(30);
    }
  }
}

// -------------------------------------
// Demo code
// Taken from the FastLED examples folder and adapted to run
// properly here

#if HW_USE_IR   // if IR used slow down the FastLED updates as they block interrupts
#define FRAMES_PER_SECOND 12
#else
#define FRAMES_PER_SECOND 25
#endif
#define ANIMATION_DELAY (1000/FRAMES_PER_SECOND)

static uint32_t timeStart = 0;
static uint32_t timeHue = 0;
static uint8_t hue = 0;
static int16_t idx = 0;

void fadeall(void) 
{ 
  for (int i = 0; i < NUM_LEDS; i++) 
    leds[i].nscale8(250); 
}

void initDemo(void)
{
  clearAll();
  timeStart = timeHue = millis();
  idx = hue = 0;
}

void updateHue(void)
{
  if (millis() - timeHue >= 20)
  {
    hue++;
    timeHue = millis();
  }
}

void demoCylon(bool bInit)
{
  static uint8_t state = 0;
  static int16_t idx = 0;

  if (bInit)
  {
    initDemo();
    state = 0;
  }

  if (millis() - timeStart < ANIMATION_DELAY)
    return;

  switch (state)
  {
    case 0: // First slide the led in one direction
      timeStart = millis();
      leds[idx++] = CHSV(hue++, 255, 255); // Set the i'th led to red 
      FastLED.show();       // Show the leds
      fadeall();
      if (idx == NUM_LEDS) state = 1;
      break;

    case 1: // Now go in the other direction.  
      leds[--idx] = CHSV(hue++, 255, 255);     // Set the i'th led to red 
      FastLED.show();     // Show the leds
      fadeall();
      if (idx < 0) state = 0;
      break;

    default:
      state = 0;
      break;
  }
}

void demoRainbow(bool bInit)
{
  if (bInit) initDemo();

  if (millis() - timeStart < ANIMATION_DELAY)
    return;
  timeStart = millis();

  // FastLED's built-in rainbow generator
  fill_rainbow(leds, NUM_LEDS, hue, 7);
  updateHue();
  FastLED.show();
}

void demoRainbowWithGlitter(bool bInit)
{
  if (bInit) initDemo();

  if (millis() - timeStart < ANIMATION_DELAY)
    return;
  timeStart = millis();

  // built-in FastLED rainbow, plus some random sparkly glitter
  fill_rainbow(leds, NUM_LEDS, hue, 7);
  if (random8() < 80)
    leds[random16(NUM_LEDS)] += CRGB::White;
  FastLED.show();
}

void demoConfetti(bool bInit)
{
  if (bInit) initDemo();

  if (millis() - timeStart < ANIMATION_DELAY)
    return;
  timeStart = millis();

  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(hue + random8(64), 200, 255);
  FastLED.show();
}

void demoSinelon(bool bInit)
{
  if (bInit) initDemo();

  if (millis() - timeStart < ANIMATION_DELAY)
    return;
  timeStart = millis();

  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, NUM_LEDS, 20);
  int pos = beatsin16(13, 0, NUM_LEDS);
  leds[pos] += CHSV(hue, 255, 192);
  updateHue();
  FastLED.show();
}

void demoBPM(bool bInit)
{
  const uint8_t BeatsPerMinute = 62;
  const CRGBPalette16 palette = PartyColors_p;

  if (bInit) initDemo();

  if (millis() - timeStart < ANIMATION_DELAY)
    return;
  timeStart = millis();

  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);

  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
  updateHue();
  FastLED.show();
}

void demoJuggle(bool bInit) 
{
  byte dothue = 0;

  if (bInit) initDemo();

  if (millis() - timeStart < ANIMATION_DELAY)
    return;
  timeStart = millis();

  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, NUM_LEDS, 20);
  for (int i = 0; i < 8; i++) 
  {
    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  FastLED.show();
}

// -------------------------------------
// Command detection
void getCommand(void)
// Handle the interfaces to queue commands received
{
#if HW_USE_BLUETOOTH
  if (BT.getCommand())    // Bluetooth interface
  {
    PRINTCMD("\n+Q BT ", BT.c);
    ENQUEUE(BT.c);
  }
#endif
#if HW_USE_IR
  if (IR.getCommand())    // Bluetooth interface
  {
    PRINTCMD("\n+Q IR ", IR.c);
    ENQUEUE(IR.c);
  }
#endif

  if (UI.getCommand())  // Physical (switch) interface
  {
    PRINTCMD("\n+Q SW ", UI.c);
    ENQUEUE(UI.c);
  }
}

// -------------------------------------
// Arduino Standard functions
void setup(void)
{
#if DEBUG
  Serial.begin(57600);
#endif
  PRINTS("\n[Chroniker Clock Debug]");

  // Start up the library code(s)
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  Wire.begin();   // I2C library

  // turn the clock on to 12H mode, 
  // and set for 1 second callback type alarm
  RTC.control(DS3231_12H, DS3231_ON);
  RTC.setAlarm1Callback(cbClock);
  RTC.setAlarm1Type(DS3231_ALM_SEC);

  // Start the control interfaces
#if HW_USE_BLUETOOTH
  BT.begin(); // Bluetooth
#endif
#if HW_USE_IR
  IR.begin(); // IR Remote
#endif
  UI.begin(); // User switches

#if USE_LDR_SENSOR
  pinMode(LDR_SENSOR, INPUT); // set the LDR input port
#endif

  // Check if lamp test is needed invoked -
  // startup with the mode switch active.
  if (digitalRead(MODE_SWITCH_PIN) == MODE_SWITCH_ACTIVE)
  {
    // queue the lamp test
    PRINTS("\nLamp Test");
    ENQUEUE_C(CMD_LAMPTEST, 0);

    while (digitalRead(MODE_SWITCH_PIN) == MODE_SWITCH_ACTIVE)
      ; // wait for switch release
    delay(100); // avoid switch bounce
  } 
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
}

void loop (void) 
{
  static bool newDemo;

  cmdQ_t c = { 0, 0 };

  // -- Process the command queue
  getCommand();
  if (!Q.isEmpty())
  {
    DEQUEUE(c);
    PRINTCMD("\n-Q ", c);

    switch (c.cmd)
    {
    case CMD_LAMPTEST:  // do lamp test cycle
      lampTest();
      break;

    case CMD_RESET:     // soft reset (reboot)
      hwReset();
      break;

    case CMD_SETUP:     // set the time on the clock
      runState = RUN_SETUP;
      break;
      
    case CMD_DEMO:     // set the time on the clock
      if (c.data == CD_OFF)
      {
        curDemo = -1;
        runState = RUN_INIT;
      }
      else
      {
        runState = RUN_DEMO;
        curDemo++;
        newDemo = true;
      }
      break;

    case CMD_CLKFACE:
      if (c.data == CC_CYCLE)
        curClkFace++;
      break;

    case CMD_BRIGHT:  // change base brightness
      setBrightness(c.data, true);
      PRINT("\nsetBright = ", c.data);
      break;

    case CMD_TIME:    // set the time directly
      RTC.s = (c.data & 0xff);
      c.data >>= 8;
      RTC.m = (c.data & 0xff);
      c.data >>= 8;
      RTC.h = (c.data & 0xff);
      RTC.writeTime();
    }
  }

  // -- Execute the LED display FSM
 switch (runState)
  {
  case RUN_INIT:
    PRINTFSM("\nRUN_INIT", runState);
    setBrightness();    // set the default brightness levels for ambient conditions
    runState = RUN_NORMAL;
    break;

  case RUN_NORMAL:
    PRINTFSM("\nRUN_NORMAL", runState);
    RTC.checkAlarm1();  // callback will do the work of updates
    break;

  case RUN_SETUP:
    PRINTFSM("\nRUN_SETUP", runState);
    if (adjustTime(c.cmd, c.data))
      runState = RUN_INIT;
    break;

  case RUN_DEMO:
    PRINTFSM("\nRUN_DEMO", runState);
    switch (curDemo)
    {
      case 0: demoCylon(newDemo);    newDemo = false; break;
      case 1: demoRainbow(newDemo);  newDemo = false; break;
      case 2: demoRainbowWithGlitter(newDemo); newDemo = false; break;
      case 3: demoConfetti(newDemo); newDemo = false; break;
      case 4: demoSinelon(newDemo);  newDemo = false; break;
      case 5: demoBPM(newDemo);      newDemo = false; break;
      case 6: demoJuggle(newDemo);   newDemo = false; break;
      default:    // cycled too far, reset it
        curDemo = 0;
        newDemo = true;
        break;
    }
    break;

  default:
    PRINTFSM("\n??UNKNOWN", runState);
    runState = RUN_INIT;
    break;
  }
}
