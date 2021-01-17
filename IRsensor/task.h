#include <IRremote.h>
#include "settings.h"

IRsend irsend;
static bool alarm_state = false;

#include "USER_sensors.h"

#define MainTimerIsExpired(startTime,interval) (millis()-startTime>interval)

// конечный автомат
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
      irsend.sendNEC(IR_CODE, 32); // 32 - размер слова в битах
      DELAY(1000);
    }          
    alarm_state = false;
  }
}