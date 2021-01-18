/*
 * Модуль часов на 7сигментном индикаторе TM1637
 * @Author: wisenheimer
 * @Date:   2019-05-28 10:00:00
 * @Last Modified by:  
 * @Last Modified time:
 */

#include "settings.h" 

#if GYVER_TM1637_LIB
  #include "GyverTM1637.h"
  GyverTM1637 tm1637(CLK, DIO);
#else
  #include "TM1637.h"  // http://www.seeedstudio.com/wiki/File:DigitalTube.zip
  TM1637 tm1637(CLK,DIO);  
#endif
  
struct clock_time
{
  uint8_t second:6;
  uint8_t minute:6;
  uint8_t hour:5;
  uint8_t point:1;
  uint8_t brightness:3; // яркость, от 0 до 7 
} my_time;

void clock_set(byte time[])
{
  my_time.hour = time[0];
  my_time.minute = time[1];
  my_time.second = time[2];
  time = millis();
}

void clock_init() {
  my_time.brightness = BRIGHT_TYPICAL;
  
#if GYVER_TM1637_LIB
  tm1637.clear();
  tm1637.brightness(my_time.brightness);  // яркость, 0 - 7 (минимум - максимум)
#else
  tm1637.init();
  tm1637.set(my_time.brightness);
#endif    
}

void clock()
{
  static uint32_t set_time = 0;
  
  int8_t TimeDisp[4];
  uint32_t i;
  
  for(i = set_time; i < millis(); i+=1000)
  {
    my_time.second++; 
    if (my_time.second > 59)
    {
      my_time.second = 0;
      my_time.minute++;
    }
    if (my_time.minute > 59)
    {
      my_time.minute = 0;
      my_time.hour++;
    }
    if (my_time.hour > 23)
    {
      my_time.hour = 0;
    }
  }

  set_time = i;

  my_time.point = !my_time.point;
  tm1637.point(my_time.point);

  // забиваем массив значениями для отпарвки на экран
    
  TimeDisp[0] = my_time.hour / 10;
  TimeDisp[1] = my_time.hour % 10;
  TimeDisp[2] = my_time.minute / 10;
  TimeDisp[3] = my_time.minute % 10;

  // отправляем массив на экран
  tm1637.display(TimeDisp);
}