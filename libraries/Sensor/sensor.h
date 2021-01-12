#ifndef SENS_h
#define SENS_h

#include "settings.h"
#include "text.h"

#if IR_ENABLE
#   include "ir.h"
#endif

#if RF_ENABLE
#   include "nRF24.h"
#endif

#if RC_ENABLE
#   include "rc.h"
#endif

#if DHT_ENABLE
#   include "stDHT.h"
#endif

#if DS18_ENABLE
#if GYVER_DS18B20_LIB
#   include <microDS18B20.h>
#else
#   include <OneWire.h>
#   include <DallasTemperature.h>
#endif // GYVER_DS18B20_LIB
#endif // DS18_ENABLE

// Сюда добавляем типы датчиков.
// Датчики температуры DHT11, DHT21, DHT22 объявлены в stDHT.h
enum type : uint8_t { 
    // датчик с одним цифровым выходом
    DIGITAL_SENSOR,
    // датчик с аналоговым выходом
    ANALOG_SENSOR,
#if IR_ENABLE
    // беспроводной датчик с ик диодом
    IR_SENSOR,
#endif
#if RF_ENABLE
    // беспроводной датчик RF24L01
    RF24_SENSOR,
#endif
#if RC_ENABLE
    // беспроводной датчик 433 МГц
    RC_SENSOR,
#endif
#if DS18_ENABLE
    // датчик температуры DS18B20
    DS18B20, 
#endif
#if TERM_ENABLE
    // датчик температуры - термистор
    TERMISTOR
#endif
};

typedef struct
{
    uint8_t alarm   :1; // флаг срабатывания
    uint8_t level   :1; // высокий или низкий уровень пина
    uint8_t prev    :1; // предыдущее состояние пина
    uint8_t circuit :CIRCUIT_BITS; // контур охраны
    uint8_t enable  :1; // флаг доступности (вкл/откл)
#if DS18_ENABLE
    uint8_t index   :3; // индекс датчика ds18b20 на линии
#endif   
}SENS_PARAMS;

class Sensor
{
  public:
    // volatile означает указание компилятору не оптимизировать код её чтения,
    // поскольку её значение может изменяться внутри обработчика прерывания
    RET_TYPE value; // последнее показание датчика
    SENS_PARAMS param;
    SENSOR_INIT_PARAM* settings;

    Sensor(SENSOR_INIT_PARAM* sens);

    ~Sensor();
    
    uint8_t opros();      // возвращает флаг срабатывания датчика.
    void get_info(TEXT *str); // возвращает строку с именем датчика и флаг срабатывания
    
  private:
#if DHT_ENABLE
    DHT* dht; // датчик температуры и влажности   
#endif

    bool get_pin_state();
};

#endif

