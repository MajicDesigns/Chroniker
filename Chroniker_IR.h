#pragma once

#include <Arduino.h>
#include <IRReadOnlyRemote.h>
#include "Chroniker.h"

/*
Infrared Remote class

The IR Remote class implements the IR interface for Chroniker
An IR keypress needs to be interpreted based on the current run mode state of the
FSM and the type of keypress.
*/

class IRemote: public iChroniker
{
public:
  // Functions
  IRemote(uint8_t irqPin)
  {
    c.cmd = c.data = 0;
    _IR = new IRReadOnlyRemote(irqPin);
  };

  ~IRemote() 
  {
    delete _IR;
  };

  virtual bool getCommand(void)
    // Returns true if a keypress was processed and saved to public variables.
  {
    uint32_t irCode;
    static uint16_t accum = 0;

    c.cmd = 0;
    irCode = _IR->read();

    // 'empty' or 'IR repeat' codes -> nothing to process
    if (irCode == 0 || irCode == 0xFFFFFFFF)
      return(false);

    PRINTX("\nIR: Rcv ", irCode);

    // search the data table for a matching value
    for (uint8_t i = 0; i < ARRAY_SIZE(IRCodes); i++)
    {
      if (IRCodes[i].irCode == irCode)
      {
        PRINTX(" Matched ", IRCodes[i].irCode);

        if (IRCodes[i].cmd != -1)
        {
          // actual command
          c.cmd = IRCodes[i].cmd;
          if (IRCodes[i].data == -1)
          {
            c.data = accum;
            accum = 0;    // reset value
          }
          else
          {
            c.data = IRCodes[i].data;
          }
        }
        else // IRCodes[i].cmd == -1 -> accumulator value
        {
          accum = (accum * 10) + IRCodes[i].data;
          PRINT(" new accum: ", accum);
        }
        break;
      }
    }

    return(c.cmd != 0);
  }

private:
  typedef struct keyTable_t
  {
    uint32_t  irCode;
    int8_t    cmd;
    int8_t    data;
  };

  IRReadOnlyRemote *_IR;
  static const keyTable_t IRCodes[21];
};

const IRemote::keyTable_t IRemote::IRCodes[] =
{
  { 0xFFA25D, CMD_RESET, 0 },    // On/Off
  { 0xFF629D, CMD_SETUP, 0 },    // Mode
  { 0xFFE21D, CMD_LAMPTEST, 0 }, // Mute
  { 0xFF22DD, CMD_CLKFACE, CC_CYCLE }, // >||
  { 0xFF02FD, CMD_SELECT, CS_PREV },   // |<<
  { 0xFFC23D, CMD_SELECT, CS_NEXT },   // >>|
  { 0xFFE01F, CMD_BRIGHT, -1 },     // EQ
  { 0xFFA857, CMD_VALUE, CV_DOWN }, // -
  { 0xFF906F, CMD_VALUE, CV_UP },   // +
  { 0xFF9867, CMD_DEMO, CD_CYCLE }, // Shuffle
  { 0xFFB04F, CMD_DEMO, CD_OFF },   // USD
  { 0xFF6897, -1, 0 }, // 0
  { 0xFF30CF, -1, 1 }, // 1
  { 0xFF18E7, -1, 2 }, // 2
  { 0xFF7A85, -1, 3 }, // 3
  { 0xFF10EF, -1, 4 }, // 4
  { 0xFF38C7, -1, 5 }, // 5
  { 0xFF5AA5, -1, 6 }, // 6
  { 0xFF42BD, -1, 7 }, // 7
  { 0xFF4AB5, -1, 8 }, // 8
  { 0xFF52AD, -1, 9 }, // 9
};
