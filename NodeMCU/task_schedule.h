#ifndef SCHEDULE_H_
#define SCHEDULE_H_

#include <time.h>

typedef struct schedule
{
	struct tm start;
	struct tm stop;
	unsigned char dweek; // дни недели 1..7 биты
	void (*callback)(); // вызываемая функция
	bool flag; // запущен или нет
} SCHEDULE;

class Schedule
{
public:
	SCHEDULE task;

	Schedule(void (func()));
	~Schedule();

	void setStartTime(int hour, int min, int sec);
	void setStopTime(int hour, int min, int sec);
	void setDay(int day, bool val);

	void run(time_t epoch);
};

#endif // SCHEDULE_H_