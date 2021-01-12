
enum {
  NON,
  SENS_OPROS,
  LED13_OFF,
  ALARM_VKL,
  ALARM_VIKL
};

static uint8_t task_state = NON; // Переменная состояния конечного автомата
static uint8_t alarm_state = NON;

#define MainTimerIsExpired(startTime,interval) (millis()-startTime>interval)

// конечный автомат
void taskRun()
{
  // Переменные объявленные как static не исчезают после выхода из функции. А сохраняют свое состояние.  
  static uint32_t Delay;
  static uint32_t Delay2;

  switch (task_state) {
      case SENS_OPROS:
          if ( !MainTimerIsExpired(Delay, 980) ) break; // Если 1с не прошла - сразу выходим
#if TM1637_ENABLE
          clock();
#endif
          /// Опрос датчиков ///
          sensors->SensOpros(param.tx);
          Delay = millis(); // Ставим задержку
          if(!flags.GUARD_ENABLE) break;
          // мигание светодиодом при постановке на охрану
          digitalWrite(LED_BUILTIN, HIGH);        
          task_state = LED13_OFF;      
        break;
      case LED13_OFF:
          if ( !MainTimerIsExpired(Delay, 20) ) break;
          digitalWrite(LED_BUILTIN, LOW); 
      default:
        task_state = SENS_OPROS;
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
      break;
      default:
        alarm_state = NON;
  }
}