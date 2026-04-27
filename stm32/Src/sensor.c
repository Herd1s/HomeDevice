#include "sensor.h"
#include "drivers.h"

void Sensor_Init(void) {}

void Sensor_ReadFast(SensorData* out_data) {
  out_data->smoke = Drivers_ReadMq2Adc();
  out_data->flame = Drivers_ReadFlame();
  out_data->intrusion = Drivers_ReadIntrusion();
}

void Sensor_ReadSlow(SensorData* in_out_data) {
  int8_t t = in_out_data->temperature;
  uint8_t h = in_out_data->humidity;
  if (Drivers_ReadDht11(&t, &h)) {
    in_out_data->temperature = t;
    in_out_data->humidity = h;
  }
}

