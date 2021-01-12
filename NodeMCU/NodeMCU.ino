/*************************************************************
  Download latest Blynk library here:
    https://github.com/blynkkk/blynk-library/releases/latest

  Blynk is a platform with iOS and Android apps to control
  Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build graphic interfaces for all your
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: http://www.blynk.cc
    Sketch generator:           http://examples.blynk.cc
    Blynk community:            http://community.blynk.cc
    Follow us:                  http://www.fb.com/blynkapp
                                http://twitter.com/blynk_app

  Blynk library is licensed under MIT license
  This example code is in public domain.

 *************************************************************
  This example runs directly on NodeMCU.

  Note: This requires ESP8266 support package:
    https://github.com/esp8266/Arduino

  Please be sure to select the right NodeMCU module
  in the Tools -> Board menu!

  For advanced settings please follow ESP examples :
   - ESP8266_Standalone_Manual_IP.ino
   - ESP8266_Standalone_SmartConfig.ino
   - ESP8266_Standalone_SSL.ino

  Change WiFi ssid, pass, and Blynk auth token to run :)
  Feel free to apply it to any other example. It's simple!

  Wi-Fi модем Blynk.
 * 
 * Для включения/отключения вывода отладочных сообщений
 * в Serial отредактируйте файл
 * libraries/main_type/settings.h
 *
 * Пример работы с WiFiManager
 * https://github.com/tzapu/WiFiManager/blob/master/examples/AutoConnectWithFSparamseters/AutoConnectWithFSparamseters.ino
 *
 * Прошивать как есть
 * @Author: wisenheimer
 * @Date:   2019-04-17 8:30:00
 * @Last Modified by: wisenheimer 
 * @Last Modified time: 2019-09-05 16:00:00
 *************************************************************/

/* Comment this out to disable prints and save space */

#define BLYNK_PRINT Serial

#include <EEPROM.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiUdp.h>
#include "settings.h"
#include "audio.h"
//#include "task_schedule.h"

//#include "ESP8266WebServer.h"           // Содержится в пакете.
#include "DNSServer.h"                  // Содержится в пакете.
#include "WiFiManager.h"                // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson

// Виртуальные пины виджетов
#define TERMINAL_PIN  V0
#define TABLE_PIN     V1
#define ALARM_PIN     V2
#define PLAYER_PIN    V3
#define VOLUME_PIN    V4
#define LCD_PIN       V5
#define TAB_PIN       V7
#define GPS_BTN_PIN   V8
#define KONTUR0_PIN   V9
#define KONTUR1_PIN   V10
#define GPS_PIN       V11
#define GUARD_PIN     V12
#define MONO_PIN      V14
#define BASS_PIN      V15
#define FIND_PIN      V16
#define MUTE_PIN      V17

#define TAB_ALL 1
#define TAB_C0 2
#define TAB_C1 3
#define TAB_SETTINGS 4

#define EMAIL(text) if(flag[EMAIL_ENABLE].value)Blynk.email("","ESP8266",text)

#define READ_COM_FIND(str) strstr(in_buffer,str)

#define FLAGS_DELETE {for(uint8_t i=0;i<params.flags_num;i++)free(flag[i].name);delete[] flag;params.flags_num=0;}

#define PRINT_IN_BUFFER(str)  Serial.println();Serial.print(str);Serial.print(in_buffer);for(uint8_t i=0;i<in_index;i++){\
                              Serial.print(' ');Serial.print(*(in_buffer+i),HEX);}Serial.println();

struct saved_data{
  char token[33]; // токен Blynk
  uint32_t enable; // включенные датчики
  uint16_t circuit:CIRCUIT_NUM; // включенные контура охраны
  uint8_t radio_freq_index; 
} saved_data;

// Отображаемые данные во вкладке
struct view
{
  uint8_t circuit:CIRCUIT_NUM; // 2 контура
  uint8_t index:3; // до 7 вкладок
} tab;

FLAG*   flag;
DATCHIK* sensor;

struct values
{
  uint8_t sens_num:5; // Число датчиков
  uint8_t flags_num:3; // Число флагов
  uint8_t gps_enable:1; // вкл/выкл охрану по GPS
} params;

bool flag_set_circuit = false;

// Attach virtual serial terminal to Virtual Pin
WidgetTerminal terminal(TERMINAL_PIN);

WidgetTable table;

BLYNK_ATTACH_WIDGET(table, TABLE_PIN);

//#include "music.h"
#include "table.h"
#include "espI2C.h"

uint8_t crc_update(uint8_t crc, uint8_t data) {   // Процедура обновления CRC
  uint8_t i = 8;                          // Используем полином CRC8 1-Wire
  while (i--) {                            
    crc = ((crc ^ data) & 1) ? (crc >> 1) ^ 0b10001100 : (crc >> 1); 
    data >>= 1;
  }
  return crc;
}


void save_data()
{
  static uint8_t saved_data_crc = 0;

  uint8_t* p = (uint8_t*)&saved_data;
  uint8_t crc = 0;

  for(uint8_t i = 0; i < sizeof(saved_data); i++)
  {
    crc = crc_update(crc, p[i]);
  }
  
  if(saved_data_crc != crc)
  {
    EEPROM.put(0,saved_data);

    if (EEPROM.commit())
    {
      DEBUG_PRINTLN(F("Settings saved"));
      saved_data_crc = crc;
    }
    else
    {
      DEBUG_PRINTLN(F("EEPROM error"));
    }
  }
}

BLYNK_WRITE(TAB_PIN) {

  Serial.printf("Item %d selected\n", param.asInt());

  TABLE_CLEAR
  flag_set_circuit = false;

  memset(&tab, 0, sizeof(tab));

  tab.index = param.asInt();

  switch (tab.index) {
    case TAB_ALL: // Всё
      table_add_flags();
      //bitSet(tab.circuit,0);
      //bitSet(tab.circuit,1);
      tab.circuit = 0x03;
      break;
    case TAB_C0: // Только 0 контур
    case TAB_C1: // Только 1 контур
      // Устанавливаем бит маски контуров
      bitSet(tab.circuit,tab.index-TAB_C0);
      break;
    default: // Настройка контуров
      flag_set_circuit = true;
      for(uint8_t i=0; i<params.sens_num; i++)
      {
        sensor[i].value = sensor[i].circuit;
      }
      // Печатаем шапку
      TABLE_ADD_ROW(SENSOR, KONTUR, 1)  
  }  
  table_add_circuit(tab.circuit);
}

uint8_t get_flags()
{
  uint8_t tmp = 0;

  for(uint8_t i=0; i<params.flags_num; i++)
  {
    if(flag[i].value) tmp |= 1 << i;
  }

  return tmp;
}

BLYNK_WRITE(GPS_PIN) {

  // вышли из зоны / вошли в зону охраны
  if(params.gps_enable)
  {
    // Включаем/отключаем охрану
    Blynk.virtualWrite(GUARD_PIN, param.asInt());
  }
}

BLYNK_WRITE(GPS_BTN_PIN) {
  params.gps_enable = param.asInt();
}

BLYNK_WRITE(GUARD_PIN) {
  
  // Включаем/отключаем охрану
  flag[2].value = param.asInt();

  i2c_send_command(get_flags(),I2C_FLAGS);  
}

/*
 * Храним параметр enable датчиков в EEPROM
 * При включении/отключении контуров изменяем параметр enable
 * только в памяти ардуины.
*/
void set_enable(uint8_t circuit, bool State)
{
  uint16_t tmp = sensors_get_enable(circuit);

  State ? saved_data.enable|=tmp : saved_data.enable^=tmp;   
  
  i2c_send_command(saved_data.enable, I2C_SENS_ENABLE);

  State?bitSet(saved_data.circuit,circuit):bitClear(saved_data.circuit,circuit);
}

//как только подключились
BLYNK_CONNECTED() {
  uint8_t pins[] = {
    GPS_BTN_PIN, TAB_PIN
  //  GPS_BTN_PIN, TAB_PIN, MONO_PIN, BASS_PIN, MUTE_PIN, VOLUME_PIN
  };

  for(uint8_t i = 0; i < sizeof(pins); i++)
  {
    //запросить информацию у сервера о состоянии пина
    Blynk.syncVirtual(pins[i]);
    ESP.wdtFeed();   // пнуть собаку
    yield();  // ещё раз пнуть собаку
  }
}

void set_circuit_enable(uint8_t circuit, bool State)
{
    State ? bitSet(saved_data.circuit,circuit) : bitClear(saved_data.circuit,circuit);
       
    i2c_send_command(saved_data.circuit,I2C_MASK_CIRCUIT);
}

//этот метод будет вызван после ответа сервера 
BLYNK_WRITE(KONTUR0_PIN) {
  
  set_circuit_enable(0, param.asInt());
}

BLYNK_WRITE(KONTUR1_PIN) {
  
  set_circuit_enable(1, param.asInt());
}

// ---- Синхронизация часов с сервером NTP
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov"; //"ru.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
unsigned int localPort = 2390;      // local port to listen for UDP packets
time_t epoch = 0; // время

BlynkTimer timer;

char* printTime(char* buf, uint8_t size)
{
  time_t rawtime = epoch;
  struct tm  ts;

  // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
  ts = *localtime(&rawtime);
  strftime(buf, size, "%H:%M:%S", &ts);
  return buf;
}

void myTime()
{
	epoch++; // прибавляем секунду ко времени
//  guard.run(epoch); // Проверяем расписание
  if(epoch%60 == 0)
  {
    save_data();
  }
}

void myTimerEvent()
{
  static uint8_t i = 0;
  uint8_t res = 0;
  // Запрашиваем данные в Ардуино
  do
  {
    if(Wire.requestFrom(I2C_ARDUINO_ADDR, 32)) res = i2c_read();    
  } while(res);

  if(!params.sens_num) i2c_send_command(0,I2C_SENS_NUM);
  else
  {
    if(i>=params.sens_num) i=0;
    DEBUG_PRINT(F("sens_num="));DEBUG_PRINT(params.sens_num);DEBUG_PRINT(F(" i="));DEBUG_PRINTLN(i);
    if(sensor[i].name == NULL) i2c_send_command(i,I2C_SENS_NAME);
    else i2c_send_command(i,I2C_SENS_VALUE);
    i++;
  }

  if(params.flags_num<2 || flag[0].name == NULL)
  {
    FLAGS_DELETE
    TABLE_CLEAR
    i2c_send_command(0,I2C_FLAG_NAMES);
  }  
}

// таймер синхронизации времени
void syncTimerEvent()
{
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP); 

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  
  int cb = udp.parsePacket();
  if (!cb) {
    DEBUG_PRINTLN(F("no packet yet"));
  }
  else {
    DEBUG_PRINT(F("packet received, length="));
    DEBUG_PRINTLN(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    DEBUG_PRINT(F("Seconds since Jan 1 1900 = " ));
    DEBUG_PRINTLN(secsSince1900);

    // now convert NTP time into everyday time:
    DEBUG_PRINT(F("Unix time = "));
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);
    
    // корректировка часового пояса и синхронизация 
    epoch = epoch + GMT * 3600;
      
   // настройка времени часов модема SIM800L
    // AT+CCLK="19/08/27,10:01:00+03"
    // print the hour, minute and second:
    DEBUG_PRINT(F("The UTC time is "));       // UTC is the time at Greenwich Meridian (GMT)

    time_t rawtime = epoch;
    struct tm ts;
    char buf[80];

    // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    ts = *localtime(&rawtime);
    strftime(buf, sizeof(buf), "AT+CCLK=\"%y/%m/%d,%H:%M:%S+", &ts);
    if(GMT < 10)
    {
      strcat(buf,"0");
    }
    itoa(GMT,buf+strlen(buf),10);
    strcat(buf,"\"\n");
    Serial.println(buf);
    
    I2C_WRITE(buf,strlen(buf));
  }
  // wait ten seconds before asking for the time again
}

// You can send commands from Terminal to your hardware. Just use
// the same Virtual Pin as your Terminal Widget
BLYNK_WRITE(TERMINAL_PIN)
{
  if(strstr((char*)param.getBuffer(), "#") != NULL)
  { // Это DTMF команда
    char* p = (char*)param.getBuffer();
    uint8_t i = 0;
    char buf[4];
    while(isdigit(*p) && i < 3) buf[i++] = *p++;
    buf[i] = 0;
    if(i)
    {
      i = atoi(buf);
      i2c_send_command(i,I2C_DTMF);
    }    
  }
  else
  {
    I2C_WRITE((char*)param.getBuffer(), param.getLength());
  }
}

void setup()
{

  memset(&params, 0x00, sizeof(values));

  // Debug console
	Serial.begin(SERIAL_RATE);

  // commit 512 bytes of ESP8266 flash (for "EEPROM" emulation)
  // this step actually loads the content (512 bytes) of flash into 
  // a 512-byte-array cache in RAM

  EEPROM.begin(sizeof(saved_data));
  EEPROM.get(0,saved_data);
    
  delay(2000);

  WiFiManager wifiManager;

  //strncpy(saved_data.token, custom_blynk_token.getValue(), sizeof(saved_data.token));

  // Попадаем сюда при удержании кнопки FLASH во время старта
  if(digitalRead(D3)==LOW)
  {
    // Выодим данные Wi-Fi точки и токен Blynk 
    // The extra paramseters to be configured (can be either global or just in the setup)
    // After connecting, paramseter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_blynk_token("blynk", "blynk token", saved_data.token, sizeof(saved_data.token));  
 
    //add all your paramseters here
    wifiManager.addParameter(&custom_blynk_token);

    //SSID & password paramseters already included
    wifiManager.startConfigPortal();

    strncpy(saved_data.token, custom_blynk_token.getValue(), sizeof(saved_data.token));
  }

  DEBUG_PRINTLN("\nSSID: "+wifiManager.getWiFiSSID()+"\nSSID2: "+wifiManager.getWiFiSSID(true)+"\nPASS: "+wifiManager.getWiFiPass()+"\nPASS2: "+wifiManager.getWiFiPass(true)+"\nTOKEN: "+String(saved_data.token));

  Blynk.begin(saved_data.token, wifiManager.getWiFiSSID().c_str(), wifiManager.getWiFiPass().c_str());

  // This will print Blynk Software version to the Terminal Widget when
  // your hardware gets connected to Blynk Server
 	terminal.println("Blynk v" BLYNK_VERSION ": Device started");

  radio_setup();

  // Setup a function to be called every second
	timer.setInterval(2000L, myTimerEvent);
	timer.setInterval(SYNC_TIME_PERIOD, syncTimerEvent);
	timer.setInterval(1000, myTime);

  table_init();

	terminal.print(F("Ready "));
	terminal.print(F("IP address: "));
	terminal.println(WiFi.localIP());
	terminal.flush();

  //clean table at start
  TABLE_CLEAR

  DEBUG_PRINTLN(F("Starting UDP"));
  udp.begin(localPort);
  DEBUG_PRINT(F("Local port: "));
  DEBUG_PRINTLN(udp.localPort());

  // синхронизируем время при загрузке
  syncTimerEvent();
    
//  int count = sizeof(audio_files)/sizeof(char*);
//  Serial.printf("%d music files finded\n", count);

/*
  BlynkParamAllocated items(count); // list length, in bytes
  for(int i = 0; i < count; i++) items.add(audio_files[i]);
*/  
  //items.add("OFF");
/*  items.add("New item 1");
    items.add("New item 2");
    items.add("New item 3");
    items.add("New item 4");
    items.add("New item 5");
    items.add("New item 6");
*/
	//Blynk.setProperty(MUSIC_PIN, "labels", items);

  
  //Blynk.setProperty(MUSIC_PIN, "labels", audio_files[0], audio_files[1], audio_files[2], audio_files[3], audio_files[4], "выкл");

  //Blynk.setProperty(MUSIC_PIN, "Sound", "OFF", "Сирена");
//  mp3_init();
//  mp3_file_load(audio_files[0]);
}

void loop()
{
	Blynk.run();
	timer.run(); // Initiates BlynkTimer
  radio_run();
// 	mp3_run();
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  DEBUG_PRINTLN(F("sending NTP packet..."));
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
} 

