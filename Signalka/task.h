
enum {
  NON,
  LED13_ON,
  LED13_OFF,
  ALARM_VKL,
  ALARM_VIKL
};

static uint8_t task_state = LED13_OFF; // Переменная состояния конечного автомата
static uint8_t alarm_state = NON;

// Период мигания сигнального светодиода на 13 пине в режиме Охрана, миллисекунд
#define LED_BLINK_PERIOD 3000
// Продолжительность тревоги в миллисекундах, после чего счётчики срабатываний обнуляются.
#define ALARM_MAX_TIME 60000
#define MainTimerIsExpired(startTime,interval) (millis()-startTime>interval)

void taskRun()
{
  // Переменные объявленные как static не исчезают после выхода из функции. А сохраняют свое состояние.  
  static uint32_t Delay;
  static uint32_t Delay2;
  static uint32_t LED_Delay;

  if ( MainTimerIsExpired(Delay, 500) )
  {
#if TM1637_ENABLE
    clock();
#endif
    /// Опрос датчиков ///
    sensors->SensOpros(param.tx);
    Delay = millis(); // Ставим задержку    
  }

  switch (task_state)
  {
    case LED13_ON:
      if ( MainTimerIsExpired(LED_Delay, LED_BLINK_PERIOD) )
      {
        digitalWrite(LED_BUILTIN, flags.GUARD_ENABLE);
        LED_Delay = millis();
        task_state = LED13_OFF;
      }
    break;

    case LED13_OFF:
      if ( MainTimerIsExpired(LED_Delay, 20) )
      {
        digitalWrite(LED_BUILTIN, LOW);
        LED_Delay = millis();
        task_state = LED13_ON;
      } 
    break;            
  }
          
  switch (alarm_state) {
      case ALARM_VKL:
          // Включаем режим тревоги.
          ALARM_ON;
          Delay2 = millis(); // Ставим задержку
          alarm_state = ALARM_VIKL;
        break; 
      case ALARM_VIKL:
          if(!flags.GUARD_ENABLE) Delay2 = 0;
          if ( !MainTimerIsExpired(Delay2, ALARM_MAX_TIME) ) break;
          ALARM_OFF;
          alarm_state = NON;
  }
}