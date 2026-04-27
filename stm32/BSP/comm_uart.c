#include "comm_uart.h"
#include "drivers.h"

#include <string.h>

static ProtocolParser s_parser;
static CommFrameHandler s_cb = 0;

void CommUart_Init(CommFrameHandler cb) {
  Protocol_InitParser(&s_parser);
  s_cb = cb;
}

void CommUart_Poll(void) {
  uint8_t b;
  ProtocolFrame frame;
  while (Drivers_Uart1_ReadByte(&b)) {
    if (Protocol_PushByte(&s_parser, b, &frame)) {
      if (s_cb != 0) {
        s_cb(&frame);
      }
    }
  }
}

void CommUart_SendToEsp(uint8_t type, const uint8_t* data, uint8_t len) {
  uint8_t buf[PROTO_MAX_DATA + 5];
  uint16_t n = Protocol_BuildFrame(type, data, len, buf, sizeof(buf));
  if (n > 0U) {
    Drivers_Uart1_Send(buf, n);
  }
}

void CommUart_SendToAirText(const char* text) {
  Drivers_Uart2_Send((const uint8_t*)text, (uint16_t)strlen(text));
}

void CommUart_SendToAirRaw(const uint8_t* data, uint16_t len) {
  Drivers_Uart2_Send(data, len);
}

