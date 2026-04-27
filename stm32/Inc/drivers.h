#ifndef DRIVERS_H
#define DRIVERS_H

#include <stdint.h>

// Board/system
void Drivers_Init(void);
uint32_t Drivers_GetMs(void);

// UART1 <-> ESP32
int Drivers_Uart1_ReadByte(uint8_t* out_byte);
void Drivers_Uart1_Send(const uint8_t* data, uint16_t len);

// UART2 <-> Air780E
void Drivers_Uart2_Send(const uint8_t* data, uint16_t len);

// Sensors
uint8_t Drivers_ReadDht11(int8_t* out_temp, uint8_t* out_humi);
uint16_t Drivers_ReadMq2Adc(void);
uint8_t Drivers_ReadFlame(void);
uint8_t Drivers_ReadIntrusion(void);

// Actuators
void Drivers_SetBuzzer(uint8_t on);
void Drivers_SetRelay(uint8_t on);

// Keys: index 0..3, return 1 when pressed.
uint8_t Drivers_KeyReadRaw(uint8_t index);

// OLED
void Drivers_OledClear(void);
void Drivers_OledPrint(uint8_t row, uint8_t col, const char* text);
void Drivers_OledFlush(void);

// Time helper for AUTO mode: 1 night, 0 day.
uint8_t Drivers_IsNightTime(void);

#endif
