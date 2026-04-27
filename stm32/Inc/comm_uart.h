#ifndef COMM_UART_H
#define COMM_UART_H

#include "protocol.h"

typedef void (*CommFrameHandler)(const ProtocolFrame* frame);

void CommUart_Init(CommFrameHandler cb);
void CommUart_Poll(void);
void CommUart_SendToEsp(uint8_t type, const uint8_t* data, uint8_t len);
void CommUart_SendToAirText(const char* text);
void CommUart_SendToAirRaw(const uint8_t* data, uint16_t len);

#endif

