/*
  В скетче использованы функции из библиотеки iarduino_GSM
  https://iarduino.ru/file/345.html
*/
#include "sim800.h"

#if SLEEP_MODE_ENABLE
  #include <avr/sleep.h>
#endif

//****************обработчик прерывания********************
//  О прерываниях подробно https://arduinonsk.ru/blog/87-all-pins-interrupts

void call()
{

} 

#ifdef __cplusplus
extern "C"{
  ISR(PCINT2_vect)
  {

  }
}
#endif

//**********************************************************

#define GSM_TXT_CP866    0   //  Название кодировки в которой написан текст. (паремр функций TXTsendCoding() и TXTreadCoding() указывающий кодировку CP866)
#define GSM_TXT_UTF8     1   //  Название кодировки в которой написан текст. (паремр функций TXTsendCoding() и TXTreadCoding() указывающий кодировку UTF8)
#define GSM_TXT_WIN1251  2   //  Название кодировки в которой написан текст. (паремр функций TXTsendCoding() и TXTreadCoding() указывающий кодировку WIN1251)
#define GSM_SMS_CLASS_0  0   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS класса 0)
#define GSM_SMS_CLASS_1  1   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS класса 1)
#define GSM_SMS_CLASS_2  2   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS класса 2)
#define GSM_SMS_CLASS_3  3   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS класса 3)
#define GSM_SMS_CLASS_NO 4   //  Класс отправляемых SMS сообщений.           (паремр функции SMSsendClass() указывающий что отправляются SMS без класса)

#define OK   "OK"
#define ERR  "ERROR"

extern MY_SENS *sensors;
extern FLAGS flags;
extern COMMON_OP param;
//extern MY_SERIAL

volatile uint8_t answer_flags;
enum answer : uint8_t {get_pdu, get_ip, ip_ok, gprs_connect, email_end, smtpsend, smtpsend_end, admin_phone};

#define CLEAR_FLAG_ANSWER           answer_flags=0
#define GET_FLAG_ANSWER(flag)       bitRead(answer_flags,flag)
#define SET_FLAG_ANSWER_ONE(flag)   bitSet(answer_flags,flag)
#define SET_FLAG_ANSWER_ZERO(flag)  bitClear(answer_flags,flag)
#define INVERT_FLAG_ANSWER(flag)    INV_FLAG(answer_flags,1<<flag)

#define RING_BREAK uart.println(F("ATH")) // разорвать все соединения

#define MODEM_GET_STATUS  F("AT+CPAS")
  #define NOT_READY       "CPAS: 1"

// Выставляем тайминги (мс)
// время между опросами модема на предмет зависания и не отправленных смс/email
#define REGULAR_OPROS_TIME  10000

#define GPRS_GET_IP       if(!answer_flags && param.gprs_connect){uart.println(F("AT+SAPBR=2,1"));SET_FLAG_ANSWER_ONE(get_ip);vars.gprs_init_count++;}
#define GPRS_CONNECT(op)  if(!answer_flags && !param.gprs_connect){uart.print(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\";+SAPBR=3,1,\"APN\",\"internet\";+SAPBR=3,1,\"USER\",\"")); \
                          uart.print(PGM_TO_CHAR(op.user));uart.print(F("\";+SAPBR=3,1,\"PWD\",\""));uart.print(PGM_TO_CHAR(op.user));\
                          uart.println(F("\";+SAPBR=1,1"));SET_FLAG_ANSWER_ONE(gprs_connect);param.gprs_connect=true;}
#define GPRS_DISCONNECT   if(param.gprs_connect){uart.println(F("AT+SAPBR=0,1"));answer_flags=0;param.gprs_connect=false;}

// ****************************************
#define ADMIN_PHONE_SET_ZERO   memset(&admin,0,sizeof(ABONENT_CELL))

#define PGM_TO_CHAR(str)       strcpy_P(param.tx->fifo->tail,(char*)pgm_read_word(&(str))) 

#define POWER_ON digitalWrite(BOOT_PIN,LOW);DELAY(1000);digitalWrite(BOOT_PIN,HIGH)

#define GET_MODEM_ANSWER(answer,wait_ms) runAT(wait_ms,PSTR(answer))

#define ADMIN "ADMIN"

//Эти сообщения пересылаются на e-mail
const char str_0[] PROGMEM = "+CBC:";
const char str_1[] PROGMEM = "ALARM";
const char* const string[] PROGMEM = {str_0, str_1};

uint8_t op_count; // число операторов 

typedef struct PROGMEM
{
  const char* op;
  const char* user;
} OPERATORS; 

#define ADD_OP(index,name,user) const char op_##index[]    PROGMEM=name; \
                                const char user_##index[]  PROGMEM=user

// Пароли для входа в интернет
ADD_OP(0, "MTS",     "mts");
ADD_OP(1, "MEGAFON", "gdata");
ADD_OP(2, "MegaFon", "gdata");
ADD_OP(3, "Beeline", "beeline");
ADD_OP(4, "Bee Line","beeline");
// Все операторы с пустым паролем (Tele2, MOTIV, ALTEL и т.д.)
ADD_OP(5, "ANYOP", "");  

const OPERATORS op_base[] PROGMEM = {
  {op_0, user_0},
  {op_1, user_1},
  {op_2, user_2},
  {op_3, user_3},
  {op_4, user_4},
  {op_5, user_5}
};

// Условия для рестарта модема
#define ADD_RESET(index,name) const char reset_##index[] PROGMEM=name

ADD_RESET(0,NOT_READY);
ADD_RESET(1,"SIM800");
ADD_RESET(2,"NOT READY");
ADD_RESET(3,"SIM not");
ADD_RESET(4," 99");
ADD_RESET(5,"+CMS ERROR");
ADD_RESET(6,"failed");

const char* const reset[] PROGMEM = {reset_0, reset_1, reset_2, reset_3, reset_4, reset_5, reset_6};

// Условия для завершения звонка
#define ADD_BREAK(index,name) const char ath_##index[] PROGMEM=name

ADD_BREAK(0,"CONNECT");
ADD_BREAK(1,"ANSWER");
ADD_BREAK(2,"BUSY");

const char* const ath[] PROGMEM = {ath_0, ath_1, ath_2};

#if BEEP_ENABLE

  void beep()
  {
    tone(BEEP_PIN,5000);
    DELAY(1000);
    noTone(BEEP_PIN);
  }

  #define BEEP_INIT   pinMode(BEEP_PIN,OUTPUT);digitalWrite(BEEP_PIN,LOW)
  #define BEEP        beep();

#else

  #define BEEP_INIT
  #define BEEP

#endif

SIM800::SIM800()
{
  op_count = sizeof(op_base)/sizeof(OPERATORS);
  vars.gsm_operator = op_count - 1;
  
#if PDU_ENABLE
  memset(&vars, 0x00, sizeof(vars));
  vars.codTXTread = GSM_TXT_UTF8; //  Тип кодировки строки StrIn.
  vars.codTXTsend = GSM_TXT_UTF8; //  Тип кодировки строки StrIn.
  vars.clsSMSsend = GSM_SMS_CLASS_NO;
#endif
}

SIM800::~SIM800()
{

}

void SIM800::init()
{  
  //phone_num       = 0;
  //cell_num        = 0;
  param.ceng  = 0;
  ADMIN_PHONE_SET_ZERO;
  flags.RING_ENABLE = 1;
  OTCHET_INIT
  // зуммер
  BEEP_INIT;
  reinit();
}

void SIM800::reinit()
{
  param.sim800_enable = 0;
  vars.reset_count = 0;
  vars.gprs_init_count = 0;
  param.gprs_connect=0;
  CLEAR_FLAG_ANSWER;
  
  POWER_ON; // Включение GSM модуля
  for (uint8_t i = 0; i < 10; ++i)
  {
    uart.println(F("AT+IPR?"));
    if(GET_MODEM_ANSWER(OK, 1000)==R_OK) break;
  }

  param.opros_time = REGULAR_OPROS_TIME;
}

void SIM800::reinit_end()
{
  const __FlashStringHelper* cmd[] = {
      F("AT+DDET=1"),         // вкл. DTMF. 
      F("ATS0=0"),            // устанавливает количество гудков до автоответа
      F("AT+CLTS=1;&W"),      // синхронизация времени по сети
      F("ATE0"),              // выключаем эхо
      F("AT+CLIP=1"),         // Включаем АОН
#if PDU_ENABLE
      F("AT+CMGF=0"),         // PDU формат СМС
#else 
      F("AT+CMGF=1"),         // Формат СМС = ASCII текст
      F("AT+CSCS=\"GSM\""),   // Режим кодировки текста = GSM (только англ.)
#endif
      F("AT+IFC=1,1"),        // устанавливает программный контроль потоком передачи данных
      F("AT+CNMI=2,2,0,0,0"), // Текст смс выводится в com-порт
      //F("AT+CSCB=1"),         // Отключаем рассылку широковещательных сообщений        
      //F("AT+CNMI=1,1")      // команда включает отображение номера пришедшей СМСки +CMTI: «MT», <номер смски>
      F("AT+CNMI=2,2"),       // выводит сразу в сериал порт, не сохраняя в симкарте
      F("AT+CPBS=\"SM\""),    // вкл. доступ к sim карте
      F("AT+CMGD=1,4"),       // удалить все смс
      F("AT+CEXTERNTONE=0"),  // открывает микрофон
      F("ATL9"),              // громкость динамика
    //  F("AT+CLVL=?"),
    //  F("AT+CLVL?"),
    //  F("AT+CIPGSMLOC=1,1"),
    //  F("AT+CLBS=1,1"),
    //  F("AT+CIMI"),           // получаем International Mobile Subscriber Identity
    //  F("AT+CCINFO"),
      F("AT+CPBS?"),           // определить количество занятых записей и их общее число
    //  F("AT+CIPGSMLOC=1")
      F("AT+CSCLK=0")
    //  F("AT+CENG=3")
    };

    uint8_t size = sizeof(cmd)/sizeof(cmd[0]);

    for(uint8_t i = 0; i < size; i++)
    {
      uart.println(cmd[i]);
      if(!GET_MODEM_ANSWER(OK, 10000)==R_OK) return;
    }

    do { uart.println(F("AT+COPS?")); }
    while (GET_MODEM_ANSWER("+COPS: 0,0", 10000)!=R_OK);

    param.timeRegularOpros = millis();

    param.DTMF[0] = ESP_RESET;

#if TM1637_ENABLE

    timeSync = millis()+10000;

#endif      

    BEEP

    param.need_init = 0;

    //param.sleep = 1;    
}

void SIM800::ring()
{
  if(flags.RING_ENABLE)
  {
    uart.print(F("ATD>"));
    uart.print(admin.index);
    uart.println(';');
  }
}

#define DTMF_SHIFT(buf,index,len) if(index>len){for(uint8_t i=0;i<index;i++){buf[i]=buf[i+1];}index--;}

uint8_t common_parser()
{
  char *p;

  if(param.rx->filling()) DEBUG_PRINTLN(param.rx->GetText());

  if((p = READ_COM_FIND("DTMF:"))!=NULL) //+DTMF: 2
  { 
    p+=6;

    if(*p == '#')
    {
      param.DTMF[0] = atoi(param.dtmf);
      param.dtmf_index=0;
//      RING_BREAK;
    }
  
    else if(*p == '*')
    {
      param.DTMF[1] = atoi(param.dtmf);
      param.dtmf_index=0; 
    }
    else if(isdigit(*p))
    {
      DTMF_SHIFT(param.dtmf,param.dtmf_index,DTMF_BUF_SIZE-2);
      param.dtmf[param.dtmf_index++] = *p;       
    }
    param.dtmf[param.dtmf_index] = 0;

    return 1;    
  }

#if TM1637_ENABLE
  // получаем время  +CCLK: "04/01/01,03:36:04+12"
  if ((p=READ_COM_FIND("+CCLK"))!=NULL)
  {
    char ch;
    uint8_t time[3];
    p+=5;
    if(*p==':') p+=12;
    else if(*p=='=') p+=11;
    else return 0;

    for(uint8_t i = 0; i < 3; i++)
    {
      ch = *(p+2);
      *(p+2) = 0;
      time[i] = atoi(p);
      *(p+2) = ch;
      p+=3;
      DEBUG_PRINT(F("time[")); DEBUG_PRINT(i); DEBUG_PRINT(F("]=")); DEBUG_PRINTLN(time[i]);
    }

    clock_set(time);

    return 1;
  }
#endif

  return 0;
}

uint8_t SIM800::parser()
{
  char *p, *pp;
  char ch;

  if(common_parser()) return 1;

#if PDU_ENABLE

  if(GET_FLAG_ANSWER(get_pdu))
  {
    SMS_INFO sms_data;
    sms_data.txt = param.tx->GetText();

    uart.println(param.rx->GetText());
    
    PDUread(&sms_data);
    
    uart.print(F("num=")); uart.println(sms_data.num);
    uart.print(F("tim=")); uart.println(sms_data.tim);
    uart.print(F("ID="));  uart.println(sms_data.lngID);
    uart.print(F("SUM=")); uart.println(sms_data.lngSum);
    uart.print(F("NUM=")); uart.println(sms_data.lngNum);

    uart.println(sms_data.txt);

    param.rx->Clear();  
    param.rx->AddText(sms_data.txt);
    
    if(param.esp_enable || SMS_FORWARD)
    { // пересылаем входящие смс админу
      param.tx->AddText(param.rx->GetText());
      param.tx->AddChar('\n');
    }

    if (admin.phone[0])
    {
      if (strstr(admin.phone, sms_data.num))
      {
        // смс от Админа
        SET_FLAG_ANSWER_ONE(admin_phone);
      }
    }

    SET_FLAG_ANSWER_ZERO(get_pdu);     
  }
  
#endif

  if(READ_COM_FIND(OK)!=NULL)
  {
    if(GET_FLAG_ANSWER(email_end))
    {
      SET_FLAG_ANSWER_ZERO(email_end);
      uart.print(F("AT+SMTPBODY="));
      uart.println(param.tx->filling());      
    }

    if(GET_FLAG_ANSWER(smtpsend))
    {
      SET_FLAG_ANSWER_ZERO(smtpsend);
      SET_FLAG_ANSWER_ONE(smtpsend_end);
      uart.println(F("AT+SMTPSEND"));
    }

    if(GET_FLAG_ANSWER(get_ip))
    {
      SET_FLAG_ANSWER_ZERO(get_ip);
    }

    if(GET_FLAG_ANSWER(ip_ok))
    {
      SET_FLAG_ANSWER_ZERO(ip_ok);
      email();
    }

    if(GET_FLAG_ANSWER(gprs_connect))
    {
      SET_FLAG_ANSWER_ZERO(gprs_connect);
      GPRS_GET_IP
    }
  }

  // требуется загрузка текста смс для отправки
  if(READ_COM_FIND(">")!=NULL)
  {
#if (PDU_ENABLE==0)

    param.tx->filling() <= 160 ? vars.SmsLen = param.tx->filling() : vars.SmsLen = 160;
    
    for(uint16_t i = 0; i < vars.SmsLen; i++)
    {
      uart.print(param.tx->GetByte(i));
    }      
    uart.write(26);
#endif

    return 1;        
  }

  // +CENG: 0,"250,01,0a8f,25ea,56,37
  // +CENG: 1,",,0000,0000,00,00"
  // +CENG: 1,",,0000,ffff,56,34"
  if((p = READ_COM_FIND("+CENG:"))!=NULL)
  {
    if(READ_COM_FIND("0000")!=NULL)
    {
      param.tx->Clear();
      param.sleep = 0;
      return 1;
    }
    
    if(READ_COM_FIND("+CENG: 2,\"")!=NULL)
    {
      param.sleep = 1;
    }        

    p+=10;
    if(READ_COM_FIND(": 0,\"")!=NULL || READ_COM_FIND(": 1,\"")!=NULL || READ_COM_FIND(": 2,\"")!=NULL)
        param.tx->AddText(p);

    return 1;    
  }


  // +CPBS: "SM",18,100
/*  if((p = READ_COM_FIND("CPBS:"))!=NULL)
  {
    p+=11;
    pp=p;
    while(*p!=',' && *p) p++;
    *p=0;
    phone_num = atoi(pp);
    cell_num = atoi(p+1);

    return 1;    
  }
*/

  // +CPBF: 20,"+79xxxxxxxxx",145,"ADMIN"
  // +CPBR: 1,"+380xxxxxxxxx",129,"Ivanov"
  // +CPBR: 20,"+79xxxxxxxxx",145,"USER"
  // Запоминаем индекс ячейки телефонной книги
  if((p=READ_COM_FIND("CPBR: "))!=NULL || (p=READ_COM_FIND("CPBF: "))!=NULL)
  {
    p+=6; pp = p;
    if(flags.EMAIL_ENABLE || param.esp_enable)
    {
      param.tx->AddText(p);
      if(millis() - param.timeRegularOpros > param.opros_time - 1000)
      { // оставляем время для получения всех записей с симкарты до отправки e-mail
        param.timeRegularOpros = millis() - param.opros_time + 1000;
      }       
    }
    while(*p!=',' && *p) p++;
    *p = 0;
    last_abonent.index = atoi(pp);
    p+=2; pp = p;
    get_name(get_number_and_type(p));

    if(strstr_P(pp,PSTR(ADMIN))!=NULL)
    {
      admin = last_abonent;
    }
    return 1;
  }

  if(READ_COM_FIND("+IPR: 0")!=NULL)
  {
    uart.print(F("AT+IPR="));
    uart.print(SERIAL_RATE);
    uart.println(F(";&W"));
    return 1;
  }  

  // RING
  //+CLIP: "+79xxxxxxxxx",145,"",0,"USER",0
  //+CLIP: "+79xxxxxxxxx",145,"",0,"",0
  if((p=READ_COM_FIND("CLIP:"))!=NULL)
  {
    if(flags.EMAIL_ENABLE || param.esp_enable) param.tx->AddText(p);
    memset(&last_abonent, 0, sizeof(ABONENT_CELL));

    get_name(get_number_and_type(p+7)+5);
    if(last_abonent.name[2]) // номер зарегистрирован
    {
      if(READ_COM_FIND_RAM(admin.phone)==NULL && admin.phone[0]) // не админ
      {
        RING_BREAK;
        if(flags.GUARD_ENABLE) param.DTMF[0] = GUARD_OFF;
        else param.DTMF[0] = GUARD_ON;
      }
      else uart.println(F("ATA"));          
    }
    else
    {
      RING_BREAK;
      if(admin.index == 0)
      {
        /* Добавление текущего номера в телефонную книгу */
        strcpy_P(last_abonent.name,PSTR(ADMIN));    
        uart.print(F("AT+CPBW=,\""));
        uart.print(last_abonent.phone);
        uart.print(F("\",145,"));
        uart.println(last_abonent.name);
        uart.println(F("AT+CPBF"));       
      }
    } 
    
    DEBUG_PRINT(F("admin.index=")); DEBUG_PRINTLN(admin.index);
    DEBUG_PRINT(F("admin.phone=")); DEBUG_PRINTLN(admin.phone);
    DEBUG_PRINT(F("last_abonent.phone=")); DEBUG_PRINTLN(last_abonent.phone);

    param.sleep = 0;
    //param.ceng  = 1;

    //uart.println(F("AT+CREG=2"));
    //DELAY(1000);
    //uart.println(F("AT+CREG?"));

    return 1;
  }

  if(READ_COM_FIND("COPS: 0,0")!=NULL)
  {
    vars.gsm_operator = 0;
    while (vars.gsm_operator < op_count-1 && READ_COM_FIND_P(op_base[vars.gsm_operator].op)==NULL) vars.gsm_operator++;
    return 1;
  }

#if PDU_ENABLE
  
  if ((p=READ_COM_FIND("+CPMS:"))!=NULL) 
  { //  Получаем позицию начала текста "+CPMS:" в ответе +CPMS: "ПАМЯТЬ1",ИСПОЛЬЗОВАНО,ОБЪЁМ, "ПАМЯТЬ2",ИСПОЛЬЗОВАНО,ОБЪЁМ, "ПАМЯТЬ3",ИСПОЛЬЗОВАНО,ОБЪЁМ\r\n
    uint8_t i = 0;
    while(*p && i<7)
    {
      if(*p==',') i++; //  Получаем позицию параметра ИСПОЛЬЗОВАНО, он находится через 7 запятых после текста "+CPMS:".
      p++;
    }

    SMSsum = _num(*p); p++; //  Получаем первую цифру в найденной позиции, это первая цифра количества использованной памяти.
    if( *p!=','  ){SMSsum*=10; SMSsum+= _num(*p); p++;}  //  Если за первой цифрой не стоит знак запятой, значит там вторая цифра числа, учитываем и её.
    uart.print(F("SMSsum="));
    uart.println(SMSsum);

    //  Получаем позицию параметра ОБЪЁМ, он находится через 8 запятых после текста "+CPMS:".
    if( *p!=',' )
    {
      p++;
      maxSMS = _num(*p); p++;                         //  Получаем первую цифру в найденной позиции, это первая цифра доступной памяти.
      if( *p!='\r'  ){maxSMS*=10; maxSMS+= _num(*p);} //  Если за первой цифрой не стоит знак \r, значит там вторая цифра числа, учитываем и её.
      uart.print(F("maxSMS="));
      uart.println(maxSMS);
    }
    
    return 1;
  }

  if ((p=READ_COM_FIND("+CMGR:"))!=NULL) // +CMGR: СТАТУС,["НАЗВАНИЕ"],РАЗМЕР\r\nPDU\r\n\r\nOK\r\n".
  {       
    if((p = READ_COM_FIND_RAM(","))!=NULL) //  Получаем позицию первой запятой следующей за текстом "+CMGR:", перед этой запятой стоит цифра статуса.
    {             
      if (*(p-1) != '0') { SMSavailable(); return;}  //  Если параметр СТАТУС не равен 0, значит это не входящее непрочитанное SMS сообщение. Обращаемся к функции SMSavailable(), чтоб удалить все сообщения, кроме входящих непрочитанных.
      SET_FLAG_ANSWER_ONE(get_pdu);
    }
    return 1;
  }

  if ((p=READ_COM_FIND("+CMT:"))!=NULL) // получили номер смс в строке вида +CMT: "",140
  {
    SET_FLAG_ANSWER_ONE(get_pdu);      
    return 1;
  }

#endif

  if ((p=READ_COM_FIND("CMGS:"))!=NULL) // +CMGS: <mr> - индекс отправленного сообщения 
  { // смс успешно отправлено
    // удаляем все отправленные сообщения
    uart.println(F("AT+CMGD=1,3"));

#if PDU_ENABLE

    vars.lngNum++;
    if(vars.lngSum < vars.lngNum)
    {
      vars.lngSum = 0;
      vars.lngNum = 0;
    }

#endif
    while(vars.SmsLen && param.tx->filling())
    {
      ch=param.tx->GetChar();
      vars.SmsLen--;
    }

    return 1;
  }
  
  if ((p=READ_COM_FIND("CMTI:"))!=NULL) // получили номер смс в строке вида +CMTI: "SM",0
  {       
    p+=11;
    // получили номер смс в строке вида +CMTI: "SM",0
    // чтение смс из памяти модуля
    uart.print(F("AT+CMGR=")); uart.println(p);
    return 1;
  }

  // +CMGL: 1,"REC UNREAD","679","","18/10/22,19:16:57+12"
  if ((p=READ_COM_FIND("CMGL:"))!=NULL) // получили номер смс в строке
  {       
    // находим индекс смс в памяти модема
    p+=6;
   
    uart.print(F("AT+CMGR="));
    while(*p!=',')
    {  // чтение смс из памяти модуля
       uart.print(*p++);
    }
    uart.println();
    return 1;    
  }
/*
  if ((p=READ_COM_FIND("+CIMI:"))!=NULL)
  {       
    uart.println(p);
    return 1;
  }
*/
/*
  if ((p=READ_COM_FIND("+CREG:"))!=NULL)
  {       
    param.tx->AddText(p);
    uart.println(F("AT+CREG=0"));
    return 1;
  }
*/

  if (admin.phone[0])
  {
    if (READ_COM_FIND_RAM(admin.phone))
    {
      // звонок или смс от Админа
      SET_FLAG_ANSWER_ONE(admin_phone);
      return 1;
    }      
  }

  if(READ_COM_FIND("DIALTON")!=NULL)
  {
    ring();
    return 1;    
  }

  for(uint8_t i = 0; i < 7; i++)
  {
    if(READ_COM_FIND_P(reset[i])!=NULL)
    {
      param.DTMF[0]=MODEM_RESET;
      return 1;
    }
  }

  for(uint8_t i = 0; i < 3; i++)
  {
    if(READ_COM_FIND_P(ath[i])!=NULL)
    {
      RING_BREAK;
      param.dtmf_index = 0;
      //param.dtmf_str_index = 0;
      return 1;
    }
  }

  // ALARM RING
  // +CALV: 2

  for(uint8_t i = 0; i < 2; i++)
  {
    if((p=READ_COM_FIND_P(string[i]))!=NULL)
    {
      param.tx->AddText(p);
      return 1;
    }
  }

  // +CUSD: 0, "Balance:40,60r", 15
  // +CUSD: 0, "041D0435043F0440043", 72
  if ((p=READ_COM_FIND("+CUSD:"))!=NULL)
  { // Пришло уведомление о USSD-ответе
    char* pos;
//  Разбираем ответ:  
//  Получаем  позицию начала текста "+CUSD:" в ответе "+CUSD: СТАТУС, "ТЕКСТ" ,КОДИРОВКА\r\n".
    while(*p!='\"' && *p) p++;
    if(*p!='\"') return 0;
    p++;  pos = p;              //  Сохраняем позицию первого символа текста ответа на USSD запрос.
    while(*p!='\"' && *p) p++;
    if(*p!='\"') return 0;
#if PDU_ENABLE
    uint16_t len;    //  Переменные для хранения позиции начала и длины информационного блока в ответе (текст ответа).
    uint8_t coder;   //  Переменные для хранения статуса USSD запроса и кодировки USSD ответа.
    char* _txt = param.tx->fifo->buffer + param.tx->fifo->tail; 

    len = p-pos; len/=2;        //  Сохраняем количество байтов в тексте ответа на USSD запрос.
    while(*p!=',' && *p) p++;
    if(*p!=',') return 0;
    p++;
    while(*p==' ' && *p) p++;
    coder = _num(*p); p++;      //  Сохраняем значение первой цифры кодировки текста.
    if( *p!='\r'  ){coder*=10; coder+= _num(*p); p++;}  //  Сохраняем значение второй цифры кодировки текста (если таковая существует).
    if( *p!='\r'  ){coder*=10; coder+= _num(*p); p++;}  //  Сохраняем значение третей цифры кодировки текста (если таковая существует).
//  Разкодируем ответ в строку _txt:     //
    if(coder==72) {_SMSdecodUCS2(_txt, len, pos-param.rx->GetText());}else  //  Разкодируем ответ в строку _txt указав len - количество байт     и pos - позицию начала закодированного текста.
    if(coder==15) {memcpy(_txt, pos, len<<1);_txt[len<<1]=0;}else
                  // {_SMSdecodGSM (buf, (len/7*8),pos-param.rx->GetText());}else    //  Разкодируем ответ в строку _txt указав len - количество символов и pos - позицию начала закодированного текста.
                  {_SMSdecod8BIT(_txt, len, pos-param.rx->GetText());}      //  Разкодируем ответ в строку _txt указав len - количество байт     и pos - позицию начала закодированного текста.

    DEBUG_PRINTLN(_txt);
    len=strlen(_txt);
    param.tx->fifo->tail = (param.tx->fifo->tail + len) % param.tx->fifo->size;
    param.tx->fifo->filling += len;
    ((char*)param.tx->fifo->buffer)[param.tx->fifo->tail] = 0;
#else
    *p=0;
    param.tx->AddText(pos);
#endif
    param.tx->AddChar('\n');

    return 1;    
  }

  ////////////////////////////////////////////////////////////
  /// Для GPRS
  ////////////////////////////////////////////////////////////
  // требуется загрузка текста письма для отправки
  if(READ_COM_FIND("DOWNLOAD")!=NULL)
  {
    while(param.tx->filling())
    {
      uart.print(param.tx->GetChar());
    }
    uart.write(26);
    // Ждём OK от модема,
    // после чего даём команду на отправку письма
    SET_FLAG_ANSWER_ONE(smtpsend);
    
    return 1;        
  }

  if((p = READ_COM_FIND("+SMTPSEND:"))!=NULL)
  {
    SET_FLAG_ANSWER_ZERO(smtpsend_end); 
    // письмо успешно отправлено
    if((p = READ_COM_FIND(": 1"))!=NULL)
    {
      vars.gprs_init_count = 0;
      param.tx->Clear();
    }

    return 1;    
  }

  if((p = READ_COM_FIND("+SAPBR"))!=NULL)
  {
    if((p = READ_COM_FIND(": 1,1"))!=NULL)
    {
      SET_FLAG_ANSWER_ZERO(get_ip);
      SET_FLAG_ANSWER_ONE(ip_ok);
    }
    else param.gprs_connect=0;

    return 1;
  }

  // получаем смс
  // Выполнение любой AT+ команды
  if ((p=READ_COM_FIND("AT+"))!=NULL || (p=READ_COM_FIND("at+"))!=NULL)
  {
    //if (GET_FLAG_ANSWER(admin_phone))
    {
      uart.println(p);
      SET_FLAG_ANSWER_ZERO(admin_phone);
    }   
    
    return 1;      
  }

  // Выполнение любой AT+ команды
  if ((p=READ_COM_FIND("Охрана"))!=NULL || (p=READ_COM_FIND("охрана"))!=NULL)
  {
    //if (GET_FLAG_ANSWER(admin_phone))
    {
      p+=13;
      if(*p=='0') param.DTMF[0] = GUARD_OFF;
      if(*p=='1') param.DTMF[0] = GUARD_ON;
      SET_FLAG_ANSWER_ZERO(admin_phone);
    }   
    
    return 1;      
  }

  // Выполнение любой AT+ команды
  if ((p=READ_COM_FIND("Guard"))!=NULL || (p=READ_COM_FIND("guard"))!=NULL)
  {
    //if (GET_FLAG_ANSWER(admin_phone))
    {
      p+=6;
      if(*p=='0') param.DTMF[0] = GUARD_OFF;
      if(*p=='1') param.DTMF[0] = GUARD_ON;
      SET_FLAG_ANSWER_ZERO(admin_phone);
    }   
    
    return 1;      
  }

  // Получаем смс вида "pin НОМЕР_ПИНА on/off".
  // Например, "pin 14 on" и "pin 14 off"
  // задают пину 14 (А0) значения HIGH или LOW соответственно.
  // Поддерживаются пины от 5 до 19, где 14 соответствует А0, 15 соответствует А1 и т.д
  // Внимание. Пины A6 и A7 не поддерживаются, т.к. они не работают как цифровые выходы.
  if ((p=READ_COM_FIND("pin "))!=NULL)
  {
    //if (GET_FLAG_ANSWER(admin_phone))
    {
      p+=4;
      uint8_t buf[3];
      uint8_t i = 0;
      uint8_t pin;
      uint8_t level;
      while(*p>='0' && *p<='9' && i < 2)
      {
        buf[i++] = *p++;
      }
      buf[i] = 0;
      pin = atoi(buf);

      if(pin < 5 || pin > A5) return;
      
      p++;
      
      if(*p == 'o' && *(p+1) == 'n') level = HIGH;
      else if(*p == 'o' && *(p+1) == 'f' && *(p+2) == 'f') level = LOW;
      else return;      
  
      pinMode(pin, OUTPUT); // устанавливаем пин как выход
      digitalWrite(pin, level);
      // ответное смс
      param.tx->AddText_P(PSTR("PIN "));
      param.tx->AddInt((int)pin);
      param.tx->AddText_P(PSTR(" is "));
      param.tx->AddChar(digitalRead(pin)+48);
      param.tx->AddChar('\n');
      DEBUG_PRINTLN(param.tx->GetText()); // отладочное собщение

      SET_FLAG_ANSWER_ZERO(admin_phone);
    }   
    
    return 1;      
  }

  return 0;
}


void SIM800::email()
{
  if(param.tx->filling())
  {
    uint8_t i, size;

    const __FlashStringHelper* cmd[] = 
    {
      F("AT+EMAILCID=1;+EMAILTO=30;+SMTPSRV="), SMTP_SERVER,
      F(";+SMTPAUTH=1,"), SMTP_USER_NAME_AND_PASSWORD,
      F(";+SMTPFROM="), SENDER_ADDRESS_AND_NAME,
      F(";+SMTPSUB=\"Info\""),
      F(";+SMTPRCPT=0,0,"), RCPT_ADDRESS_AND_NAME
#ifdef RCPT_CC_ADDRESS_AND_NAME
      ,F(";+SMTPRCPT=1,0,"), RCPT_CC_ADDRESS_AND_NAME // копия
#ifdef RCPT_BB_ADDRESS_AND_NAME
      ,F(";+SMTPRCPT=2,0,"), RCPT_BB_ADDRESS_AND_NAME // вторая копия
#endif
#endif
    };

    size = sizeof(cmd)/sizeof(cmd[0]);
  
    for(i = 0; i < size; i++)
    {
      uart.print(cmd[i]);
      uart.flush();
    } 

    uart.println();

    SET_FLAG_ANSWER_ONE(email_end);
  }
}

char* SIM800::get_number_and_type(char* p)
{
  uint8_t i = 0;

  while((*p!='\"' || *p=='+') && i < NLENGTH-1) last_abonent.phone[i++] = *p++;
        
  last_abonent.phone[i] = 0;

  return p+6;
}

char* SIM800::get_name(char* p)
{
  uint8_t i = 0;
  uint8_t count = 2;

  memset(last_abonent.name, 0, TLENGTH);

  while(count && *p)
  {
    if(*p == '\"') count--;
    last_abonent.name[i++] = *p++;
  }

  return p;
}

uint8_t SIM800::read_com(const char* answer)
{
  char inChar;
  uint8_t ret = R_NULL;
  int i = 0;

  while (uart.available()) {
    // получаем новый байт:  
    inChar = (char)uart.read();
    //DEBUG_PRINT(inChar);
    // добавляем его:
    param.rx->AddChar(inChar);

    if(inChar=='\n' || inChar == '>')
    {
      //uart.print(F("receive: ")); uart.print(param.rx->GetText());

      if(!param.sim800_enable)
      {
        param.sim800_enable = 1;
        param.need_init = 1;
      }
      vars.reset_count = 0;
      param.timeRegularOpros = millis();
      parser();
      if(answer!=NULL)
      {
        if(strstr_P(param.rx->GetText(),answer)!=NULL)
        {
          ret = R_OK;
        }
      }
      if(READ_COM_FIND(ERR)!=NULL)
      {
        ret = R_ERR;
      }
      param.rx->Clear();
    }
    DELAY(10);
  }

  if(!param.rx->free_space()) param.rx->Clear();

  return ret;
}

#define DELETE_PHONE(index)    {uart.print(F("AT+CPBW="));uart.println(index);}
//#define FLAG_STATUS(flag,name) flag_status(flag,PSTR(name))

void SIM800::wiring() // прослушиваем телефон
{
  uint8_t tmp;

  read_com(NULL);

#if TM1637_ENABLE
  // синхронизация времени часов
  if(!GET_FLAG_ANSWER(email_end))
  {
    if(millis() - timeSync > SYNC_TIME_PERIOD)
    {
      uart.println(F("AT+CCLK?"));
      timeSync = millis();
    }
  }
#endif

  // Опрос модема
  if(millis() - param.timeRegularOpros > param.opros_time)
  {
    SET_FLAG_ANSWER_ZERO(admin_phone);

    if(vars.reset_count > 3) param.DTMF[0]=MODEM_RESET;

    if(vars.gprs_init_count > 3)
    {
      // если gprs не подключается, переходим на смс
      if(flags.SMS_ENABLE)
      {
        vars.gprs_init_count = 0;
        flags.EMAIL_ENABLE = 0;
        GPRS_DISCONNECT
      } 
      else param.DTMF[0]=MODEM_RESET;
    }

    if(param.need_init==0 && !admin.phone[0]) param.DTMF[0]=EMAIL_ADMIN_PHONE;

    if(param.need_init) reinit_end();
    else
    {
      // Есть данные для отправки
      if(param.tx->filling() && param.esp_enable == 0)
      {      
        if(flags.EMAIL_ENABLE)
        {
          GPRS_CONNECT(op_base[vars.gsm_operator])
          else GPRS_GET_IP                             
        }
        else // Отправка SMS
        if(flags.SMS_ENABLE)
        {
          if(admin.phone[0])
          {
#if PDU_ENABLE

            if(param.tx->fifo->tail > param.tx->fifo->head)
            {
              param.tx->filling() <= 70 ? vars.SmsLen = param.tx->filling() : vars.SmsLen = 70;

              char* txt = (char*)param.tx->fifo->buffer+param.tx->fifo->head;
              char* p = txt+vars.SmsLen;
              char ch = *p;
              *p = 0;
              SMSsend(txt,admin.phone);
              *p = ch;
            }
            else
            {
              char txt[70];
              vars.SmsLen = param.tx->GetBytes(txt,70,1);
              txt[vars.SmsLen] = 0;

              SMSsend(txt,admin.phone);
            }
//            SendSMS();
#else
            // AT+CMGS=\"+79xxxxxxxxx\"
            uart.print(F("AT+CMGS=\""));uart.print(admin.phone);uart.println('\"');
#endif
          }            
        }
        else param.tx->Clear();            
      }
      else if(param.need_init==0)
      {        
        // проверяем непрочитанные смс и уровень сигнала
        //uart.println(F("AT+CMGL=\"REC UNREAD\",1;+CSQ"));
        //uart.println(F("AT+CREG?"));
#if SLEEP_MODE_ENABLE
  
        //if(!digitalRead(POWER_PIN) && !flags.ALARM)
        if(admin.phone[0] && param.sleep)
        {
          // Маскируем прерывания
          PCMSK2 = 0b11110000;
          // разрешаем прерывания
          PCICR = bit(2);
          // Прерывание для RING
          attachInterrupt(0, call, LOW); //Если на 0-вом прерываниии - ноль, то просыпаемся.
      
#if TM1637_ENABLE
          digitalWrite(CLK, LOW);
          digitalWrite(DIO, LOW);         
#endif

          //uart.println(F("AT+CSCLK=2")); // Режим энергосбережения

          param.ceng = 0;

          do
          {
            uart.println(F("AT+CSCLK=2")); // Вкл. режим энергосбережения      
          }
          while(!GET_MODEM_ANSWER(OK,1000));
   
          set_sleep_mode(SLEEP_MODE_PWR_DOWN);
          sleep_mode(); // Переводим МК в спящий режим

          // запрещаем прерывания
          PCICR = 0;
          detachInterrupt(0);

          do
          {
            param.rx->Clear();
            uart.println(F("AT+CSCLK=0")); // Выкл. режим энергосбережения
            DELAY(500);
            uart.println(F("AT+CSCLK?"));
          }
          while(!GET_MODEM_ANSWER("CSCLK: 0",1000));

          //  uart.println(F("WakeUp"));

          if(flags.EMAIL_ENABLE || param.esp_enable) param.tx->AddText_P(PSTR("WakeUp:"));
        }
#endif

        if(param.ceng) uart.println(F("AT+CENG?"));
        else uart.println(F("AT+CSQ"));
        vars.reset_count++;
      }
    }

    param.timeRegularOpros = millis();       
  }

  if(flags.INTERRUPT)
  {
    flags.INTERRUPT;
    param.DTMF[0] = GET_INFO;
  }  
}

void SIM800::DTMF_parser()
{
  if(param.DTMF[1])
  {
    uart.print(F("AT+CUSD=1,\"*")); uart.print(param.DTMF[1]);
    uart.print('*');uart.print(param.DTMF[0]); uart.println(F("#\""));
    param.DTMF[1] = 0;
  }
  else    
  switch (param.DTMF[0])
  {
    case EMAIL_ON_OFF:
        INVERT_FLAG(EMAIL_ENABLE);
        FLAG_STATUS(flags.EMAIL_ENABLE, EMAIL_NAME);
        param.tx->AddChar('\n');
      break;
    case TEL_ON_OFF:
        INVERT_FLAG(RING_ENABLE);
        FLAG_STATUS(flags.RING_ENABLE, RING_NAME);
        param.tx->AddChar('\n');  
      break;      
    case SMS_ON_OFF:
        INVERT_FLAG(SMS_ENABLE);
        FLAG_STATUS(flags.SMS_ENABLE, SMS_NAME);
        param.tx->AddChar('\n');             
      break;
    case BAT_CHARGE:
        uart.println(F("AT+CBC"));
      break;
    case EMAIL_ADMIN_PHONE: // получение номера админа на email
        uart.println(F("AT+CPBF=\"ADMIN\""));  
      break;
    case ADMIN_NUMBER_DEL:
        DELETE_PHONE(admin.index);
        ADMIN_PHONE_SET_ZERO;
      break;   
    default:
        uart.print(F("AT+CUSD=1,\""));
#if PDU_ENABLE
        uart.print('*');
#else
        uart.print('#');
#endif
        uart.print(param.DTMF[0]); uart.println(F("#\""));  
  }
}

//    ФУНКЦИЯ ВЫПОЛНЕНИЯ AT-КОМАНД:                                                         //  Функция возвращает: строку с ответом модуля.
uint8_t SIM800::runAT(uint16_t wait_ms, const char* answer)
{
  uint8_t res;
  uint32_t time = millis() + wait_ms;

  do
  {
    res = read_com(answer);
  }
  while(millis() < time && res==R_NULL);

  //if(answer && res==R_NULL) uart.println(F("modem not answer"));

  return res;
}

#if PDU_ENABLE

bool SIM800::SendSMS()
{
  char txt[71];
  uint16_t size = param.tx->filling();
  uint8_t res;
  
  if(!size) return 0;

  do
  {
    if(vars.lngSum == 0)
    { // Первая отправка
      vars.lngSum = 1;

      if(size > 70) // если более 1 смс
      {
        vars.lngSum = size/66;
        if (size%66) vars.lngSum++;
      }
    }

    if(vars.lngNum == 0) vars.lngNum=1;

    DEBUG_PRINT(F("Sum=")); DEBUG_PRINT(vars.lngSum); DEBUG_PRINT(F(" Num=")); DEBUG_PRINTLN(vars.lngNum);

    vars.lngSum == 1 ? size=70 : size=66;
    vars.SmsLen = param.tx->GetBytes(txt,size,1);
    txt[vars.SmsLen] = 0;

    do
    {
      if(vars.lngSum == 1)
      {    
        res = SMSsend(txt, admin.phone);
      }
      else
      {
        res = SMSsend(txt, admin.phone, 0x1234, vars.lngSum, vars.lngNum);
      }
    }while(res!=R_OK);
  }while(vars.lngSum);

  return 1;
}

//    ФУНКЦИЯ ПРЕОБРАЗОВАНИЯ СИМВОЛА В ЧИСЛО:                                           //  Функция возвращает: число uint8_t.
uint8_t SIM800::_num(char symbol){                                                       //  Аргументы функции:  символ 0-9,a-f,A-F.
  uint8_t i = uint8_t(symbol);                                                          //  Получаем код символа
  if ( (i>=0x30) && (i<=0x39) ) { return i-0x30; }else                                  //  0-9
  if ( (i>=0x41) && (i<=0x46) ) { return i-0x37; }else                                  //  A-F
  if ( (i>=0x61) && (i<=0x66) ) { return i-0x57; }else                                  //  a-f
  { return      0; }                                                                    //  остальные символы вернут число 0.
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ ПРЕОБРАЗОВАНИЯ ЧИСЛА В СИМВОЛ:                                            //  Функция возвращает: символ char.
char  SIM800::_char(uint8_t num){                                                        //  Аргументы функции:  число 0-15.
  if(num<10){return char(num+0x30);}else                                                //  0-9
  if(num<16){return char(num+0x37);}else                                                //  A-F
  {return '0';}                                                                         //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ ПОЛУЧЕНИЯ КОЛИЧЕСТВА СИМВОЛОВ В СТРОКЕ С УЧЁТОМ КОДИРОВКИ:                //  Функция возвращает: число uint16_t соответствующее количеству символов (а не байт) в строке.
uint16_t SIM800::_SMStxtLen(const char* txt){                                            //  Аргументы функции:  txt - строка с текстом
  uint16_t numIn=0, sumSymb=0;                                                          //  Объявляем временные переменные.
  uint16_t lenIn=strlen(txt);                                                           //
  uint8_t  valIn=0;                                                                     //
  switch(vars.codTXTsend){                                                                   //  Тип кодировки строки txt.
  //  Получаем количество символов в строке txt при кодировке UTF-8:                    //
  case GSM_TXT_UTF8:                                                                    //
    while(numIn<lenIn){                                                                 //  Пока номер очередного читаемого байта не станет больше объявленного количества байтов.
      valIn=uint8_t(txt[numIn]); sumSymb++;                                             //
      if(valIn<0x80){numIn+=1;}else                                                     //  Символ состоит из 1 байта
      if(valIn<0xE0){numIn+=2;}else                                                     //  Символ состоит из 2 байт
      if(valIn<0xF0){numIn+=3;}else                                                     //  Символ состоит из 3 байт
      if(valIn<0xF8){numIn+=4;}else                                                     //  Символ состоит из 4 байт
      {numIn+=5;}                                                                       //  Символ состоит из 5 и более байт
    }                                                                                   //
    break;                                                                              //
    //  Получаем количество символов в строке txt при кодировке CP866:                  //
    case GSM_TXT_CP866:   sumSymb=lenIn; break;                                         //
    //  Получаем количество символов в строке txt при кодировке Windows1251:            //
    case GSM_TXT_WIN1251: sumSymb=lenIn; break;                                         //
  } return sumSymb;                                                                     //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ КОДИРОВАНИЯ ТЕКСТА SMS СООБЩЕНИЯ В ФОРМАТЕ GSM:                           //  Функция возвращает: число uint16_t соответствующее позиции после последнего закодированного символа из строки txt.
uint16_t SIM800::_SMScoderGSM(const char* txt, uint16_t pos, uint16_t len){              //  Аргументы функции:  txt - строка с текстом, pos - позиция взятия первого символа из строки txt, len - количество кодируемых символов из строки txt.
  uint8_t  valByteIn = 0;                                                               //  Определяем временную переменную для хранения значения очередного читаемого символа из строки txt.
  uint16_t numByteIn = 0;                                                               //  Определяем временную переменную для хранения номера   очередного читаемого символа из строки txt.
  uint8_t  numBitIn = 0;                                                                //  Определяем временную переменную для хранения номера   очередного бита в байте valByteIn.
  uint8_t  valByteOut = 0;                                                              //  Определяем временную переменную для хранения значения очередного сохраняемого байта.
  uint8_t  numBitOut = 0;                                                               //  Определяем временную переменную для хранения номера   очередного бита в байте valByteOut.
                                                                                        //
  if(len==255){len=strlen(txt)-pos;}                                                    //
  while( numByteIn < len ){                                                             //  Пока номер очередного читаемого символа меньше заданного количества читаемых символов.
    valByteIn = txt[pos+numByteIn]; numByteIn+=1;                                       //  Читаем значение символа с номером numByteIn в переменную valByteIn.
    for(numBitIn=0; numBitIn<7; numBitIn++){                                            //  Проходим по битам байта numByteIn (номер бита хранится в numBitIn).
      bitWrite( valByteOut, numBitOut, bitRead(valByteIn,numBitIn) ); numBitOut++;      //  Копируем бит из позиции numBitIn байта numByteIn (значение valByteIn) в позицию numBitOut символа numByteOut, увеличивая значение numBitOut после каждого копирования.
      if(numBitOut>7){                                                                  //  
        uart.print(_char(valByteOut/16));                                             //  
        uart.print(_char(valByteOut%16));                                             //  
        valByteOut=0; numBitOut=0;                                                      //  
      }                                                                                 //  При достижении numBitOut позиции старшего бита в символе (numBitOut>=7), обнуляем старший бит символа (txt[numByteOut]&=0x7F), обнуляем позицию бита в символе (numBitOut=0), переходим к следующему символу (numByteOut++).
    }                                                                                   //  
  }                                                                                     //  
  if(numBitOut){                                                                        //  
    uart.print(_char(valByteOut/16));                                                 //  
    uart.print(_char(valByteOut%16));                                                 //  
  }                                                                                     //  
  return pos+numByteIn;                                                                 //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ GSM ТЕКСТА SMS СООБЩЕНИЯ ИЗ СТРОКИ                         //
void  SIM800::_SMSdecodGSM(char* txt, uint16_t len, uint16_t pos, uint16_t udh_len)      //
{ //  Аргументы функции:  txt - строка для текста, len - количество символов в тексте, pos - позиция начала текста в строке, udh_len количество байт занимаемое заголовком.
  if(udh_len>0){ len -= udh_len*8/7; if(udh_len*8%7){len--;} }                          //  Если текст начинается с заголовка, то уменьшаем количество символов в тексте на размер заголовка.
  uint8_t  valByteIn  = 0;                                                              //  Определяем временную переменную для хранения значения очередного читаемого байта из строки.
  uint16_t numByteIn  = udh_len*2;                                                      //  Определяем временную переменную для хранения номера   очередного читаемого байта из строки.
  uint8_t  numBitIn   = udh_len==0?0:(7-(udh_len*8%7));                                 //  Определяем временную переменную для хранения номера   очередного бита в байте numByteIn.
  uint16_t numByteOut = 0;                                                              //  Определяем временную переменную для хранения номера   очередного раскодируемого символа для строки txt.
  uint8_t  numBitOut  = 0;                                                              //  Определяем временную переменную для хранения номера   очередного бита в байте numByteOut.
                                                                                        //
  while(numByteOut<len){                                                                //  Пока номер очередного раскодируемого символа не станет больше объявленного количества символов.
    valByteIn = _num(*(param.rx->GetText()+pos+numByteIn))*16 +                             //  Читаем значение байта с номером numByteIn в переменную valByteIn.
                _num(*(param.rx->GetText()+pos+numByteIn+1)); numByteIn+=2;                 //
    while(numBitIn<8){                                                                  //  Проходим по битам байта numByteIn (номер бита хранится в numBitIn).
      bitWrite( txt[numByteOut], numBitOut, bitRead(valByteIn,numBitIn) ); numBitOut++; //  Копируем бит из позиции numBitIn байта numByteIn (значение valByteIn) в позицию numBitOut символа numByteOut, увеличивая значение numBitOut после каждого копирования.
      if(numBitOut>=7){ txt[numByteOut]&=0x7F; numBitOut=0; numByteOut++;}              //  При достижении numBitOut позиции старшего бита в символе (numBitOut>=7), обнуляем старший бит символа (txt[numByteOut]&=0x7F), обнуляем позицию бита в символе (numBitOut=0), переходим к следующему символу (numByteOut++).
      numBitIn++;                                                                       //  
    } numBitIn=0;                                                                       //  
  }                                                                                     //
  txt[numByteOut]=0;                                                                    //  Присваиваем символу len+1 значение 0 (конец строки).
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ 8BIT ТЕКСТА SMS СООБЩЕНИЯ:                                 //
void  SIM800::_SMSdecod8BIT(char* txt, uint16_t len, uint16_t pos){                      //  Аргументы функции:  txt - строка для текста, len - количество байт в тексте, pos - позиция начала текста в строке.
  uint16_t numByteIn  = 0;                                                              //  Определяем временную переменную для хранения номера   очередного читаемого байта из строки.
  uint16_t numByteOut = 0;                                                              //  Определяем временную переменную для хранения номера очередного раскодируемого символа для строки txt.
                                                                                        //  
  while(numByteOut<len){                                                                //  Пока номер очередного раскодируемого символа не станет больше объявленного количества символов.
    txt[numByteOut]= _num(*(param.rx->GetText()+pos+numByteIn))*16 + _num(*(param.rx->GetText()+pos+numByteIn+1)); numByteIn+=2; numByteOut++;      //  Читаем значение байта с номером numByteIn в символ строки txt под номером numByteOut.
  } txt[numByteOut]=0;                                                                  //  Присваиваем символу len+1 значение 0 (конец строки).
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ КОДИРОВАНИЯ ТЕКСТА SMS СООБЩЕНИЯ В ФОРМАТЕ UCS2:                          //  Функция возвращает: число uint16_t соответствующее позиции после последнего закодированного символа из строки txt.
uint16_t SIM800::_SMScoderUCS2(const char* txt, uint16_t pos, uint16_t len)              //  Аргументы функции:  txt - строка с текстом, pos - позиция взятия первого символа из строки txt, len - количество кодируемых символов из строки txt.
{                                                                                       //
  uint8_t  valByteInThis  = 0;                                                          //  Определяем временную переменную для хранения значения очередного читаемого байта.
  uint8_t  valByteInNext  = 0;                                                          //  Определяем временную переменную для хранения значения следующего читаемого байта.
  uint16_t numByteIn      = pos;                                                        //  Определяем временную переменную для хранения номера очередного   читаемого байта.
  uint16_t numSymbIn      = 0;                                                          //  Определяем временную переменную для хранения номера очередного   читаемого символа.
  uint8_t  lenTXT         = strlen(txt);                                                //  Определяем временную переменную для хранения длины строки в байтах.
  switch(vars.codTXTsend){                                                                   //  Тип кодировки строки StrIn.
    //        Преобразуем текст из кодировки UTF-8 кодировку UCS2: (и записываем его в строку в формате текстового HEX)             //
    case GSM_TXT_UTF8:                                                                  //
      while(numSymbIn<len && numByteIn<lenTXT)                                          //
      { //  Пока номер очередного читаемого символа не станет больше объявленного количества кодируемых символов или не превысит длину строки.
        valByteInThis = uint8_t(txt[numByteIn  ]);                                      //  Читаем значение очередного байта.
        valByteInNext = uint8_t(txt[numByteIn+1]);                                      //  Читаем значение следующего байта.
        numSymbIn++;                                                                    //  Увеличиваем количество прочитанных символов.
        if (valByteInThis==0x00) { return numByteIn; }                                  //  Очередной прочитанный символ является символом конца строки, возвращаем номер прочитанного байта.
        else if (valByteInThis < 0x80)                                                  //
        {                                                                               //
          numByteIn+=1;  uart.print(F("00"));                                         //  Очередной прочитанный символ состоит из 1 байта и является символом латинского алфавита, записываем 00 и его значение.
          uart.print(_char(valByteInThis/16));                                        //  записываем его старший полубайт.
          uart.print(_char(valByteInThis%16));                                        //  записываем его младший полубайт.
        }                                                                               //
        else if (valByteInThis==0xD0)                                                   //
        {                                                                               //
          numByteIn+=2; uart.print(F("04"));                                          //  Очередной прочитанный символ состоит из 2 байт и является символом Русского алфавита, записываем 04 и проверяем значение байта valByteInNext ...
          if (valByteInNext==0x81) { uart.print(F("01")); }                           //  Символ  'Ё' - 208 129 => 04 01
          if((valByteInNext>=0x90)&&(valByteInNext<=0xBF))                              //
          {                                                                             //
            uart.print(_char((valByteInNext-0x80)/16));                               //  Символы 'А-Я,а-п' - 208 144 - 208 191  =>  04 16 - 04 63
            uart.print(_char((valByteInNext-0x80)%16));                               //
          }                                                                             //  
        }                                                                               //
        else if (valByteInThis==0xD1)                                                   //
        {                                                                               //
          numByteIn+=2; uart.print(F("04"));                                          //  Очередной прочитанный символ состоит из 2 байт и является символом Русского алфавита, записываем 04 и проверяем значение байта valByteInNext ...
          if (valByteInNext==0x91) { uart.print(F("51")); }                           //  Символ 'ё' - 209 145 => 04 81
          if((valByteInNext>=0x80)&&(valByteInNext<=0x8F))                              //
          {                                                                             //
            uart.print(_char((valByteInNext-0x40)/16));                               //  Символы 'р-я' - 209 128 - 209 143 => 04 64 - 04 79
            uart.print(_char((valByteInNext-0x40)%16));                               //
          }                                                                             //  
        }                                                                               //
        else if (valByteInThis <0xE0) { numByteIn+=2; uart.print(F("043F")); }        //  Очередной прочитанный символ состоит из 2 байт, записываем его как символ '?'
        else if (valByteInThis <0xF0) { numByteIn+=3; uart.print(F("003F")); }        //  Очередной прочитанный символ состоит из 3 байт, записываем его как символ '?'
        else if (valByteInThis <0xF8) { numByteIn+=4; uart.print(F("003F")); }        //  Очередной прочитанный символ состоит из 4 байт, записываем его как символ '?'
        else { numByteIn+=5; uart.print(F("003F")); }                                 //  Очередной прочитанный символ состоит из 5 и более байт, записываем его как символ '?'
      }                                                                                 //
      break;                                                                            //
                                                                                        //
//        Преобразуем текст из кодировки CP866 в кодировку UCS2: (и записываем его в строку в формате текстового HEX)           //  
    case GSM_TXT_CP866:                                                                 //
      while(numSymbIn<len && numByteIn<lenTXT)                                          //
      {                                                                                 //  Пока номер очередного читаемого символа не станет больше объявленного количества кодируемых символов или не превысит длину строки.
        valByteInThis = uint8_t(txt[numByteIn]);                                        //  Читаем значение очередного символа.
        numSymbIn++; numByteIn++;                                                       //  Увеличиваем количество прочитанных символов и номер прочитанного байта.
        if (valByteInThis==0x00) { return numByteIn; }                                  //  Очередной прочитанный символ является символом конца строки, возвращаем номер прочитанного байта.
        else if (valByteInThis <0x80)                                                   //
        {                                                                               //
          uart.print(F("00"));                                                        //  Очередной прочитанный символ состоит из 1 байта и является символом латинского алфавита, записываем 00 и его значение.
          uart.print(_char(valByteInThis/16));                                        //  записываем его старший полубайт.
          uart.print(_char(valByteInThis%16));                                        //  записываем его младший полубайт.
        }                                                                               //
        else if (valByteInThis==0xF0) { uart.print(F("0401")); }                      //  Символ 'Ё' - 240 => 04 01 записываем его байты.
        else if (valByteInThis==0xF1) { uart.print(F("0451")); }                      //  Символ 'e' - 241 => 04 81 записываем его байты.
        else if((valByteInThis>=0x80)&&(valByteInThis<=0xAF))                           //
        {                                                                               //
          uart.print(F("04")); valByteInThis-=0x70;                                   //  Символы 'А-Я,а-п' - 128 - 175 => 04 16 - 04 63 записываем 04 и вычисляем значение для кодировки UCS2:
          uart.print(_char(valByteInThis/16));                                        //  записываем старший полубайт.
          uart.print(_char(valByteInThis%16));                                        //  записываем младший полубайт.
        }                                                                               //
        else if((valByteInThis>=0xE0)&&(valByteInThis<=0xEF))                           //
        {                                                                               //
          uart.print(F("04")); valByteInThis-=0xA0;                                   //  Символы 'р-я'     - 224 - 239      =>  04 64 - 04 79                 записываем 04 и вычисляем значение для кодировки UCS2:
          uart.print(_char(valByteInThis/16));                                        //  записываем старший полубайт.
          uart.print(_char(valByteInThis%16));                                        //  записываем младший полубайт.
        }                                                                               //
      }                                                                                 //
      break;                                                                            //
//        Преобразуем текст из кодировки Windows1251 в кодировку UCS2: (и записываем его в формате текстового HEX)         //  
    case GSM_TXT_WIN1251:                                                               //
      while(numSymbIn<len && numByteIn<lenTXT)                                          //
      {                                                                                 //  Пока номер очередного читаемого символа не станет больше объявленного количества кодируемых символов или не превысит длину строки.
        valByteInThis = uint8_t(txt[numByteIn]);                                        //  Читаем значение очередного символа.
        numSymbIn++; numByteIn++;                                                       //  Увеличиваем количество прочитанных символов и номер прочитанного байта.
        if (valByteInThis==0x00) { return numByteIn; }                                  //  Очередной прочитанный символ является символом конца строки, возвращаем номер прочитанного байта.
        else if (valByteInThis <0x80)                                                   //
        {                                                                               //
          uart.print(F("00"));                                                        //  Очередной прочитанный символ состоит из 1 байта и является символом латинского алфавита, записываем 00 и его значение.
          uart.print(_char(valByteInThis/16));                                        //  записываем его старший полубайт.
          uart.print(_char(valByteInThis%16));                                        //  записываем его младший полубайт.
        }                                                                               //
        else if (valByteInThis==0xA8) { uart.print(F("0401")); }                      //  Символ 'Ё' - 168 => 04 01 записываем его байты.
        else if (valByteInThis==0xB8) { uart.print(F("0451")); }                      //  Символ 'e' - 184 => 04 81 записываем его байты.
        else if((valByteInThis>=0xC0)&&(valByteInThis<=0xEF))                           //
        {                                                                               //
          uart.print(F("04")); valByteInThis-=0xB0;                                   //  Символы 'А-Я,а-п' - 192 - 239 => 04 16 - 04 63 записываем 04 и вычисляем значение для кодировки UCS2:
          uart.print(_char(valByteInThis/16));                                        //  записываем старший полубайт.
          uart.print(_char(valByteInThis%16));                                        //  записываем младший полубайт.
        }                                                                               //
        else if((valByteInThis>=0xF0)&&(valByteInThis<=0xFF))                           //
        {                                                                               //
          uart.print(F("04")); valByteInThis-=0xB0;                                   //  Символы 'р-я' - 240 - 255 => 04 64 - 04 79 записываем 04 и вычисляем значение для кодировки UCS2:
          uart.print(_char(valByteInThis/16));                                        //  записываем старший полубайт.
          uart.print(_char(valByteInThis%16));                                        //  записываем младший полубайт.
        }                                                                               //
      }                                                                                 //
      break;                                                                            //
  }                                                                                     //  
  return numByteIn;                                                                     //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ UCS2 ТЕКСТА SMS СООБЩЕНИЯ ИЗ строки                        //
void  SIM800::_SMSdecodUCS2(char* txt, uint16_t len, uint16_t pos){                      //  Аргументы функции:  txt - строка для текста, len - количество байт в тексте, pos - позиция начала текста в строке.
  uint8_t  byteThis = 0;                                                                //  Определяем временную переменную для хранения значения очередного читаемого байта.
  uint8_t  byteNext = 0;                                                                //  Определяем временную переменную для хранения значения следующего читаемого байта.
  uint16_t numIn    = 0;                                                                //  Определяем временную переменную для хранения номера   очередного читаемого байта.
  uint16_t numOut   = 0;                                                                //  Определяем временную переменную для хранения номера   очередного раскодируемого символа.
                                                                                        // 
  len<<=1;                                                                               //  Один байт данных занимает 2 символа в строке.
  switch(vars.codTXTread){                                                                   //  Тип кодировки строки StrIn.
//        Преобразуем текст из кодировки UCS2 в кодировку UTF-8:                        //
    case GSM_TXT_UTF8:                                                                  //
      while(numIn<len){                                                                 //  Пока номер очередного читаемого байта не станет больше объявленного количества байтов.
        byteThis = _num(*(param.rx->GetText()+pos+numIn))*16 +                              //
                   _num(*(param.rx->GetText()+pos+numIn+1)); numIn+=2;                      //  Читаем значение очередного байта в переменную byteThis.
        byteNext = _num(*(param.rx->GetText()+pos+numIn))*16 +                              //
                   _num(*(param.rx->GetText()+pos+numIn+1)); numIn+=2;                      //  Читаем значение следующего байта в переменную byteNext.
        if(byteThis==0x00){                            txt[numOut]=byteNext;      numOut++;}else //  Символы латинницы
        if(byteNext==0x01){txt[numOut]=0xD0; numOut++; txt[numOut]=byteNext+0x80; numOut++;}else //  Символ  'Ё'       - 04 01         => 208 129
        if(byteNext==0x51){txt[numOut]=0xD1; numOut++; txt[numOut]=byteNext+0x40; numOut++;}else //  Символ  'ё'       - 04 81         => 209 145
        if(byteNext< 0x40){txt[numOut]=0xD0; numOut++; txt[numOut]=byteNext+0x80; numOut++;}else //  Символы 'А-Я,а-п' - 04 16 - 04 63 => 208 144 - 208 191
                          {txt[numOut]=0xD1; numOut++; txt[numOut]=byteNext+0x40; numOut++;}     //  Символы 'р-я'     - 04 64 - 04 79 => 209 128 - 209 143
      }                                                                                 //
      txt[numOut]=0;                                                                    //
      break;                                                                            //
//        Преобразуем текст из кодировки UCS2 в кодировку CP866:                        //
    case GSM_TXT_CP866:                                                                 //
      while(numIn<len){                                                                 //  Пока номер очередного читаемого байта не станет больше объявленного количества байтов.
        byteThis = _num(*(param.rx->GetText()+pos+numIn))*16 + _num(*(param.rx->GetText()+pos+numIn+1)); numIn+=2; //  Читаем значение очередного байта в переменную byteThis.
        byteNext = _num(*(param.rx->GetText()+pos+numIn))*16 + _num(*(param.rx->GetText()+pos+numIn+1)); numIn+=2; //  Читаем значение следующего байта в переменную byteNext.
        if(byteThis==0x00){txt[numOut]=byteNext;      numOut++;}else                    //  Символы латинницы
        if(byteNext==0x01){txt[numOut]=byteNext+0xEF; numOut++;}else                    //  Символ  'Ё'       - 04 01          =>  240
        if(byteNext==0x51){txt[numOut]=byteNext+0xA0; numOut++;}else                    //  Символ  'ё'       - 04 81          =>  141
        if(byteNext< 0x40){txt[numOut]=byteNext+0x70; numOut++;}else                    //  Символы 'А-Я,а-п' - 04 16 - 04 63  =>  128 - 175
                          {txt[numOut]=byteNext+0xA0; numOut++;}                        //  Символы 'р-я'     - 04 64 - 04 79  =>  224 - 239
      }                                                                                 //
      txt[numOut]=0;                                                                    //
      break;                                                                            //
//        Преобразуем текст из кодировки UCS2 в кодировку Windows1251:                  //
    case GSM_TXT_WIN1251:                                                               //
      while(numIn<len){                                                                 //  Пока номер очередного читаемого байта не станет больше объявленного количества байтов.
        byteThis = _num(*(param.rx->GetText()+pos+numIn))*16 + _num(*(param.rx->GetText()+pos+numIn+1)); numIn+=2; //  Читаем значение очередного байта в переменную byteThis.
        byteNext = _num(*(param.rx->GetText()+pos+numIn))*16 + _num(*(param.rx->GetText()+pos+numIn+1)); numIn+=2; //  Читаем значение следующего байта в переменную byteNext.
        if(byteThis==0x00){txt[numOut]=byteNext;      numOut++;}else                    //  Символы латинницы
        if(byteNext==0x01){txt[numOut]=byteNext+0xA7; numOut++;}else                    //  Символ  'Ё'       - 04 01          =>  168
        if(byteNext==0x51){txt[numOut]=byteNext+0x67; numOut++;}else                    //  Символ  'ё'       - 04 81          =>  184
        if(byteNext< 0x40){txt[numOut]=byteNext+0xB0; numOut++;}else                    //  Символы 'А-Я,а-п' - 04 16 - 04 63  =>  192 - 239
                          {txt[numOut]=byteNext+0xB0; numOut++;}                        //  Символы 'р-я'     - 04 64 - 04 79  =>  240 - 255
      }                                                                                 //
      txt[numOut]=0;                                                                    //
      break;                                                                            //
  }                                                                                     //
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ КОДИРОВАНИЯ АДРЕСА SMS СООБЩЕНИЯ В СТРОКУ:                                //
void  SIM800::_SMScoderAddr(const char* num){                                            //  Аргументы функции:  num - строка с адресом для кодирования.
  uint16_t j=num[0]=='+'?1:0, len=strlen(num);                                          //  Определяем временные переменные.
                                                                                        //
  for(uint16_t i=j; i<len; i+=2){                                                       //  
    if( (len<=(i+1)) || (num[i+1]==0) ){uart.print('F');}else{uart.print(num[i+1]);}//  Отправляем следующий символ из строки num, если символа в строке num нет, то отправляем символ 'F'.
    if( (len<= i   ) || (num[i  ]==0) ){uart.print('F');}else{uart.print(num[i  ]);}//  Отправляем текущий   символ из строки num, если символа в строке num нет, то отправляем символ 'F'.
  }                                                                                     //
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ АДРЕСА SMS СООБЩЕНИЯ ИЗ СТРОКИ:                            //
void  SIM800::_SMSdecodAddr(char* num, uint16_t len, uint16_t pos){                      //  Аргументы функции:  num - строка для получения адреса.
  uint8_t j=0;                                                                          //            len - количество полубайт в адресе (количество символов в номере).
                                                                                        //
  for(uint16_t i=0; i<len; i+=2){                                                       //            pos - позиция адреса в строке.
    if( *(param.rx->GetText()+pos+i+1)!='F' && *(param.rx->GetText()+pos+i+1)!='f'){num[i]=*(param.rx->GetText()+pos+i+1); j++;} //  Сохраняем следующий символ из строки на место текущего в строке num, если этот символ не 'F' или 'f'.
    if( *(param.rx->GetText()+pos+i  )!='F' && *(param.rx->GetText()+pos+i  )!='f'){num[i+1]=*(param.rx->GetText()+pos+i); j++;} //  Сохраняем текущий   символ из строки на место следующего в строке num, если этот символ не 'F' или 'f'.
  } num[j]=0;                                                                           //  
}                                                                                       //  
                                                                                        //  
//    ФУНКЦИЯ РАЗКОДИРОВАНИЯ ДАТЫ ОТПРАВКИ SMS СООБЩЕНИЯ ИЗ СТРОКИ:                     //
void  SIM800::_SMSdecodDate(char* tim, uint16_t pos){                                    //  Аргументы функции:  tim - строка для даты
  tim[ 0]=*(param.rx->GetText()+pos+5);                                                     //  ст. день.     pos - позиция даты в строке
  tim[ 1]=*(param.rx->GetText()+pos+4);                                                     //  мл. день.
  tim[ 2]='.';                                                                          //  
  tim[ 3]=*(param.rx->GetText()+pos+3);                                                     //  ст. мес.
  tim[ 4]=*(param.rx->GetText()+pos+2);                                                     //  мл. мес.
  tim[ 5]='.';                                                                          //  
  tim[ 6]=*(param.rx->GetText()+pos+1);                                                     //  ст. год.
  tim[ 7]=*(param.rx->GetText()+pos+0);                                                     //  мл. год.
  tim[ 8]=' ';                                                                          //  
  tim[ 9]=*(param.rx->GetText()+pos+7);                                                     //  ст. час.
  tim[10]=*(param.rx->GetText()+pos+6);                                                     //  мл. час.
  tim[11]=':';                                                                          //  
  tim[12]=*(param.rx->GetText()+pos+9);                                                     //  ст. мин.
  tim[13]=*(param.rx->GetText()+pos+8);                                                     //  мл. мин.
  tim[14]=':';                                                                          //  
  tim[15]=*(param.rx->GetText()+pos+11);                                                    //  ст. сек.
  tim[16]=*(param.rx->GetText()+pos+10);                                                    //  мл. сек.
  tim[17]=0;                                                                            //  конец строки.
}                                                                                       //  
                                                                                        //  
/*                                                                                      //
  ФУНКЦИЯ ЧТЕНИЯ ВХОДЯЩЕГО SMS СООБЩЕНИЯ:                                               //
  Функция возвращает: ничего                                                            //
*/                                                                                      //
void  SIM800::PDUread(SMS_INFO* sms){ //  Аргументы функции:  SMS_INFO).                 //
//      Готовим переменные:                                                             //  
  uint8_t  i              = 0;                                                          //
  bool   f                = 0;                                                          //  Флаг указывающий на успешное чтение сообщения из памяти в строку.
  uint8_t  PDU_SCA_LEN    = 0;                                                          //  Первый байт блока SCA, указывает количество оставшихся байт в блоке SCA.
  uint8_t  PDU_SCA_TYPE   = 0;                                                          //  Второй байт блока SCA, указывает формат адреса сервисного центра службы коротких сообщений.
  uint8_t  PDU_SCA_DATA   = 0;                                                          //  Позиция первого байта блока SCA, содержащего адрес сервисного центра службы коротких сообщений.
  uint8_t  PDU_TYPE       = 0;                                                          //  Первый байт блока TPDU, указывает тип PDU, состоит из флагов RP UDHI SRR VPF RD MTI (их назначение совпадает с первым байтом команды AT+CSMP).
  uint8_t  PDU_OA_LEN     = 0;                                                          //  Первый байт блока OA, указывает количество полезных (информационных) полубайтов в блоке OA.
  uint8_t  PDU_OA_TYPE    = 0;                                                          //  Второй байт блока OA, указывает формат адреса отправителя сообщения.
  uint8_t  PDU_OA_DATA    = 0;                                                          //  Позиция третьего байта блока OA, это первый байт данных содержащих адрес отправителя сообщения.
  uint8_t  PDU_PID        = 0;                                                          //  Первый байт после блока OA, указывает на идентификатор (тип) используемого протокола.
  uint8_t  PDU_DCS        = 0;                                                          //  Второй байт после блока OA, указывает на схему кодирования данных (кодировка текста сообщения).
  uint8_t  PDU_SCTS_DATA  = 0;                                                          //  Позиция первого байта блока SCTS, содержащего дату и время отправки сообщения.
  uint8_t  PDU_UD_LEN     = 0;                                                          //  Первый байт блока UD (следует за блоком SCTS), указывает на количество символов (7-битной кодировки) или количество байт (остальные типы кодировок) в блоке UD.
  uint8_t  PDU_UD_DATA    = 0;                                                          //  Позиция второго байта блока UD, с данного байта начинается текст SMS или блок заголовка (UDH), зависит от флага UDHI в байте PDUT (PDU_TYPE).
  uint8_t  PDU_UDH_LEN    = 0;                                                          //  Первый байт блока UDH, указывает количество оставшихся байт в блоке UDH. (блок UDH отсутствует если сброшен флаг UDHI в байте PDU_TYPE).
  uint8_t  PDU_UDH_IEI    = 0;                                                          //  Второй байт блока UDH является первым байтом блока IEI, указывает на назначение заголовка. Для составных SMS значение IEI равно 0x00 или 0x08. Если IEI равно 0x00, то блок IED1 занимает 1 байт, иначе IED1 занимает 2 байта.
  uint8_t  PDU_UDH_IED_LEN= 0;                                                          //  Первый байт после блока IEI, указывает на количество байт в блоке IED состояшем из IED1,IED2,IED3. Значение данного байта не учитывается в данной библиотеке.
  uint16_t PDU_UDH_IED1   = 0;                                                          //  Данные следуют за байтом IEDL (размер зависит от значения PDU_IEI). Для составных SMS значение IED1 определяет идентификатор длинного сообщения (все SMS в составе длинного сообщения должны иметь одинаковый идентификатор).
  uint8_t  PDU_UDH_IED2   = 1;                                                          //  Предпоследний байт блока UDH. Для составных SMS его значение определяет количество SMS в составе длинного сообщения.
  uint8_t  PDU_UDH_IED3   = 1;                                                          //  Последний байт блока UDH. Для составных SMS его значение определяет номер данной SMS в составе длинного сообщения.

  sms->txt[0]=0; sms->num[0]=0; sms->tim[0]=0; sms->lngID=0; sms->lngSum=1; sms->lngNum=1;  //  Чистим данные по ссылкам и указателям для ответа.

//      Определяем значения из PDU блоков SMS сообщения находящегося в строке text:                                      //  
//      |                                                  PDU                                                   |       //  
//      +------------------+-------------------------------------------------------------------------------------+       //  
//      |                  |                                        TPDU                                         |       //  
//      |                  +-----------------------------------------+-------------------------------------------+       //  
//      |        SCA       |                                         |                    UD                     |       //  
//      |                  |      +---------------+                  |     +--------------------------------+    |       //  
//      |                  |      |      OA       |                  |     |             [UDH]              |    |       //  
//      +------------------+------+---------------+-----+-----+------+-----+--------------------------------+----+       //  
//      | SCAL [SCAT SCAD] | PDUT | OAL [OAT OAD] | PID | DCS | SCTS | UDL | [UDHL IEI IEDL IED1 IED2 IED3] | UD |       //  
//                                                                              //  
//  SCAL (Service Center Address Length) - байт указывающий размер адреса сервисного центра коротких сообщений.          //  
//  PDU_SCA_LEN   = _num(param.rx->GetChar())*16 + _num(param.rx->GetChar()); //  Получаем значение  первого байта блока SCA (оно указывает на количество оставшихся байт в блоке SCA).
//  SCAT (Service Center Address Type) - байт хранящий тип адреса сервисного центра коротких сообщений.                  //  
//  PDU_SCA_TYPE  = _num(param.rx->GetChar())*16 + _num(param.rx->GetChar()); //  Получаем значение  второго байта блока SCA (тип адреса: номер, текст, ... ).
//  SCAD (Service Center Address Date) - блок адреса сервисного центра коротких сообщений. С третьего байта начинается сам адрес.       //  
//  SCAL (Service Center Address Length) - байт указывающий размер адреса сервисного центра коротких сообщений.          //  
  PDU_SCA_LEN   = _num(*(param.rx->GetText()))*16 + _num(*(param.rx->GetText()+1));             //  Получаем значение  первого байта блока SCA (оно указывает на количество оставшихся байт в блоке SCA).
  i = 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
  //  SCAT (Service Center Address Type) - байт хранящий тип адреса сервисного центра коротких сообщений.                //  
  PDU_SCA_TYPE  = _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1));         //  Получаем значение  второго байта блока SCA (тип адреса: номер, текст, ... ).
  i+= 2;                                                                                //  
  PDU_SCA_DATA  = i;                                                                    //  Сохраняем позицию  третьего байта блока SCA (для получения адреса в дальнейшем).
  i+= PDU_SCA_LEN*2 - 2;                                                                //  Смещаем курсор на PDU_SCA_LEN байт после байта PDU_SCA_LEN.
//  PDUT (Packet Data Unit Type) - байт состоящий из флагов определяющих тип блока PDU. //  
  PDU_TYPE    = _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1));           //  Получаем значение  байта PDU_TYPE (оно состоит из флагов RP UDHI SRR VPF RD MTI).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  OAL (Originator Address Length) - байт указывающий размер адреса отправителя.       //  
  PDU_OA_LEN    = _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1));         //  Получаем значение  первого байта блока OA (оно указывает на количество полезных полубайтов в блоке OA).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  OAT (Originator Address Type) - байт хранящий тип адреса отправителя.               //  
  PDU_OA_TYPE   = _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1));         //  Получаем значение  второго байта блока OA (тип адреса: номер, текст, ... ).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  OAD (Originator Address Date) - адрес отправителя. С третьего байта начинается сам адрес, его размер равен PDU_OA_LEN полубайтов.     //  
  PDU_OA_DATA   = i;                                                                    //  Сохраняем позицию  третьего байта блока OA (для получения адреса в дальнейшем).
  i+= (PDU_OA_LEN + (PDU_OA_LEN%2));                                                    //  Смещаем курсор на значение PDU_OA_LEN увеличенное до ближайшего чётного.
//  PID (Protocol Identifier) - идентификатор протокола передачи данных, по умолчанию байт равен 00.                     //  
  PDU_PID     = _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1));           //  Получаем значение  байта PID (идентификатор протокола передачи данных).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  DCS (Data Coding Scheme) - схема кодирования данных (кодировка текста сообщения).   //  
  PDU_DCS     = _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1));           //  Получаем значение  байта DCS (схема кодирования данных).
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  SCTS (Service Center Time Stam) - отметка о времени получения сообщения в сервис центр коротких сообщений.           //  
  PDU_SCTS_DATA = i;                                                                    //  Сохраняем позицию  первого байта блока SCTS (для получения даты и времени в дальнейшем).
  i+= 14;                                                                               //  Смещаем курсор на 14 полубайт (7 байт).
//  UDL (User Data Length) - размер данных пользователя.                                //  
  PDU_UD_LEN    = _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1));         //  Получаем значение  байта UDL (размер данных пользователя). Для 7-битной кодировки - количество символов, для остальных - количество байт.
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  UD (User Data) - данные пользователя (заголовок и текст SMS сообщения).             //  
  PDU_UD_DATA   = i;                                                                    //  Позиция первого байта блока UD (данные). Блок UD может начинаться с блока UDH (заголовок), если установлен флаг UDHI в байте PDU_TYPE, а уже за ним будет следовать данные текста SMS.
//  UDHL (User Data Header Length) - длина заголовока в данных пользователя.            //  
  PDU_UDH_LEN   = (PDU_TYPE & 0b01000000)? _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1)) : 0; //  Получаем значение  первого байта блока UDH (оно указывает на количество оставшихся байт в блоке UDH). Блок UDH отсутствует если сброшен флаг UDHI в байте PDU_TYPE, иначе блок UD начинается с блока UDH.
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  IEI (Information Element Identifier) - идентификатор информационного элемента.      //  
  PDU_UDH_IEI   = (PDU_UDH_LEN)? _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1)) : 0; //  Получаем значение  первого байта блока IEI (блок указывает на назначение заголовка). Для составных SMS блок IEI состоит из 1 байта, а его значение равно 0x00 или 0x08. Если IEI равно 0x00, то блок IED1 занимает 1 байт, иначе IED1 занимает 2 байта.
  i+= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт).
//  IEDL (Information Element Data Length) - длина данных информационных элементов.     //  
  PDU_UDH_IED_LEN = (PDU_UDH_LEN)? _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1)) : 0; //  Получаем значение  первого байта после блока IEI (оно указывает на количество байт в блоке IED состояшем из IED1,IED2,IED3). Значение данного байта не учитывается в данной библиотеке.
  i+= PDU_UDH_LEN*2 - 4;                                                                //  Смещаем курсор к последнему байту блока UDH.
//  IED3 (Information Element Data 3) - номер SMS в составе составного длинного сообщения.  //  
  PDU_UDH_IED3  = (PDU_UDH_LEN)? _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1)) : 0; //  Получаем значение  последнего байта блока UDH. (оно указывает на номер SMS в составе составного длинного сообщения).
  i-= 2;                                                                                //  Смещаем курсор на 2 полубайта (1 байт) к началу.
//  IED2 (Information Element Data 2) - количество SMS в составе составного длинного сообщения. //  
  PDU_UDH_IED2  = (PDU_UDH_LEN)? _num(*(param.rx->GetText()+i))*16 + _num(*(param.rx->GetText()+i+1)) : 0; //  Получаем значение  предпоследнего байта блока UDH. (оно указывает на количество SMS в составе составного длинного сообщения).
  i-= 2; if(PDU_UDH_IEI){i-= 2;}                                                        //  Смещаем курсор на 2 или 4 полубайта (1 или 2 байта) к началу.
//  IED1 (Information Element Data 1) - идентификатор длинного сообщения.               //  
  PDU_UDH_IED1  = (PDU_UDH_IEI)? _num(*(param.rx->GetText()+i))*4096 + _num(*(param.rx->GetText()+i+1))*256 + _num(*(param.rx->GetText()+i+2))*16 + _num(*(param.rx->GetText()+i+3)) :
  _num(*(param.rx->GetText()+i))*16   + _num(*(param.rx->GetText()+i+1));                       //  Получаем значение  идентификатора длинного сообщения (все SMS в составе длинного сообщения должны иметь одинаковый идентификатор).
//      Выполняем дополнительные преобразования значений блоков для удобства дальнейшей работы: //  
//  Вычисляем схему кодирования данных (текста SMS сообщения):                          //  
  if((PDU_DCS&0xF0)==0xC0){PDU_DCS=0;}else if((PDU_DCS&0xF0)==0xD0){PDU_DCS=0;}else if((PDU_DCS&0xF0)==0xE0){PDU_DCS=2;}else{PDU_DCS=(PDU_DCS&0x0C)>>2;}  //  PDU_DCS = 0-GSM, 1-8бит, 2-UCS2.
//  Вычисляем тип адреса отправителя:                                                   //  
  if((PDU_OA_TYPE-(PDU_OA_TYPE%16))==0xD0){PDU_OA_TYPE=1;}else{PDU_OA_TYPE=0;}          //  PDU_OA_TYPE = 0 - адресом отправителя является номер телефона, 1 - адрес отправителя указан в алфавитноцифровом формате.
//  Вычисляем длину адреса отправителя:                                                 //  
  if(PDU_OA_TYPE){PDU_OA_LEN=(PDU_OA_LEN/2)+(PDU_OA_LEN/14);}                           //  PDU_OA_LEN = количество символов для адреса отправителя в алфавитноцифровом формате, количество цифр для адреса указанного в виде номера телефона отправителя.
//  Вычисляем длину блока UDH вместе с его первым байтом:                               //  
  if(PDU_UDH_LEN>0){PDU_UDH_LEN++;}                                                     //  PDU_UDH_LEN теперь указывает не количество оставшихся байт в блоке UDH, а размер всего блока UDH (добавили 1 байт занимаемый самим байтом PDU_UDH_LEN).
//      Расшифровка SMS сообщения:                                                      //  
//  Получаем адрес отправителя.                                                         //
  if(PDU_OA_TYPE) {_SMSdecodGSM (sms->num, PDU_OA_LEN, PDU_OA_DATA);}                   //  Если адрес отправителя указан в алфавитноцифровом формате, то декодируем адрес отправителя как текст в 7-битной кодировке в строку num.
  else      {_SMSdecodAddr(sms->num, PDU_OA_LEN, PDU_OA_DATA);}                         //  Иначе декодируем адрес отправителя как номер в строку num.
//  Получаем дату отправки сообщения (дату получения SMS сервисным центром).            //  
  _SMSdecodDate(sms->tim, PDU_SCTS_DATA);                                               //  В строку tim вернётся текст содержания "ДД.ММ.ГГ ЧЧ.ММ.СС".
                                                                                        //
//  Получаем текст сообщения:                                                           //  
  if(PDU_DCS==0){_SMSdecodGSM ( sms->txt, PDU_UD_LEN            , PDU_UD_DATA, PDU_UDH_LEN    );} //  Получаем текст сообщения принятого в кодировке GSM.
  if(PDU_DCS==1){_SMSdecod8BIT( sms->txt, PDU_UD_LEN-PDU_UDH_LEN, PDU_UD_DATA+(PDU_UDH_LEN*2) );} //  Получаем текст сообщения принятого в 8-битной кодировке.
  if(PDU_DCS==2){_SMSdecodUCS2( sms->txt, PDU_UD_LEN-PDU_UDH_LEN, PDU_UD_DATA+(PDU_UDH_LEN*2) );} //  Получаем текст сообщения принятого в кодировке UCS2.
//  Возвращаем параметры составного сообщения:                                          //
  if(PDU_UDH_LEN>1){                                                                    //
    sms->lngID  = PDU_UDH_IED1;                                                         //  Идентификатор составного сообщения.
    sms->lngSum = PDU_UDH_IED2;                                                         //  Количество SMS в составном сообщении.
    sms->lngNum = PDU_UDH_IED3;                                                         //  Номер данной SMS в составном сообщении.
  }                                                                                     //
}                                                                                       //
                                                                                        //
//    ФУНКЦИЯ ОТПРАВКИ SMS СООБЩЕНИЯ:                                                   //  Функция возвращает: флаг успешной отправки SMS сообщения true/false
uint8_t  SIM800::SMSsend(const char* txt, const char* num, uint16_t lngID, uint8_t lngSum, uint8_t lngNum){ //  Аргументы функции:  txt - передаваемый текст, num - номер получателя, lngID - идентификатор склеенных SMS, lngSum - количество склеенных SMS, lngNum - номер данной склеенной SMS.
//      Готовим переменные:                                                             //  
  uint16_t txtLen      = _SMStxtLen(txt);                                               //  Количество символов (а не байтов) в строке.
  uint8_t  PDU_TYPE     = lngSum>1?0x41:0x01;;                                          //  Первый байт блока TPDU, указывает тип PDU, состоит из флагов RP UDHI SRR VPF RD MTI (их назначение совпадает с первым байтом команды AT+CSMP). Если сообщение составное (склеенное), то устанавливаем флаг UDHI для передачи заголовка.
  uint8_t  PDU_DA_LEN   = strlen(num); if(num[0]=='+'){PDU_DA_LEN--;}                   //  Первый байт блока DA, указывает количество полезных (информационных) полубайтов в блоке DA. Так как адрес получателя указывается ввиде номера, то значение данного блока равно количеству цифр в номере.
  uint8_t  PDU_DCS      = 0x00;                                                         //  Второй байт после блока DA, указывает на схему кодирования данных (кодировка текста сообщения).
  uint8_t  PDU_UD_LEN   = 0x00;                                                         //  Первый байт блока UD, указывает на количество символов (в 7-битной кодировке GSM) или количество байт (остальные типы кодировок) в блоке UD.
  uint16_t PDU_UDH_IED1 = lngID;                                                        //  Данные следуют за байтом IEDL (размер зависит от значения PDU_IEI). Для составных SMS значение IED1 определяет идентификатор длинного сообщения (все SMS в составе длинного сообщения должны иметь одинаковый идентификатор).
  uint8_t  PDU_UDH_IED2 = lngSum;                                                       //  Предпоследний байт блока UDH. Для составных SMS его значение определяет количество SMS в составе длинного сообщения.
  uint8_t  PDU_UDH_IED3 = lngNum;                                                       //  Последний байт блока UDH. Для составных SMS его значение определяет номер данной SMS в составе длинного сообщения.
  uint16_t PDU_TPDU_LEN = 0x00;                                                         //  
                                                                                        //
  uart.print(F("SMS_send: "));                                                        //
  uart.println(txt);                                                                  // 
  uart.print(F("phone: "));                                                        //
  uart.println(num); 
//  Блок TPDU включает все блоки PDU кроме блока SCA.                                   //
//      Определяем формат кодировки SMS сообщения:                                      //  
  for(uint8_t i=0; i<strlen(txt); i++){ if(uint8_t(txt[i])>=0x80){PDU_DCS=0x08;} }      //  Если в одном из байтов отправляемого текста установлен 7 бит, значит сообщение требуется закодировать в формате UCS2
//      Определяем класс SMS сообщения;                                                 //  
  if(vars.clsSMSsend==GSM_SMS_CLASS_0){PDU_DCS|=0x10;}else                                   //  SMS сообщение 0 класса
  if(vars.clsSMSsend==GSM_SMS_CLASS_1){PDU_DCS|=0x11;}else                                   //  SMS сообщение 1 класса
  if(vars.clsSMSsend==GSM_SMS_CLASS_2){PDU_DCS|=0x12;}else                                   //  SMS сообщение 2 класса
  if(vars.clsSMSsend==GSM_SMS_CLASS_3){PDU_DCS|=0x13;}                                       //  SMS сообщение 3 класса
//      Проверяем формат номера (адреса получателя):                                    //  
//      if(num[0]=='+'){if(num[1]!='7'){return false;}}                                 //  Если первые символы не '+7' значит номер указан не в международном формате.
//      else           {if(num[0]!='7'){return false;}}                                 //  Если первый символ  не  '7' значит номер указан не в международном формате.
//      Проверяем значения составного сообщения:                                        //  
  if(lngSum==0)    {return false;}                                                      //  Количество SMS в составном сообщении должно находиться в диапазоне от 1 до 255.
  if(lngNum==0)    {return false;}                                                      //  Номер      SMS в составном сообщении должен находиться в диапазоне от 1 до 255.
  if(lngNum>lngSum){return false;}                                                      //  Номер SMS не должен превышать количество SMS в составном сообщении.
//      Проверяем длину текста сообщения:                                               //  
  if(PDU_DCS%16==0){ if(lngSum==1){ if(txtLen>160){txtLen=160;} }else{ if(txtLen>152){txtLen=152;} } }    //  В формате GSM  текст сообщения не должен превышать 160 символов для короткого сообщения или 152 символа  для составного сообщения.
  if(PDU_DCS%16==8){ if(lngSum==1){ if(txtLen>70 ){txtLen= 70;} }else{ if(txtLen> 66){txtLen= 66;} } }    //  В формате UCS2 текст сообщения не должен превышать  70 символов для короткого сообщения или 66  символов для составного сообщения.
//      Определяем размер блока UD: (блок UD может включать блок UDH - заголовок, который так же учитывается) //  Количество байт отводимое для кодированного текста и заголовка (если он присутствует).
  if(PDU_DCS%16==0){PDU_UD_LEN=txtLen;  if(lngSum>1){PDU_UD_LEN+=8;}}                   //  Получаем размер блока UD в символах, при кодировке текста сообщения в формате GSM  и добавляем размер заголовка (7байт = 8символов) если он есть.
  if(PDU_DCS%16==8){PDU_UD_LEN=txtLen*2;  if(lngSum>1){PDU_UD_LEN+=7;}}                 //  Получаем размер блока UD в байтах,   при кодировке текста сообщения в формате UCS2 и добавляем размер заголовка (7байт) если он есть.
//      Определяем размер блока TPDU:                                                       //
  if(PDU_DCS%16==0){PDU_TPDU_LEN = 0x0D + PDU_UD_LEN*7/8; if(PDU_UD_LEN*7%8){PDU_TPDU_LEN++;} } //  Размер блока TPDU = 13 байт занимаемые блоками (PDUT, MR, DAL, DAT, DAD, PID, DCS, UDL) + размер блока UD (рассчитывается из количества символов указанных в блоке UDL).
  if(PDU_DCS%16==8){PDU_TPDU_LEN = 0x0D + PDU_UD_LEN;}                                   //  Размер блока TPDU = 13 байт занимаемые блоками (PDUT, MR, DAL, DAT, DAD, PID, DCS, UDL) + размер блока UD (указан в блоке UDL).
//      Отправляем SMS сообщение:                                                       //
  uart.print(F("AT+CMGS=")); uart.println(PDU_TPDU_LEN);                        //    
  //if(GET_MODEM_ANSWER(">", 2000)!=R_OK) return R_ERR;
  DELAY(1000);
//  Выполняем команду отправки SMS без сохранения в область памяти, отводя на ответ до 1 сек.
//      Создаём блок PDU:                                                               //  
//      |                                                     PDU                                                     |           //  
//      +------------------+------------------------------------------------------------------------------------------+           //  
//      |                  |                                           TPDU                                           |           //  
//      |                  +----------------------------------------------+-------------------------------------------+           //  
//      |        SCA       |                                              |                    UD                     |           //  
//      |                  |           +---------------+                  |     +--------------------------------+    |           //  
//      |                  |           |      DA       |                  |     |              UDH               |    |           //  
//      +------------------+------+----+---------------+-----+-----+------+-----+--------------------------------+----+           //  
//      | SCAL [SCAT SCAD] | PDUT | MR | DAL [DAT DAD] | PID | DCS | [VP] | UDL | [UDHL IEI IEDL IED1 IED2 IED3] | UD |           //  
//                                                                                          //  
  uart.print(F("00"));                                                                // SCAL (Service Center Address Length)   //  Байт указывающий размер адреса сервисного центра.   Указываем значение 0x00. Значит блоков SCAT и SCAD не будет. В этом случае модем возьмет адрес сервисного центра не из блока SCA, а с SIM-карты.
  uart.print(_char(PDU_TYPE/16));                                                     //
  uart.print(_char(PDU_TYPE%16));                                                     // PDUT (Packet Data Unit Type)           //  Байт состоящий из флагов определяющих тип блока PDU.  Указываем значение 0x01 или 0x41. RP=0 UDHI=0/1 SRR=0 VPF=00 RD=0 MTI=01. RP=0 - обратный адрес не указан, UDHI=0/1 - наличие блока заголовка, SRR=0 - не запрашивать отчёт о доставке, VPF=00 - нет блока срока жизни сообщения, RD=0 - не игнорировать копии данного сообщения, MTI=01 - данное сообщение является исходящим.
  uart.print(F("00"));                                                                // MR (Message Reference)                 //  Байт                          Указываем значение 0x00. 
  uart.print(_char(PDU_DA_LEN/16));                                                   //
  uart.print(_char(PDU_DA_LEN%16));                                                   // DAL  (Destination Address Length)      //  Байт указывающий размер адреса получателя.  Указываем значение количество цифр в номере получател (без знака + и т.д.). +7(123)456-78-90 => 11 цифр = 0x0B.
  uart.print(F("91"));                                                                // DAT  (Destination Address Type)        //  Байт хранящий тип адреса получателя.        Указываем значение 0x91. Значит адрес получателя указан в международном формате: +7******... .
  _SMScoderAddr(num);                                                                   // DAD  (Destination Address Date)        //  Блок с данными адреса получателя.           Указываем номер num, кодируя его в конец строки.
  uart.print(F("00"));                                                                // PID  (Protocol Identifier)             //  Байт с идентификатором протокола передачи данных.   Указываем значение 0x00. Это значение по умолчанию.
  uart.print(_char(PDU_DCS/16));                                                      //
  uart.print(_char(PDU_DCS%16));                                                      // DCS  (Data Coding Scheme)              //  Байт указывающий схему кодирования данных.        Будем использовать значения 00-GSM, 08-UCS2 и 10-GSM, 18-UCS2. Во втором случае сообщение отобразится на дисплее, но не сохраняется на телефоне получателя.
  uart.print(_char(PDU_UD_LEN/16));                                                   //
  uart.print(_char(PDU_UD_LEN%16));                                                   // UDL  (User Data Length)                //  Байт указывающий размер данных (размер блока UD).   Для 7-битной кодировки - количество символов, для остальных - количество байт.
  if(PDU_UDH_IED2>1){                                                                   // UDH  (User Data Header)                //  Если отправляемое сообщение является стоставным,    то добавляем заголовок (блок UDH)...
    uart.print(F("06"));                                                              // UDHL (User Data Header Length)         //  Байт указывающий количество оставшихся байт блока UDH.  Указываем значение 0x06. Это значит что далее следуют 6 байт: 1 байт IEI, 1 байт IEDL, 2 байта IED1, 1 байт IED2 и 1 байт IED3.
    uart.print(F("08"));                                                              // IEI  (Information Element Identifier)  //  Байт указывающий на назначение заголовка.       Указываем значение 0x08. Это значит что данное сообщение является составным с 2 байтным блоком IED1 (если указать значение 0x0, то блок IED1 должен занимать 1 байт).
    uart.print(F("04"));                                                              // IEDL (Information Element Data Length) //  Байт указывающий количество оставшихся байт блока IED.  Указываем значение 0x04. Это значит что далее следуют 4 байта: 2 байта IED1, 1 байт IED2 и 1 байт IED3.
    uart.print(_char(PDU_UDH_IED1/4096));                                             //
    uart.print(_char(PDU_UDH_IED1%4096/256));                                         // IED1 (Information Element Data 1)      //  Блок указывающий идентификатор составного сообщения.  Все сообщения в составе составного должны иметь одинаковый идентификатор.
    uart.print(_char(PDU_UDH_IED1%256/16));                                           //
    uart.print(_char(PDU_UDH_IED1%16));                                               //    (2 байта)                           //
    uart.print(_char(PDU_UDH_IED2/16));                                               //
    uart.print(_char(PDU_UDH_IED2%16));                                               // IED2 (Information Element Data 1)      //  Байт указывающий количество SMS в составе составного сообщения.
    uart.print(_char(PDU_UDH_IED3/16));                                               //
    uart.print(_char(PDU_UDH_IED3%16));                                               // IED3 (Information Element Data 1)      //  Байт указывающий номер данной SMS в составе составного сообщения.
  }                                                                                     //
                                                                                        // UD (User Data)                         //  Блок данных пользователя (текст сообщения).       Указывается в 16-ричном представлении.
//  if(GET_MODEM_ANSWER(">", 2000)!=R_OK) return false;                                        //  Выполняем команду отправки SMS без сохранения в область памяти, отводя на ответ до 1 сек.
//      Проверяем готовность модуля к приёму блока PDU для отправки SMS сообщения:      //
  DELAY(500);                                                                           //  Выполняем отправку текста, выделяя на неё до 500 мс.
//      Кодируем и передаём текст сообщения по 24 символа:                              //  Передача текста частями снижает вероятность выхода за пределы "кучи" при передаче длинных сообщений.
  uint16_t txtPartSMS=0;                                                                //  Количество отправляемых символов за очередной проход цикла.
  uint16_t txtPosNext=0;                                                                //  Позиция после последнего отправленного символа в строке txt.
                                                                                        //
  while (txtLen) {                                                                      //
    txtPartSMS=txtLen>24?24:txtLen; txtLen-=txtPartSMS;                                 //
    if(PDU_DCS%16==0){txtPosNext=_SMScoderGSM ( txt, txtPosNext, txtPartSMS);}          //  Записать в конец строки, закодированный в формат GSM  текст сообщения, текст брать из строки txt начиная с позиции txtPosNext, всего взять txtPartSMS символов (именно символов, а не байтов).
    if(PDU_DCS%16==8){txtPosNext=_SMScoderUCS2( txt, txtPosNext, txtPartSMS);}          //  Записать в конец строки, закодированный в формат UCS2 текст сообщения, текст брать из строки txt начиная с позиции txtPosNext, всего взять txtPartSMS символов (именно символов, а не байтов).        
    DELAY(500);                                                                         //
  }                                                                                     //
  uart.print("\32");
  //uart.write(26);

  return GET_MODEM_ANSWER("+CMGS:", 10000);
}                                                                                       //
                                                                                        //
//    ФУНКЦИЯ АВТООПРЕДЕЛЕНИЯ КОДИРОВКИ СКЕТЧА:                                         //  
void  SIM800::TXTsendCodingDetect(const char* str){                                      //  Аргументы функции:  строка состоящая из символа 'п' и символа конца строки.
  if(strlen(str)==2)      {vars.codTXTsend=GSM_TXT_UTF8;   }                                 //  Если символ 'п' занимает 2 байта, значит текст скетча в кодировке UTF8.
  else  if(uint8_t(str[0])==0xAF) {vars.codTXTsend=GSM_TXT_CP866;    }                       //  Если символ 'п' имеет код 175, значит текст скетча в кодировке CP866.
  else  if(uint8_t(str[0])==0xEF) {vars.codTXTsend=GSM_TXT_WIN1251;  }                       //  Если символ 'п' имеет код 239, значит текст скетча в кодировке WIN1251.
}                                                                                       //
                                                                                        //
//    ФУНКЦИЯ ПРОВЕРКИ НАЛИЧИЯ ПРИНЯТЫХ SMS СООБЩЕНИЙ:                                  //  Функция возвращает: целочисленное значение uint8_t соответствующее количеству принятых SMS.
uint8_t SIM800::SMSavailable(void){                                                      //  Аргументы функции:  отсутствуют.
  if(_SMSsum()==0){return 0;}                                                           //  Если в выбранной памяти нет SMS (входящих и исходящих) то возвращаем 0.
  uart.println(F("AT+CMGD=1,3"));                                                     //
  GET_MODEM_ANSWER(OK,10000);                                                           //  Выполняем команду удаления SMS сообщений из памяти, выделяя на это до 10 секунд, второй параметр команды = 3, значит удаляются все прочитанные, отправленные и неотправленные SMS сообщения.
  return _SMSsum();                                                                     //  Так как все прочитанные, отправленные и неотправленные SMS сообщения удалены, то функция _SMSsum вернёт только количество входящих непрочитанных SMS сообщений.
}                                                                                       //
                                                                                        //
//    ФУНКЦИЯ ПОЛУЧЕНИЯ КОЛИЧЕСТВА ВСЕХ SMS В ПАМЯТИ:                                   //  Функция возвращает: число uint8_t соответствующее общему количеству SMS хранимых в выбранной области памяти.
uint8_t SIM800::_SMSsum(void){                                                           //  Аргументы функции:  отсутствуют.
  uart.println(F("AT+CPMS?"));                                                        //
  //  Выполняем команду получения выбранных областей памяти для хранения SMS сообщений. //
  GET_MODEM_ANSWER("+CPMS:",10000);                                                     //
  return SMSsum;                                                                        //
}                                                                                       //

#endif 
