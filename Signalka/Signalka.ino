/*
 * Прошивка для ардуино сигнализации.
 * !!! В этот файл не вносим никаких изменений!!!
 * Перед прошивкой настроить файлы
 * USER_sensors.h и
 * libraries/main_type/settings.h
 *
 * @Author: wisenheimer
 * @Date:   2019-03-29 12:30:00
 * @Last Modified by: wisenheimer  
 * @Last Modified time: 2021-01-13 17:00:00
 */

#include "modem.h"

// Дефайн задания порогов срабатывания для датчиков температуры
#define P(min,max) (((int32_t)min << 16)|max)

extern FLAGS flags;
extern COMMON_OP param;

MODEM *modem;
MY_SENS *sensors = NULL;

#include "task.h"
#include "USER_sensors.h"

void setup() {

  // наш личный "setup"
  PINS_INIT // см. main_type.h

  sensors = new MY_SENS(table, sizeof(table)/sizeof(SENSOR_INIT_PARAM));

#if TM1637_ENABLE
  clock_init();
#endif

  modem = new MODEM();

  modem->init();

  DEBUG_PRINT(F("memoryFree=")); DEBUG_PRINTLN(memoryFree()); 

//  DEBUG_PRINT(F("sizeof(sens):")); DEBUG_PRINTLN(sizeof(sens));
//  DEBUG_PRINT(F("sizeof(Sensor):")); DEBUG_PRINTLN(sizeof(Sensor));
//  DEBUG_PRINT(F("/:")); DEBUG_PRINTLN(sizeof(sens)/sizeof(Sensor));
  //DEBUG_PRINT(F("sens num:")); DEBUG_PRINTLN(sensors->size);
}

void loop()
{
  modem->wiring(); // слушаем модем
  taskRun(); 
}