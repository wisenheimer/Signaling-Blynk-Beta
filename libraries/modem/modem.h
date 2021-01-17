#ifndef MODEM_H
#define MODEM_H

#include "sim800.h"

#if DEBUG_MODE

int memoryFree();

#endif

class MODEM
{
  public:
    SIM800* phone;
    
    MODEM::MODEM();
    ~MODEM();
    
    void    init    ();
    void    wiring  ();
    void    parser  ();

  private:
       
    void    flags_info();
    void    phone_init();
};

#endif
