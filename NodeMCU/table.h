#ifndef TABLE_H_
#define TABLE_H_

#define TABLE_CLEAR {table.clear();rows=0;}
#define ROW_SELECT(index, selected) Blynk.virtualWrite(TABLE_PIN, selected ? "select" : "deselect", index);
#define TABLE_ADD_ROW(name,value,enable) {table.addRow(rows,name,value);ROW_SELECT(rows,enable);rows++;}

//void set_circuit_enable(uint8_t circuit, bool State);
void i2c_send_command(uint16_t value, uint8_t cmd);

uint8_t rows = 0;

//uint16_t circuit_enable[2] = {0,0}; // маска активности датчиков контуров

uint16_t sensors_get_enable(uint8_t circuit)
{
  uint16_t enable = 0;

  for(uint8_t i = 0; i < params.sens_num; i++)
  {
    if(bitRead(circuit, sensor[i].circuit))
    {
      sensor[i].enable?bitSet(enable,i):bitClear(enable,i);
    } 
  }

  return enable;
}

// Поиск датчика в контуре
// возвращает индекс датчика в общем массиве датчиков
// возвращает -1 если датчик не найден
int search_sensor(int circuit, int index)
{
  if(circuit)
  {
    for(int i = 0; i < params.sens_num; i++)
    {
      if(bitRead(circuit, sensor[i].circuit))
      {
        if(sensor[i].index == index) return i;
      }
    }
  } 

  return -1;
}

void table_init()
{
	TABLE_CLEAR

	// Setup table event callbacks
 	table.onOrderChange([](int indexFrom, int indexTo)
 	{
		terminal.print("Reordering: ");
		terminal.print(indexFrom);
		terminal.print(" => ");
		terminal.print(indexTo);
		terminal.flush();
 	});

 	table.onSelectChange([](int index, bool selected)
 	{
    	int i;
    	uint8_t tmp;

      DEBUG_PRINT(F("index: ")); DEBUG_PRINT(index); DEBUG_PRINTLN(selected?VKL:VIKL);

    	if(flag_set_circuit) // режим выбора контура для датчиков
      {         
        if(index)
        {
          sensor[index-1].circuit++;
          if(sensor[index-1].circuit > 1) sensor[index-1].circuit = 0; // только 2 контура
          table.updateRow(index, sensor[index-1].name, sensor[index-1].circuit);
          i2c_send_command(((index-1)<<8 | sensor[index-1].circuit),I2C_CIRCUIT);
        }        
      }
      else
      if(index < params.flags_num)
   		{
   			tmp = 0;

        for(i=0; i<params.flags_num; i++)
        {
          if(flag[i].value) tmp |= 1 << i;
        }
        selected ? bitSet(tmp,index) : bitClear(tmp,index);

      	i2c_send_command(tmp,I2C_FLAGS);   		
      }
    	else
    	{
        if(tab.index == 1) i = index - params.flags_num;
        else if(tab.circuit)
        {
          DEBUG_PRINT(F("tab: ")); DEBUG_PRINTLN(tab.circuit);
          DEBUG_PRINT(F("index in circuit: ")); DEBUG_PRINTLN(i);
          // ищем датчик в контуре
          i = search_sensor(tab.circuit, i);
          DEBUG_PRINT(F("index out circuit: ")); DEBUG_PRINTLN(i);
        }

        if(i >= 0)
        {
          sensor[i].enable = selected;
          saved_data.enable = sensors_get_enable(3); // все контуры
          //selected?bitSet(circuit_enable[0],i):bitClear(circuit_enable[0],i);
          i2c_send_command(saved_data.enable, I2C_SENS_ENABLE);

          terminal.print(F(SENSOR)); terminal.print(sensor[i].name);
          terminal.println(sensor[i].enable ? VKL : VIKL);
          terminal.flush();
        } 
    	}
	});
}

void table_add_flags()
{
  for(uint8_t i=0; i<params.flags_num; i++)
  {
    TABLE_ADD_ROW(flag[i].name,flag[i].value,flag[i].value)
  } 
}

// circuit:
// 0 - все
// 1 - первый контур
// 2 - второй контур
void table_add_circuit(uint8_t circuit)
{
  for(uint8_t i=0; i<params.sens_num; i++)
  {
    if(!circuit || bitRead(circuit,sensor[i].circuit))
    {
      TABLE_ADD_ROW(sensor[i].name, sensor[i].value, sensor[i].enable)
    }          
  }
}

/*
 * Ищем строку в таблице с сенсором по его интексу в текущей вкладке
 * Возвращает номер строки
 * Если не найдено, возвращает -1
 */
int find_sensor_in_tab(uint8_t index)
{
  uint8_t count = 0;

  //DEBUG_PRINT(F("tab.index=")); DEBUG_PRINTLN(tab.index);

  if(tab.index > TAB_C1) return -1;
  if(tab.index == TAB_ALL) return (index + params.flags_num);

  for(uint8_t i=0; i<params.sens_num; i++)
  {
    if(bitRead(tab.circuit,sensor[i].circuit))
    {
      //DEBUG_PRINT(F("count=")); DEBUG_PRINTLN(count);
      if(i == index) // тот самый датчик
      {
        return count;
      }
      count++;
    }          
  }

  return -1;
}

void table_sensor_update(uint8_t index)
{
  int row = -1;
  
  row = find_sensor_in_tab(index);

  //DEBUG_PRINT(F("ROW=")); DEBUG_PRINTLN(row);
  //DEBUG_PRINT(F("i=")); DEBUG_PRINTLN(index);
  if(row != -1) table.updateRow(row, sensor[index].name, sensor[index].value);
}

#endif // TABLE_H_