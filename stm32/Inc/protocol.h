#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_HEAD1 0xAA
#define PROTO_HEAD2 0x55
#define PROTO_MAX_DATA 64

typedef enum {
  PROTO_TYPE_ENV = 0x01,
  PROTO_TYPE_ALARM = 0x02,
  PROTO_TYPE_CTRL = 0x03,
  PROTO_TYPE_PARAM = 0x04,
  PROTO_TYPE_ACK = 0x05
} ProtoType;

typedef struct {
  uint8_t type;
  uint8_t len;
  uint8_t data[PROTO_MAX_DATA];
} ProtocolFrame;

typedef struct {
  uint8_t buf[PROTO_MAX_DATA + 5];
  uint16_t idx;
} ProtocolParser;

void Protocol_InitParser(ProtocolParser* p);
uint8_t Protocol_Checksum(uint8_t type, uint8_t len, const uint8_t* data);
uint8_t Protocol_PushByte(ProtocolParser* p, uint8_t byte, ProtocolFrame* out_frame);
uint16_t Protocol_BuildFrame(uint8_t type, const uint8_t* data, uint8_t len, uint8_t* out_buf, uint16_t max_len);

#endif

