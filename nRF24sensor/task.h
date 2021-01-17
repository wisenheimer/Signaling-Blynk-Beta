#include <SPI.h>
#include "nRF24.h"
#include "settings.h"

static bool alarm_state = false;

#include "USER_sensors.h"

#define MainTimerIsExpired(startTime,interval) (millis()-startTime>interval)

void taskRun()
{
  // Переменные объявленные как static не исчезают после выхода из функции. А сохраняют свое состояние.  
  static uint32_t Delay = 1000;

  if ( MainTimerIsExpired(Delay, 500) )
  {
    /// Опрос датчиков ///
    sensors->SensOpros(NULL);
    Delay = millis(); // Ставим задержку
  }

  if (alarm_state)
  {
    // Включаем режим тревоги.
    for(uint8_t i = 0; i < 10; i++);
    {
      // Режим тревога вкл. Отправка кодового слова.
      nRFwrite(RF_CODE); 
      DELAY(1000);
    }          
    alarm_state = false;
  }
}