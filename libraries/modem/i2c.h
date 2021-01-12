//**********************************************************
//************************** I2C ***************************
//**********************************************************
#include <Wire.h>

#define ESP_OFF       digitalWrite(A3,LOW);param.esp_enable=0;esp_flag_sens_update=false;
#define ESP_ON        digitalWrite(A3,HIGH);
#define ESP8266_RESET {ESP_OFF;DELAY(2000);ESP_ON}

extern MODEM *modem;
extern MY_SENS *sensors;

bool esp_flag_sens_update = false;

// function that executes whenever data is received from master
void receiveEvent(int numBytes)
{
  char buf[BUFFER_LENGTH];
  uint8_t i = 0, j;
  uint8_t count = 0;
  uint8_t size;
  char* p;
  bool sens_flag = false;  
  i2c_cmd *cmd;
  uint16_t val;

  while (0 < Wire.available() && i < BUFFER_LENGTH) {
    buf[i] = (uint8_t)Wire.read();
    //DEBUG_PRINT(' ');DEBUG_PRINT(buf[i]);DEBUG_PRINT('|');DEBUG_PRINT(buf[i],HEX);
    i++;
  }

  count = i;
  i = 0;

  while(i < count)
  {
    if(count - i >= sizeof(i2c_cmd))
    {
      cmd = (i2c_cmd*)&buf[i];
      j = i;

      if(cmd->type == I2C_DATA)
      {
        i+=sizeof(i2c_cmd);

        switch (cmd->cmd)
        {
          case I2C_SENS_NUM: 
          case I2C_SENS_VALUE:
            for(uint8_t k = j; k < sizeof(i2c_cmd); k++)
            {
              DEBUG_PRINT(' ');DEBUG_PRINT(buf[k]);DEBUG_PRINT('|');DEBUG_PRINT(buf[k],HEX);
            }
            DEBUG_PRINT(F("\ntype:"));
            DEBUG_PRINT(cmd->type, HEX);
            DEBUG_PRINT(F(" cmd:"));
            DEBUG_PRINT(cmd->cmd);
            DEBUG_PRINT(F(" data:"));
            //cmd->data>>=8;
            DEBUG_PRINTLN(cmd->data);

            sensors->SetSensValue(param.tx, cmd->cmd, cmd->data);
            esp_flag_sens_update=true;
          break;
          case I2C_SENS_NAME:
            
            for(uint8_t k = j; k < sizeof(i2c_cmd); k++)
            {
              DEBUG_PRINT(' ');DEBUG_PRINT(buf[k]);DEBUG_PRINT('|');DEBUG_PRINT(buf[k],HEX);
            }
            DEBUG_PRINT(F("\ntype:"));
            DEBUG_PRINT(cmd->type, HEX);
            DEBUG_PRINT(F(" cmd:"));
            DEBUG_PRINT(cmd->cmd);
            DEBUG_PRINT(F(" data:"));
            DEBUG_PRINTLN(cmd->data);       
            
            //cmd->data>>=8;
            param.tx->AddText_P(PSTR(I2C_NAME));
            param.tx->AddInt(cmd->data);
            param.tx->AddChar(SYMBOL);
            param.tx->AddText_P(sensors->GetName(cmd->data));            
            param.tx->AddChar('\n');   
          break;          
          case I2C_FLAG_NAMES:
            param.tx->AddText_P(PSTR(I2C_FLAG));
            param.tx->AddChar(SYMBOL);           
            param.tx->AddText_P(PSTR(ALARM_NAME));
            param.tx->AddChar(SYMBOL);
            param.tx->AddText_P(PSTR(GUARD_NAME));
#if SIRENA_ENABLE
            param.tx->AddChar(SYMBOL);
            param.tx->AddText_P(PSTR(SIREN_NAME));   
#endif          
            if (param.sim800_enable)
            {
              param.tx->AddChar(SYMBOL);
              param.tx->AddText_P(PSTR(EMAIL_NAME));
              param.tx->AddChar(SYMBOL);
              param.tx->AddText_P(PSTR(SMS_NAME));
              param.tx->AddChar(SYMBOL);
              param.tx->AddText_P(PSTR(RING_NAME));
            }
            param.tx->AddChar('\n');
            esp_flag_sens_update = false;                    
          break;
          case I2C_FLAGS:            
            memcpy(&flags, &(cmd->data), 1);
          break;
          case I2C_SENS_ENABLE:
            for(int k = 0; k < sensors->size; k++)
            {
              sensors->SetEnable(k, bitRead(cmd->data, k));
            }
          break;
          case I2C_DTMF:
            param.DTMF[0] = cmd->data;
          break;
          case I2C_CIRCUIT:
            sensors->SetCircuit(cmd->data>>8, cmd->data & 0x01);
          break;
          case I2C_MASK_CIRCUIT:
            sensors->circuit = cmd->data;
        }
        continue;
      }
    }
    param.rx->AddChar(buf[i]);
    //DEBUG_PRINT(buf[i]);
    if(!buf[i] || buf[i]=='\n')
    {
      //DEBUG_PRINT(F("i2c:"));DEBUG_PRINTLN(param.rx->GetText());
      modem->parser();
      param.rx->Clear();       
    }
    i++;
  }
}

// function that executes whenever data is requested from master
void requestEvent(void)
{
  uint8_t i = 0;
  char buf[BUFFER_LENGTH];

  param.esp_enable = 1;

  memset(buf, 0, BUFFER_LENGTH);

  buf[i++] = START_BYTE;
  buf[i++] = START_BYTE;
  
  buf[i++] = *((char*)&flags);

  i+=param.tx->GetBytes(buf+i, BUFFER_LENGTH-6, 0);

  buf[i++] = END_BYTE;
  buf[i++] = END_BYTE;

  Wire.write(buf, BUFFER_LENGTH);
}

void i2c_init()
{
  pinMode(A3,OUTPUT);
  ESP8266_RESET // Перезагружаем ESP8266
  Wire.begin(I2C_ARDUINO_ADDR); /* join i2c bus with address */
  Wire.onReceive(receiveEvent); /* register receive event */
  Wire.onRequest(requestEvent); /* register request event */
}
