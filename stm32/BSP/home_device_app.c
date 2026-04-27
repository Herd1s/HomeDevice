#include "home_device_app.h"

#include "air780e_sms.h"
#include "alarm_logic.h"
#include "app_config.h"
#include "app_types.h"
#include "comm_uart.h"
#include "drivers.h"
#include "key_input.h"
#include "oled_ui.h"
#include "scheduler.h"
#include "sensor.h"

#include <string.h>

enum {
  CTRL_SET_MODE = 0x01,
  CTRL_SET_RELAY = 0x02,
  CTRL_SILENCE = 0x03
};

enum {
  PARAM_SET_SMOKE_TH = 0x01
};

static SensorData g_sensors;
static DeviceState g_state;
static AlarmContext g_alarm_ctx;
static uint8_t g_alarm_flags = 0U;
static uint8_t g_dht_divider = 0U;

static uint8_t g_manual_relay_enable = 0U;
static uint8_t g_manual_relay_value = 0U;
static uint8_t g_silence_latch = 0U;

static uint16_t g_smoke_hist[8];
static uint8_t g_smoke_idx = 0U;
static uint32_t g_smoke_sum = 0U;

#define SMOKE_TH_MIN 50U
#define SMOKE_TH_MAX 4095U
#define SMOKE_TH_STEP 10U

static SystemMode effective_mode(void) {
  if (g_state.mode == MODE_AUTO) {
    return Drivers_IsNightTime() ? MODE_ARM : MODE_DISARM;
  }
  return g_state.mode;
}

static void update_smoke_filter(uint16_t raw) {
  g_smoke_sum -= g_smoke_hist[g_smoke_idx];
  g_smoke_hist[g_smoke_idx] = raw;
  g_smoke_sum += raw;
  g_smoke_idx = (uint8_t)((g_smoke_idx + 1U) % 8U);
  g_sensors.smoke = (uint16_t)(g_smoke_sum / 8U);
}

static void send_status_frame(void) {
  uint8_t payload[10];
  payload[0] = (uint8_t)g_sensors.temperature;
  payload[1] = g_sensors.humidity;
  payload[2] = (uint8_t)(g_sensors.smoke & 0xFFU);
  payload[3] = (uint8_t)((g_sensors.smoke >> 8) & 0xFFU);
  payload[4] = g_sensors.flame;
  payload[5] = g_sensors.intrusion;
  payload[6] = (uint8_t)g_state.mode;
  payload[7] = g_state.relay_on;
  payload[8] = g_state.buzzer_on;
  payload[9] = g_state.air780e_ok;
  CommUart_SendToEsp(PROTO_TYPE_ENV, payload, sizeof(payload));
}

static void send_alarm_frame(uint8_t alarm_type, uint8_t level, const char* text) {
  uint8_t buf[48];
  uint8_t i = 0U;
  buf[i++] = alarm_type;
  buf[i++] = level;
  while (*text != '\0' && i < sizeof(buf)) {
    buf[i++] = (uint8_t)(*text++);
  }
  CommUart_SendToEsp(PROTO_TYPE_ALARM, buf, i);
}

static void send_ack(uint16_t command_id) {
  uint8_t payload[2];
  payload[0] = (uint8_t)(command_id & 0xFFU);
  payload[1] = (uint8_t)((command_id >> 8) & 0xFFU);
  CommUart_SendToEsp(PROTO_TYPE_ACK, payload, sizeof(payload));
}

static void apply_outputs(void) {
  uint8_t force_relay = ((g_alarm_flags & (ALARM_FLAG_SMOKE | ALARM_FLAG_FLAME)) != 0U) ? 1U : 0U;
  uint8_t relay = force_relay ? 1U : (g_manual_relay_enable ? g_manual_relay_value : 0U);
  uint8_t buzzer = (g_alarm_flags != 0U) ? 1U : 0U;
  if (g_silence_latch) {
    buzzer = 0U;
  }

  g_state.relay_on = relay;
  g_state.buzzer_on = buzzer;

  Drivers_SetRelay(relay);
  Drivers_SetBuzzer(buzzer);
}

static void handle_rising_alarm(uint8_t rising_flags) {
  if (rising_flags & ALARM_FLAG_SMOKE) {
    send_alarm_frame(1U, 1U, "smoke warning");
    Air780E_SendSms(APP_PHONE_NUMBER, "[HomeDevice] Smoke over threshold.");
  }
  if (rising_flags & ALARM_FLAG_FLAME) {
    send_alarm_frame(2U, 2U, "flame detected");
    Air780E_SendSms(APP_PHONE_NUMBER, "[HomeDevice] Flame detected. Sprinkler ON.");
  }
  if (rising_flags & ALARM_FLAG_INTRUSION) {
    send_alarm_frame(3U, 2U, "intrusion");
    Air780E_SendSms(APP_PHONE_NUMBER, "[HomeDevice] Intrusion detected.");
  }
}

static void eval_logic(void) {
  AlarmResult r;
  DeviceState temp_state = g_state;
  temp_state.mode = effective_mode();
  r = AlarmLogic_Update(&g_alarm_ctx, &g_sensors, &temp_state);
  g_alarm_flags = r.current_flags;
  if (r.rising_flags != 0U) {
    g_silence_latch = 0U;
    handle_rising_alarm(r.rising_flags);
  }
  apply_outputs();
}

static void handle_keys(void) {
  uint8_t evt = KeyInput_Scan();
  if (evt & KEY_EVT_K1) {
    g_state.oled_page = (uint8_t)((g_state.oled_page + 1U) % 3U);
  }
  if (evt & KEY_EVT_K2) {
    if (g_state.mode == MODE_DISARM) {
      g_state.mode = MODE_ARM;
    } else if (g_state.mode == MODE_ARM) {
      g_state.mode = MODE_AUTO;
    } else {
      g_state.mode = MODE_DISARM;
    }
  }
  if (evt & KEY_EVT_K3) {
    if (g_state.oled_page == 1U) {
      if (g_state.smoke_threshold + SMOKE_TH_STEP <= SMOKE_TH_MAX) {
        g_state.smoke_threshold = (uint16_t)(g_state.smoke_threshold + SMOKE_TH_STEP);
      } else {
        g_state.smoke_threshold = SMOKE_TH_MAX;
      }
    } else {
      // Manual start relay.
      g_manual_relay_enable = 1U;
      g_manual_relay_value = 1U;
    }
  }
  if (evt & KEY_EVT_K4) {
    if (g_state.oled_page == 1U) {
      if (g_state.smoke_threshold > SMOKE_TH_MIN) {
        if (g_state.smoke_threshold >= (SMOKE_TH_MIN + SMOKE_TH_STEP)) {
          g_state.smoke_threshold = (uint16_t)(g_state.smoke_threshold - SMOKE_TH_STEP);
        } else {
          g_state.smoke_threshold = SMOKE_TH_MIN;
        }
      } else {
        g_state.smoke_threshold = SMOKE_TH_MIN;
      }
    } else {
      // Silence/reset: mute buzzer and release manual relay when no alarm.
      g_silence_latch = 1U;
      if (g_alarm_flags == 0U) {
        g_manual_relay_enable = 0U;
        g_manual_relay_value = 0U;
      }
    }
  }
}

static void on_frame_received(const ProtocolFrame* frame) {
  uint16_t cmd_id = 0U;
  if (frame->type == PROTO_TYPE_CTRL && frame->len >= 4U) {
    uint8_t code = frame->data[0];
    uint8_t val = frame->data[1];
    cmd_id = (uint16_t)frame->data[2] | ((uint16_t)frame->data[3] << 8);

    if (code == CTRL_SET_MODE) {
      if (val <= 2U) {
        g_state.mode = (SystemMode)val;
      }
    } else if (code == CTRL_SET_RELAY) {
      g_manual_relay_enable = 1U;
      g_manual_relay_value = val ? 1U : 0U;
    } else if (code == CTRL_SILENCE) {
      g_silence_latch = 1U;
    }
    send_ack(cmd_id);
  } else if (frame->type == PROTO_TYPE_PARAM && frame->len >= 5U) {
    uint8_t code = frame->data[0];
    uint16_t v = (uint16_t)frame->data[1] | ((uint16_t)frame->data[2] << 8);
    cmd_id = (uint16_t)frame->data[3] | ((uint16_t)frame->data[4] << 8);
    if (code == PARAM_SET_SMOKE_TH) {
      g_state.smoke_threshold = v;
    }
    send_ack(cmd_id);
  }
}

static void init_state(void) {
  uint8_t i;
  memset(&g_sensors, 0, sizeof(g_sensors));
  memset(&g_state, 0, sizeof(g_state));
  g_state.mode = MODE_DISARM;
  g_state.smoke_threshold = DEFAULT_SMOKE_THRESHOLD;
  g_state.net_ok = 1U;
  g_state.air780e_ok = 1U;
  for (i = 0U; i < 8U; ++i) {
    g_smoke_hist[i] = 0U;
  }
}

void HomeDevice_AppInit(void) {
  Drivers_Init();
  init_state();
  Sensor_Init();
  KeyInput_Init();
  OledUi_Init();
  CommUart_Init(on_frame_received);
  Air780E_Init();
  AlarmLogic_Init(&g_alarm_ctx);
  Scheduler_Init(Drivers_GetMs());
}

void HomeDevice_AppLoop(void) {
  uint32_t now;
  uint32_t tasks;
  now = Drivers_GetMs();
  CommUart_Poll();
  tasks = Scheduler_GetDueTasks(now);

  if (tasks & TASK_KEY) {
    handle_keys();
  }

  if (tasks & TASK_SENSOR) {
    uint16_t raw_smoke;
    Sensor_ReadFast(&g_sensors);
    raw_smoke = g_sensors.smoke;
    update_smoke_filter(raw_smoke);

    g_dht_divider++;
    if (g_dht_divider >= DHT_SAMPLE_DIV) {
      g_dht_divider = 0U;
      Sensor_ReadSlow(&g_sensors);
    }
    eval_logic();
  }

  if (tasks & TASK_OLED) {
    OledUi_Show(&g_sensors, &g_state, g_alarm_flags);
  }

  if (tasks & TASK_UPLOAD) {
    send_status_frame();
  }
}

void HomeDevice_Tick1ms(void) { Drivers_TickInc(); }
