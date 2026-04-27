#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

typedef enum {
  MODE_DISARM = 0,
  MODE_ARM = 1,
  MODE_AUTO = 2
} SystemMode;

typedef struct {
  int8_t temperature;
  uint8_t humidity;
  uint16_t smoke;
  uint8_t flame;
  uint8_t intrusion;
} SensorData;

typedef struct {
  SystemMode mode;
  uint16_t smoke_threshold;
  uint8_t relay_on;
  uint8_t buzzer_on;
  uint8_t net_ok;
  uint8_t air780e_ok;
  uint8_t oled_page;
} DeviceState;

enum {
  ALARM_FLAG_SMOKE = (1U << 0),
  ALARM_FLAG_FLAME = (1U << 1),
  ALARM_FLAG_INTRUSION = (1U << 2)
};

typedef struct {
  uint8_t current_flags;
  uint8_t rising_flags;
} AlarmResult;

#endif

