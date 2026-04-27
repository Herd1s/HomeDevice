#ifndef ALARM_LOGIC_H
#define ALARM_LOGIC_H

#include "app_types.h"

typedef struct {
  uint8_t prev_flags;
  uint16_t smoke_over_count;
} AlarmContext;

void AlarmLogic_Init(AlarmContext* ctx);
AlarmResult AlarmLogic_Update(AlarmContext* ctx, const SensorData* sensors, const DeviceState* state);

#endif

