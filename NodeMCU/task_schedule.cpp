
#include "task_schedule.h"
#include <Arduino.h>

Schedule::Schedule(void (func()))
{
	task.callback = func;
}

Schedule::~Schedule()
{

}

void Schedule::setStartTime(int hour, int min, int sec)
{
	task.start.tm_hour = hour;
    task.start.tm_min = min;
    task.start.tm_sec = sec;
}

void Schedule::setStopTime(int hour, int min, int sec)
{
	task.stop.tm_hour = hour;
    task.stop.tm_min = min;
    task.stop.tm_sec = sec;
}

void Schedule::setDay(int day, bool val)
{
	if(day>=1 || day<=7) val?bitSet(task.dweek,day):bitClear(task.dweek,day);
}

void Schedule::run(time_t epoch)
{
	time_t rawtime = epoch;
	struct tm  ts = *localtime(&epoch);

	// проверка условий сценариев
	// tm_wday	->	days since Sunday: 0-6
	if(ts.tm_wday?bitRead(task.dweek,ts.tm_wday):bitRead(task.dweek,7))
	{
  		if(epoch >= mktime(&(task.start)))
  		{
  			if(epoch <= mktime(&(task.stop)))
  			{ // однократно запускаем на выполнение
  				if(!task.flag)
  				{
  					task.callback();
  					task.flag = 1;
  				} 				
  			}
  			else
  			{ // останавливаем
  				task.flag = 0;
  			}
  		}
	}
}