#include "protocol.h"

void Protocol_InitParser(ProtocolParser* p) {
  p->idx = 0;
}

uint8_t Protocol_Checksum(uint8_t type, uint8_t len, const uint8_t* data) {
  uint16_t sum = type + len;
  uint8_t i;
  for (i = 0; i < len; ++i) {
    sum += data[i];
  }
  return (uint8_t)(sum & 0xFFU);
}

uint8_t Protocol_PushByte(ProtocolParser* p, uint8_t byte, ProtocolFrame* out_frame) {
  if (p->idx == 0U && byte != PROTO_HEAD1) {
    return 0U;
  }
  if (p->idx == 1U && byte != PROTO_HEAD2) {
    p->idx = 0U;
    return 0U;
  }

  if (p->idx >= sizeof(p->buf)) {
    p->idx = 0U;
    return 0U;
  }
  p->buf[p->idx++] = byte;

  if (p->idx < 4U) {
    return 0U;
  }

  {
    uint8_t len = p->buf[3];
    uint16_t frame_len = (uint16_t)len + 5U;
    if (len > PROTO_MAX_DATA) {
      p->idx = 0U;
      return 0U;
    }
    if (p->idx < frame_len) {
      return 0U;
    }
    if (p->idx == frame_len) {
      uint8_t type = p->buf[2];
      uint8_t check = p->buf[frame_len - 1U];
      uint8_t calc = Protocol_Checksum(type, len, &p->buf[4]);
      p->idx = 0U;
      if (check != calc) {
        return 0U;
      }
      out_frame->type = type;
      out_frame->len = len;
      if (len > 0U) {
        uint8_t i;
        for (i = 0U; i < len; ++i) {
          out_frame->data[i] = p->buf[4U + i];
        }
      }
      return 1U;
    }
  }
  p->idx = 0U;
  return 0U;
}

uint16_t Protocol_BuildFrame(uint8_t type, const uint8_t* data, uint8_t len, uint8_t* out_buf, uint16_t max_len) {
  uint16_t total = (uint16_t)len + 5U;
  uint8_t i;
  if (len > PROTO_MAX_DATA || out_buf == 0 || total > max_len) {
    return 0U;
  }
  out_buf[0] = PROTO_HEAD1;
  out_buf[1] = PROTO_HEAD2;
  out_buf[2] = type;
  out_buf[3] = len;
  for (i = 0U; i < len; ++i) {
    out_buf[4U + i] = data[i];
  }
  out_buf[4U + len] = Protocol_Checksum(type, len, data);
  return total;
}

