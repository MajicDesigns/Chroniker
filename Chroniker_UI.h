#pragma once

#include <Arduino.h>
#include <MD_KeySwitch.h>
#include "Chroniker.h"

/*
User Interface class

The User Interface Class implements the switch based interface for Chroniker
A keypress needs to be interpreted based on the current run mode state of the
FSM and the type of keypress.
*/

class UISwitch: public iChroniker
{
public:
  // Functions
  UISwitch(uint8_t pinMode, uint8_t logicMode)
  {
    c.cmd = c.data = 0;
    _swMode = new MD_KeySwitch(pinMode, logicMode);
  };

  ~UISwitch() 
  { 
    delete _swMode;
  };

  virtual void begin(void)
  // initialise library
  {
    _swMode->begin();
    _swMode->enableRepeat(false);
  }

  virtual bool getCommand(void)
  // Returns true if a keypress was processed and saved to public variables.
  {
    MD_KeySwitch::keyResult_t k = _swMode->read();

    if (k == MD_KeySwitch::KS_NULL) 
    {
      // no key pressed so no command recorded
      c.cmd = c.data = 0;
    }
    else
    {
      switch (k)
      {
        case MD_KeySwitch::KS_PRESS:     PRINTS("\nPRESS");  c.cmd = CMD_VALUE;  c.data = CV_UP; break;
        case MD_KeySwitch::KS_DPRESS:    PRINTS("\nDPRESS"); c.cmd = CMD_SETUP;  c.data = 0;  break;
        case MD_KeySwitch::KS_LONGPRESS: PRINTS("\nLPRESS"); c.cmd = CMD_SELECT; c.data = CS_NEXT; break;
      }
    }

    return(c.cmd != 0);
  }

private:
  MD_KeySwitch* _swMode;
};
