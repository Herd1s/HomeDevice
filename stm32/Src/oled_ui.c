#include "oled_ui.h"
#include "drivers.h"

#include <stdio.h>

static const char* mode_text(SystemMode mode) {
  if (mode == MODE_ARM) {
    return "ARM";
  }
  if (mode == MODE_AUTO) {
    return "AUTO";
  }
  return "DISARM";
}

void OledUi_Init(void) {
  Drivers_OledClear();
  Drivers_OledPrint(0, 0, "HomeDevice");
  Drivers_OledPrint(2, 0, "Initializing...");
  Drivers_OledFlush();
}

void OledUi_Show(const SensorData* sensors, const DeviceState* state, uint8_t alarm_flags) {
  char line[32];

  Drivers_OledClear();

  snprintf(line, sizeof(line), "T:%dC H:%d%%", (int)sensors->temperature, (int)sensors->humidity);
  Drivers_OledPrint(0, 0, line);

  snprintf(line, sizeof(line), "SMK:%u TH:%u", (unsigned)sensors->smoke, (unsigned)state->smoke_threshold);
  Drivers_OledPrint(1, 0, line);

  snprintf(line, sizeof(line), "F:%u I:%u M:%s", (unsigned)sensors->flame, (unsigned)sensors->intrusion, mode_text(state->mode));
  Drivers_OledPrint(2, 0, line);

  snprintf(line, sizeof(line), "R:%u B:%u N:%u A:%u", (unsigned)state->relay_on, (unsigned)state->buzzer_on, (unsigned)state->net_ok, (unsigned)state->air780e_ok);
  Drivers_OledPrint(3, 0, line);

  if (alarm_flags != 0U) {
    Drivers_OledPrint(4, 0, "ALARM ACTIVE");
  } else {
    Drivers_OledPrint(4, 0, "NORMAL");
  }
  Drivers_OledFlush();
}

