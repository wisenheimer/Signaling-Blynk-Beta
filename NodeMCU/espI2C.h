#ifndef ESP_I2C_H_
#define ESP_I2C_H_

#include <Wire.h>

#define I2C_WRITE(buf,len) {Wire.beginTransmission(I2C_ARDUINO_ADDR);Wire.write(buf,len);Wire.endTransmission();}
#define I2C_SEND_COMMAND(value,command) {struct i2c_cmd str;str.type=I2C_DATA;str.cmd=command;str.data=value;I2C_WRITE((uint8_t*)&str,sizeof(str));}
//#define I2C_SEND_COMMAND(index,cmd) {struct i2c_cmd str;I2C_SET_COMMAND(str,index,cmd);uint8_t* p=(uint8_t*)&str;for(uint8_t k=0;k<5;k++)Serial.printf(" %X",*(p+k));DEBUG_PRINTLN();I2C_WRITE((uint8_t*)&str,5);}

#define BUF_DEL_BYTES(pos,num) {in_index-=num;for(uint8_t i=pos,j=pos+num;i<in_index;i++,j++){in_buffer[i]=in_buffer[j];} \
                                memset(in_buffer+in_index,0,IN_BUF_SIZE-in_index);}

#define FLAG_TYPE 1
#define SENS_TYPE 2

// размер in_buffer
#define IN_BUF_SIZE 512
char in_buffer[IN_BUF_SIZE];  // сюда складываются байты из шины I2C
uint8_t in_index   = 0;

void i2c_send_command(uint16_t value, uint8_t cmd) I2C_SEND_COMMAND(value,cmd)

uint8_t get_num_rows(char* cmd)
{
  char* p;
  uint8_t count = 0;
  
  DEBUG_PRINTF("find %s in %s\n",cmd, in_buffer);
  
  if((p=READ_COM_FIND(cmd))!=NULL)
  {
    p+=strlen(cmd);

    DEBUG_PRINTF("success! p=%s\n", p);
          
    while(*p==SYMBOL)
    {
      p++;
      
      while(*p && *p!=SYMBOL && *p!='\n') {p++;}

      count++;
    }
  }

  DEBUG_PRINTF("count=%d\n", count);

  return count;
}

// I2C_NAME0%name
// [I2C_NAME][index][SYMBOL][name]
int8_t sens_read_name(uint8_t num = 0)
{
  char* p;
  char* pn;
  char* beg;
  uint8_t i;
  uint8_t index = 0;
  char tmp[3];

  if((p=READ_COM_FIND(I2C_NAME))==NULL)
  {
    return -1;
  }
  beg = p;
  p+=strlen(I2C_NAME);
  pn = p;
  i = 0;
  while(*p && *p!=SYMBOL && *p!='\n') {i++; p++;}

  if(!i)
  {
    BUF_DEL_BYTES(beg-in_buffer,p-beg)
    return -1;
  }

  *p = 0;
  index = atoi(pn);
  if(index >= num)
  {
    BUF_DEL_BYTES(beg-in_buffer,p-beg)
    return -1;
  }

  i = 0; p++;
  pn = p;
  while(*p && *p!='\n') {i++; p++;}
      
  if(sensor[index].name != NULL) free(sensor[index].name);
  if((sensor[index].name = (char*)malloc(i+1)) != NULL)
  {
    memcpy(sensor[index].name, pn, i);
    sensor[index].name[i] = 0;
    sensor[index].index = index;       
  }

  BUF_DEL_BYTES(beg-in_buffer,p-beg)

  return index;
}

uint8_t read_names(uint8_t type, uint8_t num = 0)
{
  char* p;
  char* pn;
  uint8_t i;
  uint8_t index = 0;
  char* cmd; 

  type == FLAG_TYPE ? cmd = I2C_FLAG : cmd = I2C_NAME;

  p = in_buffer;

  if(p!=NULL)
  {
    p+=strlen(cmd);

    while(*p==SYMBOL && index < num)
    {
            
      i = 0; p++;
      pn = p;
      while(*p && *p!=SYMBOL && *p!='\n') {i++; p++;}
      
      if (type == FLAG_TYPE)
      {
        if(flag[index].name != NULL) free(flag[index].name);
        if((flag[index].name = (char*)malloc(i+1)) != NULL)
        {
          memcpy(flag[index].name, pn, i);
          flag[index].name[i] = 0;         
        }
      }

      if (type == SENS_TYPE)
      {
        if(sensor[index].name != NULL) free(sensor[index].name);
        if((sensor[index].name = (char*)malloc(i+1)) != NULL)
        {
          memcpy(sensor[index].name, pn, i);
          sensor[index].name[i] = 0;
          sensor[index].index = index;        
        }
      }
		  index++;  
    }
  }

  return index;
}

bool parser()
{
  char* p;
  char i2c_data[3] = {0x00,0x00,0x00};

  *((uint16_t*)i2c_data) = I2C_DATA;
/*
  DEBUG_PRINT(F("PARSER:"));
  for(uint8_t i = 0; i < in_index; i++)
  {
  	DEBUG_PRINT(' ');DEBUG_PRINT(in_buffer[i]);DEBUG_PRINT('|');DEBUG_PRINT(in_buffer[i],HEX);
  }
  DEBUG_PRINTLN();  
*/

  while((p=READ_COM_FIND(i2c_data))!=NULL)
  { // Показания датчика
    //DEBUG_PRINTF("len=%d size=%d\n", sizeof(i2c_sens_value), in_index-(p-in_buffer));
    if((in_index-(p-in_buffer))<sizeof(i2c_sens_value)) return false;
    {        
      i2c_sens_value sens_value;
      memcpy(&sens_value, p, sizeof(i2c_sens_value));

      DEBUG_PRINT(F("\ntype:"));
      DEBUG_PRINT(sens_value.type, HEX);
      DEBUG_PRINT(F(" cmd:"));
      DEBUG_PRINT(sens_value.cmd);
      DEBUG_PRINT(F(" index:"));
      DEBUG_PRINT(sens_value.index);
      DEBUG_PRINT(F(" circuit:"));
      DEBUG_PRINT(sens_value.circuit);
      DEBUG_PRINT(F(" enable:"));
      DEBUG_PRINT(sens_value.enable);
      DEBUG_PRINT(F(" value:"));
      DEBUG_PRINTLN(sens_value.value);\

      switch (sens_value.cmd)
      {
        case I2C_SENS_NUM:
          if(!params.sens_num)
          {
            params.sens_num = sens_value.index;
                
            if(params.sens_num)
            {
              sensor = new DATCHIK[params.sens_num];
              for(uint8_t i = 0; i < params.sens_num; i++)
              {
                sensor[i].name = NULL;
              }
            }          
          }
          break;
        case I2C_SENS_VALUE:
          if(sens_value.index < params.sens_num)
          {
            /*
            char *pp = (char*)&(sens_value.value);
            for(uint8_t k = 3; k < sizeof(i2c_sens_value); k++)
            { 
              *pp++=*(p+k);
            }
            DEBUG_PRINT(F("value:"));
            DEBUG_PRINTLN(sens_value.value);
      */
            if(sensor[sens_value.index].value != sens_value.value)
            {
              sensor[sens_value.index].value = sens_value.value;
              //Blynk.virtualWrite(values[sens_value.index].v_pin, values[sens_value.index]);
              if(!flag_set_circuit)
              {
          	    table_sensor_update(sens_value.index);
              }    
            }

            sensor[sens_value.index].circuit = sens_value.circuit;      

            // enable
            if(sensor[sens_value.index].enable != sens_value.enable)
            {
              sensor[sens_value.index].enable = sens_value.enable;
              terminal.print(F(SENSOR)); terminal.print(sensor[sens_value.index].name);
              terminal.println(sensor[sens_value.index].enable ? VKL : VIKL);
              terminal.flush();

              ROW_SELECT(sens_value.index+params.flags_num, sensor[sens_value.index].enable);          
            }            
          }
      } 
#if DEBUG_MODE
      PRINT_IN_BUFFER(F("START:"))
#endif      
      BUF_DEL_BYTES(p-in_buffer,sizeof(i2c_sens_value))

#if DEBUG_MODE
      PRINT_IN_BUFFER(F("END:"))
#endif     
    }  
  }

  return true; 
}


uint8_t i2c_read()
{
  char inChar;
  bool start_flag = false;  // найден пакет данных
  uint8_t count = 0;
  uint8_t received = 0;     // запоминаем кол-во байт в приёмном буффере

  while (Wire.available()) {
    // получаем новый байт:
    inChar = (char)Wire.read();

    DEBUG_PRINT(' ');DEBUG_PRINT(inChar);DEBUG_PRINT('|');DEBUG_PRINT(inChar,HEX);

    if(inChar == START_BYTE)
    {
      count++;
      continue; 
    }

    if(inChar == END_BYTE)
    {
      count--;
      if(count == 0)
      {
        start_flag = false;
        DEBUG_PRINTLN(F("END_BYTE"));
        parser();
      } 
      continue;      
    }
    
    if(start_flag)
    {
      if(in_index == IN_BUF_SIZE-1) inChar = '\n';

      in_buffer[in_index++] = inChar;
      received++;

      if(inChar=='\n')
      {
        DEBUG_PRINTLN(in_buffer);
        
        if(!params.flags_num)
        {
          params.flags_num=get_num_rows(I2C_FLAG);
          if(params.flags_num)
          {
            flag = new FLAG[params.flags_num];
            for(uint8_t i = 0; i < params.flags_num; i++)
            {
            	flag[i].name = NULL;
            }
            read_names(FLAG_TYPE, params.flags_num);
            Blynk.syncVirtual(TAB_PIN);
            //if(params.flags_num>2) mp3_file_load(audio_files[0]);
          }
        }
        else
        {
          if(params.sens_num)
          {
            if(sens_read_name(params.sens_num) == params.sens_num-1) Blynk.syncVirtual(TAB_PIN);
          }        
     
          if(in_index==0) return received;

          parser();                    
        }
        if(in_index==0) return received;

        char time[10];
        terminal.write('\n');
        terminal.write(printTime(time, 10));
        terminal.write("->");
        terminal.write(in_buffer);
        terminal.flush(); 

        memset(in_buffer, 0, IN_BUF_SIZE);
        in_index = 0;         
      }      
    }
    else if(count==2)
    {
      //count--;
      start_flag = true;
      // читаем flags
      for(uint8_t i = 0; i < params.flags_num; i++)
      {
      	if(flag[i].value != bitRead(inChar,i))
      	{
      		flag[i].value = bitRead(inChar,i);

      		if(!i) Blynk.virtualWrite(ALARM_PIN, flag[i].value);
            
            if(tab.index==TAB_ALL) // отобразить флаги
            {
            	table.updateRow(i, flag[i].name, flag[i].value);
        		  ROW_SELECT(i, flag[i].value);
            }
      	}
      }
    }         
  }

  DEBUG_PRINTLN();

  return received;
}


#endif // ESP_I2C_H_