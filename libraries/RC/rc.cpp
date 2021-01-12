#include "settings.h"
#include "rc.h"
#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

unsigned long gotValue;

void rc_init(int pin)
{
  gotValue = 0;
  mySwitch.enableReceive(0);  // Receiver on interrupt 0 => that is pin #2
  mySwitch.enableTransmit(pin); // Transmitter is connected to Arduino Pin #10 
  // Optional set protocol (default is 1, will work for most outlets)
  // mySwitch.setProtocol(2);

  // Optional set pulse length.
  // mySwitch.setPulseLength(320);
  
  // Optional set number of transmission repetitions.
  // mySwitch.setRepeatTransmit(15);
}

unsigned long rc_read(void)
{
  if (mySwitch.available()) {
    
    gotValue = mySwitch.getReceivedValue();

    DEBUG_PRINT(F("Received "));
    DEBUG_PRINT( gotValue );
    DEBUG_PRINT(F(" / "));
    DEBUG_PRINT( mySwitch.getReceivedBitlength() );
    DEBUG_PRINT(F("bit "));
    DEBUG_PRINT(F("Protocol: "));
    DEBUG_PRINT( mySwitch.getReceivedProtocol() );

    mySwitch.resetAvailable();
  }

  return gotValue;
}

void rc_write(unsigned long value, int nbit)
{
  /* Same switch as above, but using decimal code */
  mySwitch.send(value, nbit);
}

unsigned long rc_value(void)
{
  return gotValue;
}

void rc_clear(void)
{
  gotValue = 0;
}