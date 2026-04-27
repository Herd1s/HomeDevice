#include "alarm_logic.h"
#include "app_config.h"

void AlarmLogic_Init(AlarmContext* ctx) {
  ctx->prev_flags = 0U;
  ctx->smoke_over_count = 0U;
}

AlarmResult AlarmLogic_Update(AlarmContext* ctx, const SensorData* sensors, const DeviceState* state) {
  AlarmResult result;
  uint8_t flags = 0U;

  if (sensors->smoke >= state->smoke_threshold) {
    if (ctx->smoke_over_count < 0xFFFFU) {
      ctx->smoke_over_count++;
    }
  } else {
    ctx->smoke_over_count = 0U;
  }

  if (ctx->smoke_over_count >= SMOKE_OVER_COUNT_TO_ALARM) {
    flags |= ALARM_FLAG_SMOKE;
  }
  if (sensors->flame) {
    flags |= ALARM_FLAG_FLAME;
  }
  if ((state->mode == MODE_ARM) && sensors->intrusion) {
    flags |= ALARM_FLAG_INTRUSION;
  }

  result.current_flags = flags;
  result.rising_flags = (uint8_t)(flags & (uint8_t)(~ctx->prev_flags));
  ctx->prev_flags = flags;
  return result;
}

