#ifndef MAIN_TYPE_H
#define MAIN_TYPE_H

//------------------------------------------------------------------------------
// Вывод отладочных сообщений в Serial
// В рабочем режиме вывод должен быть запрещён
#define DEBUG_MODE 0

#ifdef __AVR__
  #define GYVER_UART_LIB 0
  #define GYVER_TM1637_LIB 0
  #define GYVER_DS18B20_LIB 1

  #define DELAY _delay_ms

  #if GYVER_UART_LIB
  
    #include "GyverUART.h"

  #else

    #include "Arduino.h"

    #define uart Serial

  #endif

  //-------------------------------------------------------
  // Зарезервированные номера пинов (не изменять!!!)
  #define RING_PIN  2 // отслеживает вызовы с модема
  #define POWER_PIN 3 // отслеживает наличие питания
  #define DOOR_PIN  4 // датчик двери (геркон). Один конец на GND, второй на цифровой пин arduino.
  #define BOOT_PIN  5 // перезагрузка модема
  #define RADAR_PIN 6 // микроволновый датчик движения RCWL-0516
  // Пины для подключения модуля RF24L01 по SPI интерфейсу
  #define CE_PIN    9
  #define CSN_PIN   10
  #define MO_PIN    11
  #define MI_PIN    12
  #define SCK_PIN   13
  // Для ИК-приёмника
  #define RECV_PIN  11
  // Зуммер (пищалка)
  #define BEEP_PIN  A2

  // включение/отключение сирены
  #if SIRENA_ENABLE

    #define SIREN_ON      if(flags.SIREN_ENABLE){digitalWrite(SIREN_RELAY_PIN,SIREN_RELAY_TYPE);}
    #define SIREN_OFF     if(flags.SIREN_ENABLE){digitalWrite(SIREN_RELAY_PIN,!SIREN_RELAY_TYPE);}
    // Инициализируем пины включения реле сирены
    #define SIREN_INIT    pinMode(SIREN_RELAY_PIN,OUTPUT);digitalWrite(SIREN_RELAY_PIN,!SIREN_RELAY_TYPE);SET_FLAG_ONE(SIREN_ENABLE);

  #else

    #define SIREN_ON
    #define SIREN_OFF

    #define SIREN_INIT

  #endif

  // Продолжительность тревоги в миллисекундах, после чего счётчики срабатываний обнуляются.
  #define ALARM_MAX_TIME 60000
  // активируем флаг тревоги для сбора информации и отправки e-mail
  #define ALARM_ON  if(flags.GUARD_ENABLE && !flags.ALARM){flags.ALARM=1;modem->phone->ring(); \
                    param.tx->AddText_P(PSTR(ALARM_ON_STR));sensors->GetInfo(param.tx);param.tx->AddChar('\n'); \
                    DEBUG_PRINTLN(param.tx->GetText());SIREN_ON}

  // по окончании времени ALARM_MAX_TIME обнуляем флаг тревоги и отправляем e-mail с показаниями датчиков
  #define ALARM_OFF {flags.ALARM=0;param.tx->AddText_P(PSTR(ALARM_OFF_STR));sensors->GetInfo(param.tx);param.tx->AddChar('\n');sensors->Clear();SIREN_OFF}

  #define PINS_INIT pinMode(RING_PIN,INPUT);digitalWrite(RING_PIN,LOW);pinMode(POWER_PIN,INPUT); \
                    digitalWrite(POWER_PIN,LOW);pinMode(BOOT_PIN,OUTPUT);digitalWrite(BOOT_PIN,HIGH);SIREN_INIT \
                    pinMode(LED_BUILTIN,OUTPUT);digitalWrite(LED_BUILTIN,LOW); // Лампочка состояния флага охраны  

#else

  #define uart Serial

#endif // __AVR__

//-------------------------------------------------------
// Debug directives
//-------------------------------------------------------
#if DEBUG_MODE
#	define DEBUG_PRINT		uart.print
#	define DEBUG_PRINTLN	uart.println
# define DEBUG_PRINTF   uart.printf
#else
#	define DEBUG_PRINT(...)
#	define DEBUG_PRINTLN(...)
# define DEBUG_PRINTF(...)
#endif

#if PDU_ENABLE
# define ALARM_ON_STR  " Тревога!"
# define ALARM_OFF_STR " Конец тревоги:"
# define VKL  " вкл. "
# define VIKL " выкл. "
# define SVET  " Свет"
# define SENSOR "Датчик "
# define KONTUR "Контур"
# define ALARM_NAME   "Тревога"
# define GUARD_NAME   "Охрана"
# define SIREN_NAME   "Сирена"
# define EMAIL_NAME   "Почта(GPRS)"
# define SMS_NAME     "СМС"
# define RING_NAME    "Звонок"
#else
# define ALARM_ON_STR  " ALARM!"
# define ALARM_OFF_STR " ALARM END:"
# define VKL  " ON. "
# define VIKL " OFF. "
# define SVET " Light"
# define SENSOR "Sensor "
# define KONTUR "Circuit"
# define ALARM_NAME   "ALARM"
# define GUARD_NAME   "GUARD"
# define SIREN_NAME   "SIREN"
# define EMAIL_NAME   "EMAIL(GPRS)"
# define SMS_NAME     "SMS"
# define RING_NAME    "RING"
#endif

#define SYMBOL '%'

// тип для хранения показаний датчиков
#define RET_TYPE float

//-------------------------------------------------------
// DTMF команды.
enum flag : uint8_t {
  GUARD_ON = 1,     // 1# - постановка на охрану
  GUARD_OFF,        // 2# - снятие с охраны 
  EMAIL_ON_OFF,     // 3# - включить/отключить GPRS/EMAIL
  SMS_ON_OFF,       // 4# - включить/отключить SMS
  TEL_ON_OFF,       // 5# - включить/отключить звонок при тревоге
  GET_INFO,         // 6# - сбор и отправка всех данных датчиков
  EMAIL_ADMIN_PHONE,// 7# - отправляем на почту номер админа
  ADMIN_NUMBER_DEL, // 8# - удаление номера админа из телефонной книги
  MODEM_RESET,      // 9# - перезагрузка SIM800L
  ESP_RESET,        // 10# - перезагрузка ESP8266
  BAT_CHARGE,       // 11# - показывает заряд батареи в виде строки
                    // +CBC: 0,100,4200
                    // где 100 - процент заряда
                    // 4200 - напряжение на батарее в мВ.
  SIREN_ON_OFF      // 12# - включить/отключить использование сирены
};

//-------------------------------------------------------
            
typedef struct common_flag
{
  uint8_t ALARM         : 1; 	// флаг тревоги при срабатывании датчиков
  uint8_t GUARD_ENABLE  : 1; 	// вкл/откл защиту
#if SIRENA_ENABLE
  uint8_t SIREN_ENABLE  : 1;  // вкл сирену
#endif
  uint8_t EMAIL_ENABLE  : 1;  // отправка отчётов по e-mail
  uint8_t SMS_ENABLE    : 1; 	// отправка отчётов по sms
  uint8_t RING_ENABLE   : 1; 	// включает/выключает звонки  
  uint8_t INTERRUPT     : 1;  // прерывание
}FLAGS;

#define INV_FLAG(buf,flag)  (buf^=(flag)) // инвертировать бит

#define INVERT_FLAG(flag)   flags.flag=!flags.flag

#define I2C_ARDUINO_ADDR 8
#define I2C_NAME		      "SENS:"
#define I2C_FLAG          "FLAG:"
#define I2C_DATA          0x9090
#define I2C_SENS_VALUE 	  1
#define I2C_FLAGS         2
#define I2C_SENS_NUM      3
#define I2C_SENS_NAME 	  4
#define I2C_FLAG_NAMES    5
#define I2C_DTMF          6
#define I2C_SENS_ENABLE   7
#define I2C_CIRCUIT       8
#define I2C_MASK_CIRCUIT  9
#define I2C_TIME_SYNC     10

#define START_BYTE		    0x91
#define END_BYTE		      0x92

//#define FLAG_COUNT	2

#define CIRCUIT_BITS 1 // количество разрядов для значения номера контура
#define CIRCUIT_NUM 2 // количество контуров

struct i2c_cmd
{
  uint16_t type;
  uint16_t data;
  uint8_t cmd; 
};

struct i2c_sens_value
{
  uint16_t type;
  uint8_t cmd;
  uint8_t index   : 6; // 64 датчика
  uint8_t circuit : CIRCUIT_BITS; // 2 контура
  uint8_t enable  : 1; // флаг включения/отключения датчиков
  RET_TYPE value;
};

typedef struct
{
  char* name;
  bool value;
}FLAG;

/*
 * Датчики разделены на группы - 2 контура охраны.
 * Из Блинк можно включать и отключать контур.
 * Например внешний периметр охраны это 1 контур,
 * внутренний это 2 контур. Находясь дома, можно включить
 * только внешний контур.
*/

typedef struct
{
  char* name;
  RET_TYPE value;
  uint8_t index   : 6;  // порядковый номер в контуре 
  uint8_t circuit : CIRCUIT_BITS; // в какой охранный контур входит
  uint8_t enable  : 1; // включен или нет  
}DATCHIK;

typedef struct
{
  uint8_t type;
  const char* name;
  uint32_t code;
  void (*callback)();
  uint8_t pin;
}SENSOR_INIT_PARAM;

#endif // MAIN_TYPE_H
