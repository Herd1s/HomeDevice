#include "oled_ui.h"
#include "drivers.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define OLED_UI_ROWS 4U
#define OLED_UI_COLS 16U

static char s_prev_lines[OLED_UI_ROWS][OLED_UI_COLS + 1];
static uint8_t s_prev_valid = 0U;

static const char* mode_text(SystemMode mode) {
  if (mode == MODE_ARM) {
    return "ARM";
  }
  if (mode == MODE_AUTO) {
    return "AUTO";
  }
  return "DISARM";
}

static void ui_make_line(char* out16, const char* fmt, ...) {
  char tmp[32];
  uint8_t i;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);

  for (i = 0U; i < OLED_UI_COLS; ++i) {
    out16[i] = ' ';
  }
  out16[OLED_UI_COLS] = '\0';

  for (i = 0U; i < OLED_UI_COLS && tmp[i] != '\0'; ++i) {
    out16[i] = tmp[i];
  }
}

static void ui_flush_diff(char lines[OLED_UI_ROWS][OLED_UI_COLS + 1]) {
  uint8_t row;
  for (row = 0U; row < OLED_UI_ROWS; ++row) {
    if (!s_prev_valid || memcmp(s_prev_lines[row], lines[row], OLED_UI_COLS) != 0) {
      Drivers_OledPrint(row, 0, lines[row]);
      memcpy(s_prev_lines[row], lines[row], OLED_UI_COLS + 1U);
    }
  }
  s_prev_valid = 1U;
}

void OledUi_Init(void) {
  uint8_t i;
  Drivers_OledClear();
  for (i = 0U; i < OLED_UI_ROWS; ++i) {
    memset(s_prev_lines[i], 0, sizeof(s_prev_lines[i]));
  }
  s_prev_valid = 0U;
  Drivers_OledPrint(0, 0, "HomeDevice       ");
  Drivers_OledPrint(1, 0, "Init...          ");
  Drivers_OledPrint(2, 0, "                ");
  Drivers_OledPrint(3, 0, "                ");
  Drivers_OledFlush();
}

void OledUi_Show(const SensorData* sensors, const DeviceState* state, uint8_t alarm_flags) {
  char lines[OLED_UI_ROWS][OLED_UI_COLS + 1];

  if (state->oled_page == 0U) {
    ui_make_line(lines[0], "P0 M:%s A:%c", mode_text(state->mode), (alarm_flags != 0U) ? 'Y' : 'N');
    ui_make_line(lines[1], "T:%d H:%u S:%u", (int)sensors->temperature, (unsigned)sensors->humidity, (unsigned)sensors->smoke);
    ui_make_line(lines[2], "F:%u I:%u R:%u B:%u", (unsigned)sensors->flame, (unsigned)sensors->intrusion, (unsigned)state->relay_on,
                 (unsigned)state->buzzer_on);
    ui_make_line(lines[3], "K1Pg K2Md");
  } else if (state->oled_page == 1U) {
    ui_make_line(lines[0], "P1 THRESHOLD");
    ui_make_line(lines[1], "TH:%u NOW:%u", (unsigned)state->smoke_threshold, (unsigned)sensors->smoke);
    ui_make_line(lines[2], "K3:+10 K4:-10");
    ui_make_line(lines[3], "K1Pg K2Md");
  } else {
    ui_make_line(lines[0], "P2 CONTROL");
    ui_make_line(lines[1], "M:%s R:%u B:%u", mode_text(state->mode), (unsigned)state->relay_on, (unsigned)state->buzzer_on);
    ui_make_line(lines[2], "K3:RelayOn");
    ui_make_line(lines[3], "K4:Silence K1");
  }

  ui_flush_diff(lines);
  Drivers_OledFlush();
}
