#ifndef MY_SENS_H
#define MY_SENS_H

#include "settings.h"
#include "sensor.h"
#include "text.h"

class MY_SENS
{
  public:
    uint8_t size;
    uint8_t circuit;

        MY_SENS(SENSOR_INIT_PARAM* sens, uint8_t num);
        ~MY_SENS();

        void       Clear       ();
        void       GetInfo     (void *buf);
        void       GetInfoAll  (void *buf);
        void       SensOpros   (TEXT* tx);
        void       SetEnable   (uint8_t index, bool value);
        void       SetCircuit  (uint8_t index, uint8_t circuit);
        void       SetSensValue(TEXT* tx, uint8_t cmd, uint8_t index);

        bool       GetEnable   (uint8_t index);
        RET_TYPE   GetValue    (uint8_t index);
        uint8_t    GetType     (uint8_t index);
        uint8_t    GetCircuit  (uint8_t index);
  const char*      GetName     (uint8_t index);
    

  private:
	Sensor **sensors;
};

#endif // MY_SENS_H
