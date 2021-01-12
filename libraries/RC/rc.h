#ifndef RC_H
#define RC_H

/*
  Simple example for receiving
  
  https://github.com/sui77/rc-switch/
*/

void rc_init(int pin = 10);
unsigned long rc_read(void);
void rc_write(unsigned long value, int nbit = 24);
unsigned long rc_value(void);
void rc_clear(void);

#endif