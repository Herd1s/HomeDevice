#ifndef SENSOR_H
#define SENSOR_H

#include "app_types.h"

void Sensor_Init(void);
void Sensor_ReadFast(SensorData* out_data);
void Sensor_ReadSlow(SensorData* in_out_data);

#endif

