/*
  В скетче использованы функции из библиотеки iarduino_GSM
  https://iarduino.ru/file/345.html
*/
#include <avr/power.h>

#include "modem.h"

FLAGS flags;
volatile COMMON_OP param;

#include "i2c.h"

// Переменные, создаваемые процессом сборки,
// когда компилируется скетч
extern int __bss_end;
extern void *__brkval;

// Функция, возвращающая количество свободного ОЗУ (RAM)
int memoryFree()
{
   int freeValue;
   if((int)__brkval == 0)
      freeValue = ((int)&freeValue) - ((int)&__bss_end);
   else
      freeValue = ((int)&freeValue) - ((int)__brkval);
   return freeValue;
}

//****************обработчик прерывания********************
//  О прерываниях подробно https://arduinonsk.ru/blog/87-all-pins-interrupts

void power()
{
  flags.INTERRUPT = 1;
}

//**********************************************************

MODEM::MODEM()
{
  memset(&flags, 0, 1);
  memset(&param, 0, sizeof(COMMON_OP));
}

MODEM::~MODEM()
{
  delete phone;
  //delete param.tx;
  //delete param.rx;
}

void MODEM::phone_init()
{
  // Устанавливаем скорость связи Ардуино и модема
  uart.begin(SERIAL_RATE);

  DEBUG_PRINT(F("phone_init memoryFree=")); DEBUG_PRINTLN(memoryFree());
  DEBUG_PRINT(F("rx buf=")); DEBUG_PRINTLN(param.rx->free_space());
  DEBUG_PRINT(F("tx buf=")); DEBUG_PRINTLN(param.tx->free_space());

  if(!param.sim800_enable)
  {
    phone = new SIM800();
    phone->init();
  }
  else phone->reinit();

  DEBUG_PRINT(F("phone_init2 memoryFree=")); DEBUG_PRINTLN(memoryFree());
  
  if(!param.sim800_enable)
  {
    DEBUG_PRINTLN(F("del phone"));
    flags.RING_ENABLE = 0;
    delete phone;
   // if(!DEBUG_MODE) uart.end();
  }
  DEBUG_PRINT(F("phone_init3 memoryFree="));
  DEBUG_PRINTLN(memoryFree());
}

void MODEM::init()
{
//  memset(param.dtmf, 0x00, DTMF_BUF_SIZE);
//  param.dtmf_index = 0;
//  param.DTMF[0]    = 0;
//  param.DTMF[1]    = 0;
  param.tx  = new TEXT(161);
  param.rx  = new TEXT(161);
  //param.rx  = new TEXT(memoryFree()>1050?256:256);
  param.rx->Clear();
  param.tx->Clear();

  i2c_init();
  // Прерывание для POWER_PIN
  attachInterrupt(1, power, CHANGE);
  phone_init();
}

void MODEM::parser()
{
  // удаляем нули в начале
  //while(*param.tx->GetText() == 0) param.tx->GetChar();

  if (param.sim800_enable)
  {
    phone->parser();
  }
  else common_parser();
}

void MODEM::flags_info()
{
  FLAG_STATUS(flags.GUARD_ENABLE,  GUARD_NAME);
#if SIRENA_ENABLE
  FLAG_STATUS(flags.SIREN_ENABLE,  SIREN_NAME);
#endif
    
  if (param.sim800_enable)
  {
    FLAG_STATUS(flags.EMAIL_ENABLE,EMAIL_NAME);
    FLAG_STATUS(flags.RING_ENABLE, RING_NAME);
    FLAG_STATUS(flags.SMS_ENABLE,  SMS_NAME);
  }
  
  param.tx->AddChar('\n');
}

void MODEM::wiring() // прослушиваем телефон
{
  if (param.sim800_enable)
  {
    phone->wiring();
  }

  if(param.DTMF[0])
  {
    DEBUG_PRINT(F("DTMF[0]=")); DEBUG_PRINTLN(param.DTMF[0]);

    switch (param.DTMF[0])
    {
      case GUARD_ON:
          flags.GUARD_ENABLE = 1;
          FLAG_STATUS(flags.GUARD_ENABLE, GUARD_NAME);
          param.tx->AddChar('\n');
        break;                        
      case GUARD_OFF:
          flags.GUARD_ENABLE = 0;
          FLAG_STATUS(flags.GUARD_ENABLE, GUARD_NAME);
          param.tx->AddChar('\n');
        break;
      case GET_INFO:
          flags_info(); 
          if(flags.GUARD_ENABLE) sensors->GetInfoAll(param.tx);
          param.tx->AddChar('\n');                      
        break;
#if SIRENA_ENABLE
      case SIREN_ON_OFF:
          INVERT_FLAG(SIREN_ENABLE);
          FLAG_STATUS(flags.SIREN_ENABLE, SIREN_NAME);
          param.tx->AddChar('\n');             
        break;
#endif
      case MODEM_RESET:
          phone_init();
        break;
      case ESP_RESET:
          ESP8266_RESET;
        break;
      default:
          if (param.sim800_enable)
          {
            phone->DTMF_parser();
          }
    }
    param.DTMF[0] = 0;    
  }

  if(flags.INTERRUPT)
  {
    DELAY(10);
    flags.INTERRUPT = 0;
    param.tx->AddText_P(PSTR(SVET));
    param.tx->AddText_P(digitalRead(POWER_PIN)?PSTR(VKL):PSTR(VIKL));
  }
}
