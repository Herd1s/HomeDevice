#ifndef OLED_UI_H
#define OLED_UI_H

#include "app_types.h"

void OledUi_Init(void);
void OledUi_Show(const SensorData* sensors, const DeviceState* state, uint8_t alarm_flags);

#endif

