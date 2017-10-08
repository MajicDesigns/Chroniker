#pragma once

#define USE_ALTSOFTSERIAL 0

#include <Arduino.h>
#include <MD_CirQueue.h>
#include "Chroniker.h"

#if USE_ALTSOFTSERIAL
#include <AltSoftSerial.h>
#else
#include <SoftwareSerial.h>
#endif

/*
Bluetooth interface class

The Bluetooth interface class implements the BT interface for Chroniker.
Chroniker is configured as a BT slave (ie, it only responds to commands and
data requests from the BT master) using a simple protocol described below.
The Serial interface is implemented using the SoftwareSerial library and slow bit
rates are sufficient for the data load.

Communications between BT master and slave are implemented in structured packets
with the following request/response pattern
<Start_Char><Command><Data><End_Char>
where
<Start_Char> is a single character used to synch the start of the data packet (PKT_START)
<Command> is an identifier for the action requested (PKT_CMD_*)
<Data> is an optional single character for data supporting <Command>
<End_Char> marks the end of a data packet (PKT_END)

A request is always followed by a response in the format
<Start_Char><Cmd><Error_Code><End_Char>
where
<Start_Char> and <End_Char> are as defined above
<Cmd> is always PKT_CMD_ACK
<Error_Code> is an ASCII digit with the result of the action (PKT_ERR_*)

Packets of data time out if they are not received in their entirety within
BT_COMMS_TIMEOUT milliseconds and the requester should expect a response with the same
timeout BT_COMMS_TIMEOUT period.

The Bluetooth device is initialised in the begin() method. The hardware MUST NOT BE CONNECTED
to a master (eg, BT application) or the initialisation parameters will be passed through the 
serial interface rather than setting up the BT device.
*/

// Serial protocol parameters
const uint16_t BT_COMMS_TIMEOUT = 1000; // Protocol packet timeout period (start to end packet within this period)

const char PKT_START = '*';    // protocol packet start character
const char PKT_END = '~';      // protocol packet end character

const char PKT_CMD_LAMPTEST = CMD_LAMPTEST;
const char PKT_CMD_BRIGHT = CMD_BRIGHT;
const char PKT_CMD_RESET = CMD_RESET;
const char PKT_CMD_SETUP = CMD_SETUP;
const char PKT_CMD_SELECT = CMD_SELECT;
const char PKT_CMD_VALUE = CMD_VALUE;
const char PKT_CMD_TIME = CMD_TIME;
const char PKT_CMD_DEMO = CMD_DEMO;
const char PKT_CMD_CLKFACE = CMD_CLKFACE;
const char PKT_CMD_ACK = 'Z';   // acknowledge command - data is PKT_ERR_* defines

const char PKT_ERR_OK   = '0';  // no error/ok
const char PKT_ERR_TOUT = '1';  // timeout - start detected with no end within timeout period
const char PKT_ERR_CMD  = '2';  // command field not valid or unknown
const char PKT_ERR_DATA = '3';  // data field not valid
const char PKT_ERR_SEQ  = '4';  // generic protocol sequence error

// Set up BT module initialisation parameters
// This depends on the BT module being used, as they need different AT commands.
// The AT commands are held as a long data string in PROGMEM. AT commands are
// separated by a nul character ('\0'). The last entry ends with a double nul.
// Note: The first entry must always be the BT name as this is passed as a
// parameter and is handled differently in the begin() initialisation code.
const char *szStart = "AT+";
#if HW_USE_HC05
const char *szEnd   = "\r\n";
const char PROGMEM ATCmd[] = {"NAME=\0PSWD=1234\0ROLE=0\0CLASS=800500\0RESET\0\0"};
#endif
#if HW_USE_HC06
const char *szEnd   = "\r\n";
const char PROGMEM ATCmd[] = {"NAME\0PIN1234\0\0"};
#endif
#if HW_USE_HM10_HMSOFT
const char *szEnd   = "";
const char PROGMEM ATCmd[] = {"NAME\0PIN123456\0TYPE0\0ROLE0\0RESET\0\0"};
#endif
#if HW_USE_HM10_OTHER
const char *szEnd   = "\r\n";
const char PROGMEM ATCmd[] = {"NAME\0PIN123456\0TYPE0\0ROLE0\0RESET\0\0"};
#endif

class BTSerial: public iChroniker
{
public:
  // Functions
  BTSerial(uint8_t pinRecv, uint8_t pinSend, const char* szBTName) :
    _pinRecv(pinRecv), _pinSend(pinSend), _szBTName(szBTName)
  {
    c.cmd = c.data = 0;
#if USE_ALTSOFTSERIAL
    BTChan = new AltSoftSerial();
#else
    BTChan = new SoftwareSerial(BT_RECV_PIN, BT_SEND_PIN);
#endif
  };

  ~BTSerial()
  {
    delete BTChan;
  };

  virtual void begin(void)
  // initialise the BT device class for different hardware
  {
    const uint16_t BAUD = 9600;
    char  szCmd[20], szResp[16];
    uint8_t   i = 0;
    bool  fLast = false;

    PRINT("\nStart BT connection at ", BAUD);
    BTChan->begin(BAUD);

#if HW_USE_HC05
    // Switch the HC05 to setup mode using digital I/O
    pinMode(HC05_SETUP_ENABLE, OUTPUT);
    digitalWrite(HC05_SETUP_ENABLE, HIGH);
    delay(10);   // just a small amount of time
    digitalWrite(HC05_SETUP_ENABLE, LOW);
#endif

    // Process all the AT commands for the selected BT module
    // Send each command, read the response (or time out) and then
    // do the next one.
    // First item is always the name!
    do
    {
      fLast = getATCmd(szCmd, ARRAY_SIZE(szCmd), (i == 0));

      // Print the preamble, AT command, end of line by assembling the 
      // data into a string of allocated memory. This allows the data to
      // send out in one hit rather than piecemeal.
      char *sz = (char *)malloc((strlen(szStart) + strlen_P(szCmd) + \
                  strlen(_szBTName) + strlen(szEnd) + 1) * sizeof(char));
      strcpy(sz, szStart);
      strcat(sz, szCmd);
      if (i == 0)  // first item - insert the name
        strcat(sz, _szBTName);
      strcat(sz, szEnd);
      BTChan->print(sz);
      BTChan->flush();

      free(sz);
      i++;

      // Wait for and get the response, except for the 
      // last one when we don't care as normally a RESET.
      if (!fLast)
      {
        if (!getATResponse(szResp, ARRAY_SIZE(szResp)))
        {
          PRINT("\nBT err on ", szCmd);
          PRINT(":", szResp);
        }
      }
    } while (!fLast);

    BTChan->flush();
  }

  virtual bool getCommand(void)
    // Call repeatedly to receive and process characters waiting in the serial queue
    // Return true when a good message is fully received
  {
    static enum { ST_IDLE, ST_CMD, ST_DATA, ST_END } state = ST_IDLE;
    static uint32_t timeStart = 0;
    static uint8_t countTarget, countActual;
    static char cBuf[10];
    bool b = false;

    // check for timeout if we are currently mid packet
    if (state != ST_IDLE)
    {
      if (millis() - timeStart >= BT_COMMS_TIMEOUT)
      {
        sendACK(PKT_ERR_TOUT);
        timeStart = 0;
        state = ST_IDLE;
      }
    }

    // process the next character if there is one
    if (BTChan->available())
    {
      char ch = BTChan->read();

      switch (state)
      {
      case ST_IDLE:		// waiting start character
        if (ch == PKT_START)
        {
          PRINT("\nPkt Srt ", ch);
          state = ST_CMD;
          c.cmd = c.data = 0;
          timeStart = millis();
          countActual = 0;
        }
        break;

      case ST_CMD:		// reading command
        PRINT("\nPkt Cmd ", ch);
        c.cmd = ch;
        switch (ch)
        {
        case PKT_CMD_LAMPTEST:
        case CMD_RESET:
        case CMD_SETUP:
          state = ST_END; 	// no data required
          break;

        case PKT_CMD_SELECT:
        case PKT_CMD_VALUE:
        case PKT_CMD_DEMO:
        case PKT_CMD_CLKFACE:
          countTarget = 1;
          state = ST_DATA;	// needs data
          break;

        case PKT_CMD_BRIGHT:
          countTarget = 3;
          state = ST_DATA;
          break;

        case PKT_CMD_TIME:
          countTarget = 6;
          state = ST_DATA;
          break;

        default:
          sendACK(PKT_ERR_CMD);
          state = ST_IDLE;
          break;
        }
        break;

      case ST_DATA:		// reading data
        PRINT("\nPkt cBuf[", countActual);
        cBuf[countActual++] = ch;
        PRINT("]:", cBuf[countActual-1]);

        if (countActual >= countTarget) // we have it all!
        {
          PRINT(" done @", countActual);
          state = ST_END;
          switch (c.cmd)
          {
          case PKT_CMD_SELECT:
            if (ch != CS_NEXT && ch != CS_PREV)
              sendACK(PKT_ERR_DATA);
            else
            {
              c.data = ch;
              b = true;
            }
            break;

          case PKT_CMD_VALUE:
            if (ch != CV_DOWN && ch != CV_UP)
              sendACK(PKT_ERR_DATA);
            else
            {
              b = true;
              c.data = ch;
            }
            break;

          case PKT_CMD_DEMO:
            if (ch != CD_OFF && ch != CD_CYCLE)
              sendACK(PKT_ERR_DATA);
            else
            {
              b = true;
              c.data = ch;
            }
            break;

          case PKT_CMD_CLKFACE:
            if (ch != CC_CYCLE)
              sendACK(PKT_ERR_DATA);
            else
            {
              b = true;
              c.data = ch;
            }
            break;

          case PKT_CMD_BRIGHT:
          {
            uint16_t v = 0;

            // countTarget digits 
            for (uint8_t i = 0; i < countTarget; i++)
              v = (v * 10) + (cBuf[i] - '0');

            if (v > 255) v = 255;
            c.data = v;
            b = true;
          }
          break;

          case PKT_CMD_TIME:
          {
            uint8_t v[3] = { 0 };

            // split into digits 
            for (uint8_t i = 0; i < countTarget; i += 2)
              v[i / 2] = ((cBuf[i] - '0') * 10) + (cBuf[i + 1] - '0');

            // sanity check and error or good data 
            if (v[0] > 12 || v[1] > 59 || v[2] > 59) // hours, minutes, seconds
              sendACK(PKT_ERR_DATA);
            else // pack the time into the data field
            {
              b = true;
              for (uint8_t i = 0; i < countTarget / 2; i++)
                c.data = (c.data << 8) + v[i];
            }
          }
          break;

          default:
            sendACK(PKT_ERR_CMD);
            break;
          }
          state = (b ? ST_END : ST_IDLE);
          b = false; // reset the status for return value
        }
        break;

      case ST_END:		// reading stop character
        PRINT("\nPkt End ", ch);
        b = (ch == PKT_END);
        state = ST_IDLE;
        sendACK(b ? PKT_ERR_OK : PKT_ERR_SEQ);
        break;

      default:	// something screwed up - reset the FSM
        state = ST_IDLE;
        sendACK(PKT_ERR_SEQ);
        break;
      }
    }
  
    return(b);
  }

private:
  // Serial interface parameters
  uint8_t _pinRecv, _pinSend;
  uint8_t _cmdIdx;   // char index for getting AT command from PROGMEM
  const char *_szBTName;  // BT name
#if USE_ALTSOFTSERIAL
  AltSoftSerial *BTChan;
#else
  SoftwareSerial *BTChan;
#endif

  // Functions
  bool getATCmd(char* szBuf, uint8_t lenBuf, bool fReset = true)
  // Copy the AT command from PROGMEM into the buffer provided
  // The first call should reset the index counter
  // Return true if this is the last command
  {
    if (fReset) _cmdIdx = 0;

    strncpy_P(szBuf, ATCmd+_cmdIdx, lenBuf);
    _cmdIdx += (strlen_P(ATCmd + _cmdIdx) + 1);

    return(pgm_read_byte(ATCmd + _cmdIdx) == '\0');
  }

  bool getATResponse(char* resp, uint8_t lenBuf)
  // Get an AT response from the BT module or time out waiting
  {
    const uint16_t RESP_TIMEOUT = 500;

    uint32_t timeStart = millis();
    char c = '\0';
    uint8_t len = 0;

    *resp = '\0';
    while ((millis() - timeStart < RESP_TIMEOUT) && (c != '\n') && (len < lenBuf))
    {
      if (BTChan->available())
      {
        c = BTChan->read();
        *resp++ = c;
        *resp = '\0';
        len++;
      }
    }

    return(len != '\0');
  }

  void sendACK(char resp)
  // Send a protocol ACK to the BT master
  {
    static char msg[] = { PKT_START, PKT_CMD_ACK, PKT_ERR_OK, PKT_END, '\n', '\0' };

    msg[2] = resp;
    PRINT("\nResp: ", msg);
    BTChan->print(msg);
    BTChan->flush();
  }

};
