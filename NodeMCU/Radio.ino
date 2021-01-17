///
/// \file LCDRadio.ino 
/// \brief Radio implementation including LCD output.
/// 
/// \author Matthias Hertel, http://www.mathertel.de
/// \copyright Copyright (c) 2014 by Matthias Hertel.\n
/// This work is licensed under a BSD style license.\n
/// See http://www.mathertel.de/License.aspx
///
/// \details
/// This is a full function radio implementation that uses a LCD display to show the current station information.\n
/// It can be used with various chips after adjusting the radio object definition.\n
/// Open the Serial console with 57600 baud to see current radio information and change various settings.
///
/// Wiring
/// ------
/// The necessary wiring of the various chips are described in the Testxxx example sketches.
/// The boards have to be connected by using the following connections:
/// 
/// Arduino port | SI4703 signal | RDA5807M signal
/// :----------: | :-----------: | :-------------:
/// GND (black)  | GND           | GND   
/// 3.3V (red)   | VCC           | VCC
/// 5V (red)     | -             | -
/// A5 (yellow)  | SCLK          | SCLK
/// A4 (blue)    | SDIO          | SDIO
/// D2           | RST           | -
///
/// More documentation and source code is available at http://www.mathertel.de/Arduino

#include <RDA5807M.h>
//#include "RDA5807M_RDS.h"
#include <RDSParser.h>

// Define some stations available at your locations here:
// 89.40 MHz as 8940
// Пользовательские частоты радиостанций
RADIO_FREQ preset[] = {
  9490, 
  9630,
  9770,
  9810,
  9850,
  9910,
  9950,
  9990,
  10030,
  10070,
  10110,
  10160,
  10230,
  10280,
  10340,
  10380,
  10430,
  10480,
  10530,
  10570,
  10610,
  10680,
  10760
};

static uint8_t  i_sidx;        // Start at Station with index=5

static bool flag_play;

RDA5807M radio;    // Create an instance of a RDA5807 chip radio
/// get a RDS parser
RDSParser rds;

WidgetLCD lcd(LCD_PIN);

BLYNK_WRITE(MONO_PIN) {

  if(params.radio_enable) radio.setMono(param.asInt());
}

BLYNK_WRITE(BASS_PIN) {

  if(params.radio_enable) radio.setBassBoost(param.asInt());
}

BLYNK_WRITE(FIND_PIN) {

  if(params.radio_enable && param.asInt())
  {
    radio.seekUp(flag_play);
  }
}

BLYNK_WRITE(MUTE_PIN) {

  if(params.radio_enable) radio.setSoftMute(param.asInt());
}

BLYNK_WRITE(PLAYER_PIN)
{
  static int16_t value;
  String action = param.asStr();

  DEBUG_PRINTLN(action);
  lcd.print(0, 1, "                ");
  lcd.print(0, 1, action);

  if(params.radio_enable)
  {
    if (action == "stop")
    {    
      if(flag_play)
      {
        flag_play = false;
        radio.term();  
      }    
    } 
    else
    {
      if (action == "next") i_sidx++;

      if (i_sidx  >= sizeof(preset) / sizeof(RADIO_FREQ)) i_sidx = 0;

      if (action == "prev")
      {
        if(!i_sidx) i_sidx = sizeof(preset) / sizeof(RADIO_FREQ);
        i_sidx--;
      } 
           
      radio.setFrequency(preset[i_sidx]);
      flag_play = true;
      saved_data.radio_freq_index = i_sidx;
    }
  } 
}

// - - - - - - - - - - - - - - - - - - - - - - - - - -

static uint16_t g_block1;

// use a function in between the radio chip and the RDS parser
// to catch the block1 value (used for sender identification)
void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4)
{
  // Serial.printf("RDS: 0x%04x 0x%04x 0x%04x 0x%04x\n", block1, block2, block3, block4);
  g_block1 = block1;
  rds.processData(block1, block2, block3, block4);
}

/// Update the Time
void DisplayTime(uint8_t hour, uint8_t minute)
{
  Serial.print("Time: ");
  if (hour < 10)
    Serial.print('0');
  Serial.print(hour);
  Serial.print(':');
  if (minute < 10)
    Serial.print('0');
  Serial.println(minute);
} // DisplayTime()


/// Update the ServiceName text on the LCD display.
void DisplayServiceName(char *name)
{
  bool found = false;

  for (uint8_t n = 0; n < 8; n++)
    if (name[n] != ' ')
      found = true;

  if (found) {
    Serial.print("Sender:<");
    Serial.print(name);
    Serial.println('>');
    lcd.print(0,1,name);
  }
} // DisplayServiceName()


/// Update the ServiceName text on the LCD display.
void DisplayText(char *txt)
{
  Serial.print("Text: <");
  Serial.print(txt);
  Serial.println('>');
  //lcd.print(0,1,txt);
} // DisplayText()


/// Setup a FM only radio configuration with I/O for commands and debugging on the Serial port.
void radio_setup() {
  // Initialize the lcd
  lcd.clear();

  // Enable information to the Serial port
  flag_play = false;
  //radio.debugEnable();
  params.radio_enable=radio.init();
  if(params.radio_enable)
  {
    radio.setBand(RADIO_BAND_FM);

    // Set all radio setting to the fixed values.

    i_sidx = saved_data.radio_freq_index;
    if (i_sidx >= sizeof(preset) / sizeof(RADIO_FREQ))
    {
      i_sidx = 0;  
    }

    // setup the information chain for RDS data.
    radio.attachReceiveRDS(RDS_process);
    rds.attachServicenNameCallback(DisplayServiceName);
    rds.attachTextCallback(DisplayText);
    rds.attachTimeCallback(DisplayTime);
    
    Blynk.virtualWrite(PLAYER_PIN, "stop");
  }
} // Setup

BLYNK_WRITE(VOLUME_PIN) {
  char c[16];
  
  if(params.radio_enable) radio.setVolume(param.asInt());
  memset(c, '*', param.asInt()+1);
  memset(c+param.asInt()+1, ' ', 15-param.asInt());
  lcd.print(0, 1, c);
}

/// Constantly check for serial input commands and trigger command execution.
void radio_run() {
  static unsigned long RDSCheckTime = 0;
  static unsigned long nextRadioInfoTime = 0;  
  static uint8_t IDRepeatCounter = 0;
  static uint16_t ID = 0;
  static uint16_t MaybeThisIDIsReal = 0;
  static uint8_t rssi;  

  if(params.radio_enable && flag_play)
  {
    unsigned long now = millis();

    if (now >= RDSCheckTime)
    { // Вывод названия старнции на экран
      // Пора проверить флаг RDSR
      radio.checkRDS();

      RDSCheckTime = now + 30;
    }

    if (now >= nextRadioInfoTime)
    {
      RADIO_INFO info;
      radio.getRadioInfo(&info);

      if(rssi != info.rssi)
      {
        rssi = info.rssi;

        char s[12];
        radio.formatFrequency(s, sizeof(s));
        Serial.print(F("FREQ:")); Serial.println(s);
        lcd.clear();
        lcd.print(0, 0, s);
        lcd.print(14,0,rssi);  
      }      
      nextRadioInfoTime = now + 1000;
    } // update
  }
} // loop

// End.
