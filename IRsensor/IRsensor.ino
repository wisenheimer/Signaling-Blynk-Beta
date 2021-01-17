/*
 * Беспроводной ИК датчик.
 * IRsensor: sending IR codes with IRsend
 * An IR LED must be connected to Arduino PWM pin 3.
 *
 * Перед прошивкой настроить файлы
 * USER_sensors.h и
 * libraries/main_type/settings.h
 *
 * @Author: wisenheimer
 * @Date:   2019-04-17 8:30:00
 * @Last Modified by: wisenheimer
 * @Last Modified time: 2021-01-17 21:00:00
 */

#include <my_sensors.h>

MY_SENS *sensors = NULL;

#include "task.h"

void setup()
{
  sensors = new MY_SENS(table, sizeof(table)/sizeof(SENSOR_INIT_PARAM)); 
}

void loop()
{
  taskRun();    
}
