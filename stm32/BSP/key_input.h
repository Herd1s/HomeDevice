#ifndef KEY_INPUT_H
#define KEY_INPUT_H

#include <stdint.h>

enum {
  KEY_EVT_K1 = (1U << 0),
  KEY_EVT_K2 = (1U << 1),
  KEY_EVT_K3 = (1U << 2),
  KEY_EVT_K4 = (1U << 3)
};

void KeyInput_Init(void);
uint8_t KeyInput_Scan(void);

#endif

