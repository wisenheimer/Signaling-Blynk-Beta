
/* ================================================================================ */
/* ====================== Этот файл заполняется пользователем! ==================== */ 
/* ================================================================================ */

/* ================================================================================ */
/* ======== Базовые callback функции, вызываемые при срабатывании датчика. ======== */
/* ================================================================================ */

// Функция тревоги
void alarm_on()
{
  alarm_state = ALARM_VKL; // см. task.h
}

// Замыкание реле сирены
void sirena()
{
#if SIRENA_ENABLE
  // Включение сирены
  param.DTMF[0] = SIREN_ON_OFF;
#endif
  // Сброс датчиков
  sensors->Clear();
}

// Функции пультов управления

void guard_on()
{
  // Постановка на охрану
  param.DTMF[0] = GUARD_ON;
  // Сброс датчиков
  sensors->Clear();
}

void guard_off()
{
  // Снятие с охраны
  param.DTMF[0] = GUARD_OFF;
  // Сброс датчиков
  sensors->Clear();
}

/* ================================================================================ */
/* =============== Сюда добавляем пользовательские callback функции =============== */
/* ==================================  USER data ================================== */



/* ================================  End USER data ================================ */

/* ================================================================================ */
/* =============== Запись имён датчиков во флеш для экономии памяти. ============== */
/* ================================================================================ */
/* ============ Записываем отображаемые в сообщениях названия датчиков ============ */
/* ============ Имя строки (door, move и т.д.) должно быть уникальным. ============ */
/* ============ Оно передаётся в качестве второго параметра в table[]  ============ */
/* ================================================================================ */

/* ==================================  USER data ================================== */
/* ========================== Пишем имена своих датчиков ========================== */

const char door[] PROGMEM="Дверь";
const char move[] PROGMEM="Движение";
const char temp[] PROGMEM="Температура";

/* ================================  End USER data ================================ */

/* Вносим параметры датчиков путём заполнения структур типа SENSOR_INIT_PARAM,      */
/* которая описана в файле main_type.h                                              */
/* Создаём массив этих структур table, и сразу заполняем его данными внутри скобок  */
/* {} через запятую.                                                                */

SENSOR_INIT_PARAM table[] = {

/* ==================================  USER data ================================== */
/* Вписываем параметры наших датчиков                                               */
/* ================================================================================ */
/* Датчики типа DIGITAL_SENSOR и ANALOG_SENSOR
/* Параметры:
/* -------------------------------------------------------------------------------- */
/* | Тип, Имя, Начальное состояние пина (0 или 1), callback функция, Пин |          */
/* -------------------------------------------------------------------------------- */
  
  {DIGITAL_SENSOR, door, 1, alarm_on, DOOR_PIN},  // геркон
  {DIGITAL_SENSOR, move, 0, alarm_on, RADAR_PIN}, // датчик движения
  
/* -------------------------------------------------------------------------------- */
/* Датчики температуры типа DHT11, DHT21, DHT22, DS18B20 и TERMISTOR                */
/* -------------------------------------------------------------------------------- */
/* | Тип, Имя, Min температуры, Max температуры, callback функция, Пин |            */
/* -------------------------------------------------------------------------------- */
/* Для датчиков температуры задаётся верхний и нижний порог срабатывания            */
/* при помощи конструкции вида P(min,max),                                          */
/* где min - минимальный порог, ниже которого сработает датчик.                     */
/*     max - максимальный порог, превысив который сработает датчик.                 */

  {DS18B20, temp, P(10,50), alarm_on, DOOR_PIN},

/* если не пользоваться конструкцией P(min,max), а записать только одну цифру,      */
/* то это будет величина верхнего порога, а нижний порог будет равен нулю.          */ 

//  {DS18B20, temp, 50, alarm_on, DOOR_PIN},

/* -------------------------------------------------------------------------------- */
/* Беспроводные датчики типа RF24_SENSOR, IR_SENSOR и RC_SENSOR                     */
/* -------------------------------------------------------------------------------- */
/* | Тип, Имя, Код срабатывания, callback функция |                                 */
/* -------------------------------------------------------------------------------- */
/*  // Примеры:                                                                     */
/*  {RF24_SENSOR, okno, 1234567, alarm_on},                                         */
/*  {IR_SENSOR,   okno2, 7894561, alarm_on},                                        */
/*  {RC_SENSOR,   pir1, 9999999, alarm_on},                                         */
/*  // Выполнение команд с пульта управления                                        */
/*  // Пульт 1 кнопки 1-2                                                           */
/*  {RC_SENSOR, p1k1, 15435328, guard_on},  // на охрану                            */
/*  {RC_SENSOR, p1k2, 18435148, guard_off}, // с охраны                             */

/* ================================  End USER data ================================ */
};