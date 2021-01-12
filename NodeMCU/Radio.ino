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
///
/// History:
/// --------
/// * 05.08.2014 created.
/// * 06.10.2014 working.

#include <RDA5807M.h>
#include "RDA5807M_RDS.h"

// Define some stations available at your locations here:
// 89.40 MHz as 8940

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
// , 10650,10650,10800

static uint8_t  i_sidx;        // Start at Station with index=5

static bool flag_play;

RDA5807M radio;    // Create an instance of a RDA5807 chip radio

WidgetLCD lcd(LCD_PIN);

BLYNK_WRITE(MONO_PIN) {

  radio.setMono(param.asInt());
}

BLYNK_WRITE(BASS_PIN) {

  radio.setBassBoost(param.asInt());
}

BLYNK_WRITE(FIND_PIN) {

  if(param.asInt())
  {
    if(!flag_play) flag_play = radio.init();
    radio.seekUp(flag_play);
  }
}

BLYNK_WRITE(MUTE_PIN) {

  radio.setSoftMute(param.asInt());
}

BLYNK_WRITE(PLAYER_PIN)
{
  static int16_t value;
  String action = param.asStr();

  DEBUG_PRINTLN(action);
  lcd.print(0,1,action);

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
    if (action == "next")
    {
      i_sidx++;
      if (i_sidx  == sizeof(preset) / sizeof(RADIO_FREQ)) i_sidx = 0;
    }
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

/// Setup a FM only radio configuration with I/O for commands and debugging on the Serial port.
void radio_setup() {
  // Initialize the lcd
  lcd.clear();

  // Enable information to the Serial port
  radio.debugEnable();
  radio.init();
  radio.setBand(RADIO_BAND_FM);

  // Set all radio setting to the fixed values.
  //radio.setVolume(4);

  i_sidx = saved_data.radio_freq_index;
  if (i_sidx >= sizeof(preset) / sizeof(RADIO_FREQ))
  {
    i_sidx = 0;  
  }

  flag_play = false;
  Blynk.virtualWrite(PLAYER_PIN, "stop");
} // Setup

BLYNK_WRITE(VOLUME_PIN) {
  char c[2]={0,0};
  radio.setVolume(param.asInt());
  for(uint8_t i = 0; i < 15; i++)
  {
    i<=param.asInt()? c[0]='*':c[0]=' ';
    lcd.print(i, 1, c);
  }
}

uint16_t getRegister(uint8_t reg) {
  uint16_t result;
  Wire.beginTransmission(RDA5807M_RANDOM_ACCESS_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(RDA5807M_RANDOM_ACCESS_ADDRESS, 2, true);
  result = (uint16_t)Wire.read() << 8;
  result |= Wire.read();
  return result;
}

#define REPEATS_TO_BE_REAL_ID 3

/// Constantly check for serial input commands and trigger command execution.
void radio_run() {
  static bool RDSR_prev = false; // Предыдущее значение RDSR
  static char PSName[9]; // Значение PSName
  static char PSName_prev[9] = "        ";
  static uint8_t PSNameUpdated = 0; // Для отслеживания изменений в PSName
  static unsigned long nextRadioInfoTime = 0;
  static unsigned long RDSCheckTime = 0;
  static uint8_t IDRepeatCounter = 0;
  static uint16_t ID = 0;
  static uint16_t MaybeThisIDIsReal = 0;
  uint16_t reg0Bh, reg10h;
  uint16_t blockA, blockB, blockD;
  uint8_t errLevelB, errLevelD, groupType;
  bool groupVer;

  // some internal static values for parsing the input
  static uint8_t rssi;  

  if(flag_play)
  {
    unsigned long now = millis();

    if (now >= RDSCheckTime)
    { // Вывод названия старнции на экран
      // Пора проверить флаг RDSR
      reg0Ah = getRegister(RDA5807M_REG_STATUS1);
      if ((reg0Ah & RDA5807M_FLAG_RDSR) and (!RDSR_prev))
      { // RDSR изменился с 0 на 1
        // Сравним содержимое блока A (ID станции) с предыдущим значением
        blockA = getRegister(RDA5807M_REG_BLOCK_A);
        if (blockA == MaybeThisIDIsReal)
        {
          if (IDRepeatCounter < REPEATS_TO_BE_REAL_ID)
          {
            IDRepeatCounter++; // Значения совпадают, отразим это в счетчике
            if (IDRepeatCounter == REPEATS_TO_BE_REAL_ID)
              ID = MaybeThisIDIsReal; // Определились с ID станции
          }
        }
        else
        {
          IDRepeatCounter = 0; // Значения не совпадают, считаем заново
          MaybeThisIDIsReal = blockA;
        }
        if (ID != 0 && blockA == ID)
        { // ID станции не скачет, вероятность корректности группы в целом выше
          reg0Bh = getRegister(RDA5807M_REG_STATUS2);
          errLevelB = reg0Bh & 0x0003;//RDA5807M_BLERB_MASK;
          if (errLevelB < 3)
          { // Блок B корректный, можем определить тип и версию группы
            blockB = getRegister(RDA5807M_REG_BLOCK_B);
            groupType = (blockB & RDS_ALL_GROUPTYPE_MASK) >> RDS_ALL_GROUPTYPE_SHIFT;
            groupVer = (blockB & RDS_ALL_GROUPVER) > 0;
          
            reg10h = getRegister(RDA5807M_REG_BLER_CD);
            errLevelD = (reg10h & RDA5807M_BLERD_MASK) >> RDA5807M_BLERD_SHIFT;
  
            // ************* 0A, 0B - PSName, PTY ************
            if ((groupType == 0) && (errLevelD < 3))
            {
              blockD = getRegister(RDA5807M_REG_BLOCK_D);
              // Сравним новые символы PSName со старыми:
              char c = uint8_t(blockD >> 8); // новый символ
              blockB &= 0x0003;
              uint8_t i = blockB << 1; // его позиция в PSName
              if (PSName[i] != c)
              { // символы различаются
                PSNameUpdated &= !(1 << i); // сбросим флаг в PSNameUpdated
                PSName[i] = c;
              }
              else // символы совпадают, установим флаг в PSNameUpdated:
                PSNameUpdated |= 1 << i;
              // Аналогично для второго символа
              c = uint8_t(blockD & 255);
              i++;
              if (PSName[i] != c)
              {
                PSNameUpdated &= !(1 << i);
                PSName[i] = c;
              }
              else
                PSNameUpdated |= 1 << i;
              // Когда все 8 флагов в PSNameUpdated установлены, считаем что PSName получено полностью
              if (PSNameUpdated == 255)
              { // Дополнительное сравнение с предыдущим значением, чтобы не дублировать в Serial
                if (strcmp(PSName, PSName_prev) != 0)
                {
                  strcpy(PSName_prev, PSName);
                  lcd.print(0,1,PSName_prev);
                }
              }
            } // PSName, PTy end
          }
        }
      }
      RDSR_prev = reg0Ah & RDA5807M_FLAG_RDSR;

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
        //lcd.print(0, 1,"                ");  
      }      
      nextRadioInfoTime = now + 1000;
    } // update
  }
} // loop

// End.
