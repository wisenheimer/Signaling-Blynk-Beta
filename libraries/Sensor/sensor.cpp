#include "sensor.h"
#include "termistor.h"

#define LOW_WORD   (int16_t)(settings->code>>16)
#define HIGH_WORD  (int16_t)(settings->code)

#if DS18_ENABLE

#if GYVER_DS18B20_LIB==0
  static uint8_t ds18b20_count = 0;
#endif

#endif

Sensor::Sensor(SENSOR_INIT_PARAM* sens)
{
  param.alarm = 0;
  param.circuit = 0;
  param.enable = 1;

  settings = sens;

  if(settings->type == DIGITAL_SENSOR || settings->type == ANALOG_SENSOR)
  {
    param.level = settings->code;
    param.prev = param.level;

    if(settings->pin >= A0) return;

    pinMode(settings->pin, INPUT);
    digitalWrite(settings->pin, LOW);
  }
#if DS18_ENABLE
  else // датчик температуры DS18B20
  if(settings->type == DS18B20)
  {
#if GYVER_DS18B20_LIB
    param.index = 0;
#else
    param.index = ds18b20_count++;
#endif
  } 
#endif
}


Sensor::~Sensor()
{
#if DS18_ENABLE

#if GYVER_DS18B20_LIB==0

  if(settings->type == DS18B20 && ds18b20_count) ds18b20_count--;

#endif

#endif
}

bool Sensor::get_pin_state()
{
  bool state = false;

  if(settings->pin >= A6)
  {
    if(analogRead(settings->pin) > 500) state = true;
  }
  else state = digitalRead(settings->pin);
  
  return state;
}

uint8_t Sensor::opros()  
{
  bool flag_callback = false;

  switch (settings->type)
  {
#if DS18_ENABLE
    case DS18B20:
    {
#if GYVER_DS18B20_LIB
      MicroDS18B20 ds18b20(settings->pin); // Датчик один - не используем адресацию

      ds18b20.setResolution(9);
      if(param.index==0)
      {
        ds18b20.requestTemp();
        param.index=1;
        return 0;
      }
      param.index=0;
      DS_TEMP_TYPE tempC = ds18b20.getTemp();
      if(tempC == 0)
#else
      OneWire oneWire(settings->pin);
      // Pass our oneWire reference to Dallas Temperature. 
      DallasTemperature ds18b20(&oneWire);
      // arrays to hold device addresses
      DeviceAddress Thermometer;
      ds18b20.begin();  
      // search for devices on the bus and assign based on an index.
      if (!ds18b20.getAddress(Thermometer, param.index))
      {
        DEBUG_PRINTLN(F("Did`t find address ds18b20"));
        return 0;
      }  
      // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
      ds18b20.setResolution(Thermometer, 9);
      ds18b20.requestTemperatures(); // Send the command to get temperatures
      RET_TYPE tempC = ds18b20.getTempC(Thermometer);
      if(tempC == DEVICE_DISCONNECTED_C)    
#endif
        return 0;
      if(tempC < LOW_WORD || tempC > HIGH_WORD) flag_callback = !param.alarm;
      value = tempC;
    }
    break;
#endif

#if IR_ENABLE
    case IR_SENSOR:
      value = IRgetValue();
      if(value == settings->code)
      {
        flag_callback = !param.alarm;
      } 
    break;
#endif

#if RF_ENABLE
    case RF24_SENSOR:
      value = nRFgetValue();
      if(value == settings->code)
      {
      	flag_callback = !param.alarm;
      
      	nRFclear();
      }
    break;
#endif

#if RC_ENABLE
    case RC_SENSOR:
      value = rc_value();
      if(value == settings->code)
      {
        flag_callback = !param.alarm;
      }
    break;    
#endif

    case DIGITAL_SENSOR:
      value = get_pin_state();

      if(param.level!=value)
      {
        if(param.level==param.prev)
        {
          flag_callback = !param.alarm;
        }
      }
      param.prev = value;
    break;
  // если показание аналогового датчика превысило пороговое значение
#if DHT_ENABLE
    
    case DHT11:
    case DHT21:
    case DHT22:
      dht = new DHT(settings->type, settings->pin);
      value = dht->readTemperature();
      delete dht;
      if(value < LOW_WORD || value > HIGH_WORD)
      {
        flag_callback = !param.alarm;
      }
    break;
#endif

#if TERM_ENABLE
	case TERMISTOR:
    TERM_ADC_TO_C(value,value)
    if(value < LOW_WORD || value > HIGH_WORD)
    {
      flag_callback = !param.alarm;
    }
	 break;
#endif

    default:
      value = analogRead(settings->pin);
      if(value >= settings->code)
      {
        flag_callback = !param.alarm;
      }
  }
        
  if(flag_callback)
  {
    DEBUG_PRINT(F("Callback = "));
    DEBUG_PRINT(settings->callback==NULL?'0':'1');
    DEBUG_PRINT(F(" param.enable = "));
    DEBUG_PRINTLN(param.enable);

    if(param.enable)
    {
      param.alarm=1;
      settings->callback();
    }
  }
      
  return param.alarm;
}

// Записываем в строку показания датчика
void Sensor::get_info(TEXT *str)
{
  int i;

  str->AddText_P(settings->name);
  str->AddChar(':');

  switch (settings->type)
  {
    case ANALOG_SENSOR:
#if RF_ENABLE
    case RF24_SENSOR:
#endif
#if RC_ENABLE
    case RC_SENSOR:
#endif
      str->AddInt(value);
      break;
    
#if (DHT_ENABLE || TERM_ENABLE || DS18_ENABLE)
  #if DHT_ENABLE
    case DHT11:
    case DHT21:
    case DHT22:
  #endif
  #if TERM_ENABLE
    case TERMISTOR:
  #endif
  #if DS18_ENABLE
    case DS18B20:
  #endif
      str->AddText_P(PSTR("t="));
      str->AddInt(value);
      i = value*100;
      i = abs(i%100);
      if(i)
      {
        str->AddChar('.');        
        str->AddInt(i); 
      }
      str->AddChar('C');
      break;
#endif
    default:
      // добавляем число срабатываний датчика
      str->AddInt(param.alarm);
      str->AddChar('(');
      // добавляем текущее состояние пина (0 или 1)
      str->AddInt(value);
      str->AddChar(')');
  }
  str->AddChar(' ');
}
