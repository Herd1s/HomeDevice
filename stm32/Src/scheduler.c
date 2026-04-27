#include "app_config.h"
#include "scheduler.h"

static uint32_t s_last_key_ms = 0;
static uint32_t s_last_sensor_ms = 0;
static uint32_t s_last_oled_ms = 0;
static uint32_t s_last_upload_ms = 0;

void Scheduler_Init(uint32_t now_ms) {
  s_last_key_ms = now_ms;
  s_last_sensor_ms = now_ms;
  s_last_oled_ms = now_ms;
  s_last_upload_ms = now_ms;
}

uint32_t Scheduler_GetDueTasks(uint32_t now_ms) {
  uint32_t tasks = 0;

  if ((now_ms - s_last_key_ms) >= TASK_PERIOD_KEY_MS) {
    s_last_key_ms = now_ms;
    tasks |= TASK_KEY;
  }
  if ((now_ms - s_last_sensor_ms) >= TASK_PERIOD_SENSOR_MS) {
    s_last_sensor_ms = now_ms;
    tasks |= TASK_SENSOR;
  }
  if ((now_ms - s_last_oled_ms) >= TASK_PERIOD_OLED_MS) {
    s_last_oled_ms = now_ms;
    tasks |= TASK_OLED;
  }
  if ((now_ms - s_last_upload_ms) >= TASK_PERIOD_UPLOAD_MS) {
    s_last_upload_ms = now_ms;
    tasks |= TASK_UPLOAD;
  }

  return tasks;
}

