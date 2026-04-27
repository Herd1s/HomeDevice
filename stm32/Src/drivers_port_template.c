#include "drivers.h"

// This file is a hardware adaptation template.
// Replace stubs with STM32F103 Standard Peripheral Library code.

void Drivers_Init(void) {}

uint32_t Drivers_GetMs(void) {
  static uint32_t fake_ms = 0;
  fake_ms += 10;
  return fake_ms;
}

int Drivers_Uart1_ReadByte(uint8_t* out_byte) {
  (void)out_byte;
  return 0;
}

void Drivers_Uart1_Send(const uint8_t* data, uint16_t len) {
  (void)data;
  (void)len;
}

void Drivers_Uart2_Send(const uint8_t* data, uint16_t len) {
  (void)data;
  (void)len;
}

uint8_t Drivers_ReadDht11(int8_t* out_temp, uint8_t* out_humi) {
  *out_temp = 25;
  *out_humi = 50;
  return 1;
}

uint16_t Drivers_ReadMq2Adc(void) { return 350; }

uint8_t Drivers_ReadFlame(void) { return 0; }

uint8_t Drivers_ReadIntrusion(void) { return 0; }

void Drivers_SetBuzzer(uint8_t on) { (void)on; }

void Drivers_SetRelay(uint8_t on) { (void)on; }

uint8_t Drivers_KeyReadRaw(uint8_t index) {
  (void)index;
  return 0;
}

void Drivers_OledClear(void) {}

void Drivers_OledPrint(uint8_t row, uint8_t col, const char* text) {
  (void)row;
  (void)col;
  (void)text;
}

void Drivers_OledFlush(void) {}

uint8_t Drivers_IsNightTime(void) { return 1; }

