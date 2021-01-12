#ifndef SIM800_H
#define SIM800_H

#include "text.h"
#include "my_sensors.h"

#if TM1637_ENABLE
#   include "clock.h"
#endif

#define NLENGTH 14 // Max. length of phone number 
#define TLENGTH 21 // Max. length of text for number
#define DTMF_BUF_SIZE 6

// Опрашивает модем
#define READ_COM_FIND_RAM(str) strstr(param.rx->GetText(),str)
#define READ_COM_FIND(str)     strstr_P(param.rx->GetText(),PSTR(str))
#define READ_COM_FIND_P(str)   strstr_P(param.rx->GetText(),(char*)pgm_read_word(&(str)))

uint8_t common_parser();

enum com_res : uint8_t {R_NULL, R_OK, R_ERR};

typedef struct op
{
  TEXT*    tx;
  TEXT*    rx;
  uint32_t opros_time;
  uint32_t timeRegularOpros;
  uint16_t DTMF[2];
  char     dtmf[DTMF_BUF_SIZE]; // DTMF команда
  uint8_t  dtmf_index     :3;
  uint8_t  sim800_enable  :1;
  uint8_t  esp_enable     :1;
  uint8_t  gprs_connect   :1; // показывает, есть ли соединение
  uint8_t  need_init      :1;
  uint8_t  sleep          :1;
  uint8_t  ceng           :1;
} COMMON_OP;

typedef struct cell
{
    char    phone[NLENGTH];
    char    name[TLENGTH];
    uint8_t index; // индекс ячейки в телефонной книге  
} ABONENT_CELL;


typedef struct vars{
    uint8_t SmsLen;       // длина SMS
    uint8_t gsm_operator:3;
    uint8_t reset_count:3;
    uint8_t gprs_init_count:3;
#if PDU_ENABLE
    uint8_t lngSum : 3;   // количество склеенных SMS
    uint8_t lngNum : 3;   // номер данной склеенной SMS
    uint8_t codTXTread:2; // Тип кодировки строки StrIn.
    uint8_t codTXTsend:2; // Тип кодировки строки StrIn.
    uint8_t clsSMSsend:3;
#endif    
} SIM_VARS; 

#if PDU_ENABLE

typedef struct sms_info{
    uint16_t lngID; // идентификатор склеенных SMS
    uint8_t lngSum; // количество склеенных SMS
    uint8_t lngNum; // номер данной склеенной SMS
    char tim[18];   // дата, время
    char num[NLENGTH];   // номер получателя
    char* txt;  // передаваемый текст  
} SMS_INFO;

#endif

class SIM800
{
  public:
    SIM800::SIM800();
    ~SIM800();
    void        init    ();
    void        wiring  ();    
    void        email   ();
    void        reinit  ();
    void        ring    ();
    void        DTMF_parser();
    uint8_t     parser  ();
    uint8_t     runAT   (uint16_t wait_ms, const char* answer);
    ABONENT_CELL admin; // телефон админа

  private:
    ABONENT_CELL last_abonent;
    SIM_VARS vars;
    
#if TM1637_ENABLE

    uint32_t    timeSync;

#endif

    void        reinit_end          ();
    char*       get_number_and_type (char* p);
    char*       get_name            (char* p);
    uint8_t     read_com            (const char* answer);
    
#if PDU_ENABLE

    uint8_t     SMSsum = 0;
//    uint8_t     numSMS;                                                                               //  Объявляем  переменную для хранения № прочитанной SMS (число)
    uint8_t     maxSMS;                                                                                 //  Объявляем  переменную для хранения объема памяти SMS (число)
    
    uint8_t     _num                (char);                                                             //  Преобразование символа в число(аргумент функции: символ 0-9,a-f,A-F)
    char        _char               (uint8_t);                                                          //  Преобразование числа в символ (аргумент функции: число 0-15)
    uint8_t     _SMSsum             (void);                                                             //  Получение кол SMS в памяти(без аргументов)
//    uint8_t     SMSmax              (void);  
    uint16_t    _SMStxtLen          (const char*);                                                      //  Получение количества символов в строке (строка с текстом)
    uint16_t    _SMScoderGSM        (const char*, uint16_t, uint16_t=255);                              //  Кодирование текста в GSM (строка с текстом, позиция взятия из строки, количество кодируемых символов) Функция возвращает позицию после последнего закодированного символа из строки txt.
    void        _SMSdecodGSM        (      char*, uint16_t, uint16_t, uint16_t=0);                      //  Разкодирование текста GSM (строка для текста, количество символов в тексте, позиция начала текста в строке, количество байт занимаемое заголовком)
    void        _SMSdecod8BIT       (      char*, uint16_t, uint16_t);                                  //  Разкодирование текста 8BIT (строка для текста, количество байт в тексте, позиция начала текста в строке)
    uint16_t    _SMScoderUCS2       (const char*, uint16_t, uint16_t=255);                              //  Кодирование текста в UCS2 (строка с текстом, позиция взятия из строки, количество кодируемых символов) Функция возвращает позицию после последнего закодированного символа из строки txt.
    void        _SMSdecodUCS2       (      char*, uint16_t, uint16_t);                                  //  Разкодирование текста UCS2 (строка для текста, количество байт в тексте, позиция начала текста в строке)
    void        _SMScoderAddr       (const char*);                                                      //  Кодирования адреса SMS (строка с адресом)
    void        _SMSdecodAddr       (      char*, uint16_t, uint16_t);                                  //  Разкодирование адреса SMS (строка для адреса, количество полубайт в адресе, позиция адреса  в строке)
    void        _SMSdecodDate       (      char*,           uint16_t);                                  //  Разкодирование даты SMS (строка для даты, позиция даты в строке)
    uint8_t     SMSsend             (const char*, const char*, uint16_t=0x1234, uint8_t=1, uint8_t=1);  //  Отправка SMS (аргумент функции: текст, номер, идентификатор составного SMS сообщения, количество SMS в составном сообщении, номер SMS в составном сообщении) 
    void        PDUread             (SMS_INFO*);
    void        TXTsendCodingDetect (const char* str);    
    uint8_t     SMSavailable        (void);
    bool        SendSMS             ();
#endif 
};

#define FLAG_STATUS(flag,name) {param.tx->AddText_P(PSTR(name));param.tx->AddText_P(flag?PSTR(VKL):PSTR(VIKL));}

#endif
