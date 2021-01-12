
#include "my_sensors.h"

extern bool esp_flag_sens_update;

MY_SENS::MY_SENS(SENSOR_INIT_PARAM* sens, uint8_t num)
{
  sensors = new Sensor*[num];
  for (uint8_t i = 0; i < num; i++)
  {
    sensors[i] = new Sensor(sens+i);
  }
#if DEBUG_MODE
  for (uint8_t i = 0; i < num; i++)
  {
    DEBUG_PRINT(F("type:")); DEBUG_PRINT(sensors[i]->settings->type);
    DEBUG_PRINT(F(" name:"));
    
    char ch;
    uint8_t j = 0;

    while(ch = (char)pgm_read_byte(sensors[i]->settings->name+j))
    {
      DEBUG_PRINT(ch);
      j++;
    }
    DEBUG_PRINT(F(" code:")); DEBUG_PRINT(sensors[i]->settings->code);
    DEBUG_PRINT(F(" pin:")); DEBUG_PRINTLN(sensors[i]->settings->pin);
  }
#endif
  
  size = num;
  circuit = 0xFF;

#if IR_ENABLE
  // инициализируем ИК приёмник на 11 пине
  IRrecvInit();
#endif

// радиомодуль 433 МГц
#if RC_ENABLE
  rc_init();
#endif

// радиомодуль nRF24L01
#if RF_ENABLE
  nRF24init(1);
#endif
}

MY_SENS::~MY_SENS()
{
  for (uint8_t i = 0; i < size; i++)
  {
    delete[] sensors[i];
  }

  delete[] sensors;
}

RET_TYPE MY_SENS::GetValue(uint8_t index)
{
  return sensors[index]->value;
}

uint8_t MY_SENS::GetCircuit(uint8_t index)
{
  return sensors[index]->param.circuit;
}

void MY_SENS::SetCircuit(uint8_t index, uint8_t circuit)
{
  if(index<size)
  {
    sensors[index]->param.circuit = circuit;
  }
}

const char* MY_SENS::GetName(uint8_t index)
{
  DEBUG_PRINT(F("name index:")); DEBUG_PRINTLN(index);
  return sensors[index]->settings->name;
}

uint8_t MY_SENS::GetType(uint8_t index)
{
  return sensors[index]->settings->type;
}

void MY_SENS::GetInfo(void *buf)
{
  for(uint8_t i = 0; i < size; i++)
  {
    // Выводим только сработавшие датчики
    if(sensors[i]->param.alarm)
    {
      sensors[i]->get_info(buf);
    }
  }
}

void MY_SENS::GetInfoAll(void *buf)
{
  for(uint8_t i = 0; i < size; i++)
  {
    sensors[i]->get_info(buf);
  }
}

void MY_SENS::Clear()
{
  for(uint8_t i=0;i<size;i++) sensors[i]->param.alarm=0;
}

void MY_SENS::SetEnable(uint8_t index, bool value)
{
  sensors[index]->param.enable = value;
}

bool MY_SENS::GetEnable(uint8_t index)
{
  return sensors[index]->param.enable;
}

void MY_SENS::SetSensValue(TEXT* tx, uint8_t cmd, uint8_t index=0)
{            
  i2c_sens_value sens;
  char* p = (char*)&sens;

  DEBUG_PRINT(F("cmd:")); DEBUG_PRINT(cmd); DEBUG_PRINT(F(" index:")); DEBUG_PRINTLN(index);

  sens.type = I2C_DATA;
  sens.cmd = cmd;
  if(cmd==I2C_SENS_NUM)
  {
    sens.index = size;
  }
  else if(cmd==I2C_SENS_VALUE)
  {
    if(index >= size) return;
    sens.index = index;
    sens.value = sensors[index]->value;
    sens.circuit = sensors[index]->param.circuit;
    sens.enable = sensors[index]->param.enable;

    DEBUG_PRINT(F(" index:")); DEBUG_PRINT(index); DEBUG_PRINT(F(" value:")); DEBUG_PRINTLN(sens.value); 
  }

  for(uint8_t i = 0; i < sizeof(i2c_sens_value); i++)
  {
    tx->AddChar(*(p+i));
  }
}

void MY_SENS::SensOpros(TEXT* tx)
{
  RET_TYPE value;

#if IR_ENABLE
  IRread();
#endif

#if RC_ENABLE
  rc_read();
#endif

#if RF_ENABLE
  nRFread();
#endif

  for(uint8_t i = 0; i < size; i++)
  {
    if(bitRead(circuit, sensors[i]->param.circuit))
    {
      value = sensors[i]->value;

      sensors[i]->opros();

      if(esp_flag_sens_update && value != sensors[i]->value)
      {
        SetSensValue(tx, I2C_SENS_VALUE, i);
      } 
    }
  }

#if IR_ENABLE
  IRresume();
#endif

#if RC_ENABLE
  rc_clear();
#endif
}
