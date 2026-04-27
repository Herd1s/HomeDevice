#include "key_input.h"
#include "drivers.h"

#define KEY_NUM 4U
#define KEY_DEBOUNCE_COUNT 3U

static uint8_t s_stable_state[KEY_NUM];
static uint8_t s_counter[KEY_NUM];

void KeyInput_Init(void) {
  uint8_t i;
  for (i = 0U; i < KEY_NUM; ++i) {
    s_stable_state[i] = 0U;
    s_counter[i] = 0U;
  }
}

uint8_t KeyInput_Scan(void) {
  uint8_t evt = 0U;
  uint8_t i;

  for (i = 0U; i < KEY_NUM; ++i) {
    uint8_t raw = Drivers_KeyReadRaw(i) ? 1U : 0U;
    if (raw == s_stable_state[i]) {
      s_counter[i] = 0U;
    } else {
      if (s_counter[i] < 0xFFU) {
        s_counter[i]++;
      }
      if (s_counter[i] >= KEY_DEBOUNCE_COUNT) {
        s_counter[i] = 0U;
        s_stable_state[i] = raw;
        if (raw == 1U) {
          evt |= (uint8_t)(1U << i);
        }
      }
    }
  }
  return evt;
}

