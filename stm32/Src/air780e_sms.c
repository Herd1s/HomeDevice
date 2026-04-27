#include "air780e_sms.h"
#include "comm_uart.h"

static void send_line(const char* s) {
  CommUart_SendToAirText(s);
  CommUart_SendToAirText("\r\n");
}

void Air780E_Init(void) {
  send_line("AT");
  send_line("AT+CPIN?");
  send_line("AT+CSQ");
  send_line("AT+CMGF=1");
  send_line("AT+CSCS=\"GSM\"");
}

void Air780E_SendSms(const char* phone, const char* text) {
  send_line("AT+CMGF=1");
  CommUart_SendToAirText("AT+CMGS=\"");
  CommUart_SendToAirText(phone);
  CommUart_SendToAirText("\"\r\n");
  CommUart_SendToAirText(text);
  {
    const uint8_t ctrl_z = 0x1A;
    CommUart_SendToAirRaw(&ctrl_z, 1);
  }
}

