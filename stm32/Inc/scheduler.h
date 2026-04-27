#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

enum {
  TASK_KEY = (1UL << 0),
  TASK_SENSOR = (1UL << 1),
  TASK_OLED = (1UL << 2),
  TASK_UPLOAD = (1UL << 3)
};

void Scheduler_Init(uint32_t now_ms);
uint32_t Scheduler_GetDueTasks(uint32_t now_ms);

#endif

