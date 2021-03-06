#ifndef SETTINGS_H
#define SETTINGS_H

/*
 * В этом файле настройки осуществляются дефайнами
 * Для выбора настроки в дефайне указывают 1.
 * Для запрета указывают 0.
 */

//------------------------------------------------------------------------------
// скорость последовательного порта Serial
#define SERIAL_RATE	115200
// СМС на кирилице. Длина СМС не более 70 символов. Иначе латиницей до 160 символов.
#define PDU_ENABLE  1 // Включаем PDU-режим для отправки СМС на кирилице (требует больше ресурсов), иначе латиница (эконом вариант)
#define SMS_FORWARD 0 // пересылаем входящие смс админу
//------------------------------------------------------------------------------
// СИРЕНА
// Включает реле, к которому должна быть подключена сирена                
//----------------------------------------------------------
// Разрешение использования сирены
#define SIRENA_ENABLE 0

// Реле включения сирены
// тип реле
// 1 - включение единицей HIGH
// 0 - включение нулём LOW
#define SIREN_RELAY_TYPE 	0
// пин включения реле сирены
#define  SIREN_RELAY_PIN	9
//----------------------------------------------------------

#include "main_type.h"

//****************************************************
// Выбор способа получения отчётов (или GPRS, или SMS).
// Если разрешить оба флага GPRS_ENABLE и SMS_ENABLE,
// приоритет отдаётся GPRS. Если не удастся установить GPRS соединение, сообщения
// будут отправлены по SMS. Оба флага можно установить DTMF командами
// GPRS_ON_OFF и SMS_ON_OFF (см https://github.com/wisenheimer/Signaling-Blynk/blob/master/README.md).
// Отключить любой флаг можно, записав в него 0.
//****************************************************
#define OTCHET_INIT	flags.EMAIL_ENABLE=0;flags.SMS_ENABLE=1;flags.GUARD_ENABLE=0;
//****************************************************
//////////////////////////////////////////////////////////
/// Изменить параметры под себя
//////////////////////////////////////////////////////////
// Отправка почты
#define SMTP_SERVER					F("\"smtp-devices.yandex.com\",25") // почтовый сервер яндекс и порт
#define SMTP_USER_NAME_AND_PASSWORD	F("\"login\",\"password\"") // Логин и пароль от почты
#define SENDER_ADDRESS_AND_NAME		F("\"login@yandex.com\",\"SIM800L\"")
#define RCPT_ADDRESS_AND_NAME		F("\"login@mail.ru\",\"Alex\"") // Адрес и имя получателя
//#	define RCPT_CC_ADDRESS_AND_NAME		F("\"login@yandex.ru\",\"Ivan\"") // Адрес и имя получателя (копия)
//#	define RCPT_BB_ADDRESS_AND_NAME		F("\"login2@yandex.ru\",\"Ivan\"") // Адрес и имя получателя (вторая копия)

// Разрешаем спящий режим для экономии батареи при отключении электричества
#define SLEEP_MODE_ENABLE 0
//////////////////////////////////////////////////////////
// Подключаемые библиотеки.
// Для отключения библиотеки и экономии памяти 1 меняем на 0 и наоборот
//////////////////////////////////////////////////////////
#define BEEP_ENABLE   1 // пищалка, не работает совместно с IR_ENABLE
#define DHT_ENABLE    0 // библиотека датчиков DHT11, DHT21 и DHT22
#define TERM_ENABLE   0 // библиотека для термистора
#define DS18_ENABLE   1 // библиотека для датчика DS18B20
#define TM1637_ENABLE 1 // семисигментный индикатор для часов
//----------------------------------------------------------
// Эти две библиотеки можно включать только для Signalka.ino
//----------------------------------------------------------
#define IR_ENABLE 0 // библиотека для ик приёмника
#define RF_ENABLE 0 // библиотека для радио модуля RF24L01
#define RC_ENABLE 0 // беспроводной датчик 433 МГц
//----------------------------------------------------------
// Часы на 7-сегментном индикаторе TM1637
//----------------------------------------------------------
// Пины
#define CLK 7         
#define DIO 8
// часовой пояс. Для Москвы 3
#define GMT 3
// период синхронизации часов, мс
#define SYNC_TIME_PERIOD 20000

#endif // SETTINGS_H
