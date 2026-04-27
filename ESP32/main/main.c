#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "gateway";

// -------------------------
// User config
// -------------------------
static const char *WIFI_SSID = "Tianxuan";
static const char *WIFI_PASS = "12345678910";
// Use Nginx public endpoint (port 80). Uvicorn is bound to 127.0.0.1:8000 on server.
static const char *SERVER_BASE = "http://114.67.122.208";
static const char *DEVICE_ID = "HD-001";

// ESP32-C3 SuperMini recommended UART pins.
static const int UART_RX_PIN = 6;  // from STM32 TX
static const int UART_TX_PIN = 7;  // to STM32 RX
static const uart_port_t UART_PORT = UART_NUM_1;
static const int UART_BAUD = 115200;

// -------------------------
// Frame protocol
// Frame: AA 55 TYPE LEN DATA... CHECK
// CHECK = (TYPE + LEN + sum(DATA)) & 0xFF
// -------------------------
#define HEAD1 0xAA
#define HEAD2 0x55
#define PROTO_MAX_DATA 64

typedef enum {
  FRAME_ENV_DATA = 0x01,
  FRAME_ALARM = 0x02,
  FRAME_CTRL = 0x03,
  FRAME_PARAM = 0x04,
  FRAME_ACK = 0x05
} frame_type_t;

typedef enum {
  CTRL_SET_MODE = 0x01,   // value: 0 DISARM, 1 ARM, 2 AUTO
  CTRL_SET_RELAY = 0x02,  // value: 0 OFF, 1 ON
  CTRL_SILENCE = 0x03     // value: 1 execute
} ctrl_code_t;

typedef enum {
  PARAM_SET_SMOKE_TH = 0x01  // value: uint16 threshold
} param_code_t;

typedef struct {
  float temperature;
  float humidity;
  uint16_t smoke;
  uint8_t flame;
  uint8_t intrusion;
  uint8_t mode;
  uint8_t relay;
  uint8_t buzzer;
  uint8_t network;
  uint8_t air780e;
  bool valid;
} runtime_status_t;

typedef struct {
  bool pending;
  uint8_t alarm_type;
  uint8_t level;
  char message[96];
} alarm_packet_t;

typedef struct {
  char *buf;
  int max_len;
  int len;
} http_response_ctx_t;

static runtime_status_t g_status;
static alarm_packet_t g_alarm;
static int g_ack_command_id = -1;
static volatile bool g_wifi_connected = false;

static uint8_t g_rx_buf[256];
static size_t g_rx_idx = 0;
static uint64_t g_last_frame_ms = 0;
static uint32_t g_rx_bytes = 0;
static uint32_t g_rx_frames_ok = 0;
static uint32_t g_rx_frames_bad = 0;
static uint32_t g_upload_ok = 0;
static uint32_t g_upload_fail = 0;
static uint32_t g_cmd_poll_ok = 0;
static uint32_t g_cmd_poll_fail = 0;

static uint64_t g_last_upload_ms = 0;
static uint64_t g_last_cmd_poll_ms = 0;
static uint64_t g_last_diag_ms = 0;
static const uint32_t UPLOAD_INTERVAL_MS = 1000;
static const uint32_t CMD_INTERVAL_MS = 1000;
static const uint32_t DIAG_INTERVAL_MS = 3000;

static EventGroupHandle_t g_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static uint8_t calc_checksum(uint8_t type, uint8_t len, const uint8_t *data) {
  uint16_t sum = (uint16_t)(type + len);
  for (uint8_t i = 0; i < len; ++i) {
    sum = (uint16_t)(sum + data[i]);
  }
  return (uint8_t)(sum & 0xFFU);
}

static void send_frame(uint8_t type, const uint8_t *data, uint8_t len) {
  uint8_t frame[PROTO_MAX_DATA + 5];
  uint8_t chk;
  int total;
  if (len > PROTO_MAX_DATA) {
    return;
  }

  chk = calc_checksum(type, len, data);
  frame[0] = HEAD1;
  frame[1] = HEAD2;
  frame[2] = type;
  frame[3] = len;
  if (len > 0) {
    memcpy(&frame[4], data, len);
  }
  frame[4 + len] = chk;
  total = (int)len + 5;
  uart_write_bytes(UART_PORT, (const char *)frame, total);
}

static const char *mode_to_text(uint8_t mode) {
  switch (mode) {
    case 1:
      return "ARM";
    case 2:
      return "AUTO";
    default:
      return "DISARM";
  }
}

static const char *alarm_type_to_text(uint8_t t) {
  switch (t) {
    case 1:
      return "SMOKE";
    case 2:
      return "FLAME";
    case 3:
      return "INTRUSION";
    default:
      return "GENERAL";
  }
}

static const char *alarm_level_to_text(uint8_t lv) {
  switch (lv) {
    case 1:
      return "WARN";
    case 2:
      return "ALARM";
    default:
      return "INFO";
  }
}

static uint8_t mode_from_payload(const char *payload) {
  if (payload == NULL) {
    return 0;
  }
  if (strcmp(payload, "ARM") == 0) {
    return 1;
  }
  if (strcmp(payload, "AUTO") == 0) {
    return 2;
  }
  return 0;
}

static bool payload_is_on(const char *payload) {
  if (payload == NULL) {
    return false;
  }
  return (strcmp(payload, "1") == 0 || strcmp(payload, "ON") == 0 || strcmp(payload, "on") == 0);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
  http_response_ctx_t *ctx = (http_response_ctx_t *)evt->user_data;
  if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0 && ctx != NULL && ctx->buf != NULL) {
    int remain = ctx->max_len - 1 - ctx->len;
    int copy_len = (evt->data_len < remain) ? evt->data_len : remain;
    if (copy_len > 0) {
      memcpy(ctx->buf + ctx->len, evt->data, (size_t)copy_len);
      ctx->len += copy_len;
      ctx->buf[ctx->len] = '\0';
    }
  }
  return ESP_OK;
}

static esp_err_t http_request(esp_http_client_method_t method, const char *url, const char *post_json, char *resp_buf, int resp_buf_size,
                              int *status_code_out) {
  http_response_ctx_t ctx = {.buf = resp_buf, .max_len = resp_buf_size, .len = 0};
  esp_http_client_config_t cfg = {
      .url = url,
      .event_handler = http_event_handler,
      .user_data = &ctx,
      .timeout_ms = 5000,
  };
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  esp_err_t err;

  if (client == NULL) {
    return ESP_FAIL;
  }

  if (resp_buf != NULL && resp_buf_size > 0) {
    resp_buf[0] = '\0';
  }

  esp_http_client_set_method(client, method);
  if (method == HTTP_METHOD_POST && post_json != NULL) {
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_json, (int)strlen(post_json));
  }

  err = esp_http_client_perform(client);
  if (status_code_out != NULL) {
    *status_code_out = (err == ESP_OK) ? esp_http_client_get_status_code(client) : -1;
  }
  esp_http_client_cleanup(client);
  return err;
}

static bool post_upload(void) {
  char url[192];
  char resp[128];
  int status_code = 0;
  bool ok = false;
  cJSON *root = NULL;
  char *body = NULL;

  if (!g_wifi_connected || !g_status.valid) {
    g_upload_fail++;
    return false;
  }

  root = cJSON_CreateObject();
  if (root == NULL) {
    return false;
  }

  cJSON_AddStringToObject(root, "device_id", DEVICE_ID);
  cJSON_AddNumberToObject(root, "temperature", g_status.temperature);
  cJSON_AddNumberToObject(root, "humidity", g_status.humidity);
  cJSON_AddNumberToObject(root, "smoke", g_status.smoke);
  cJSON_AddNumberToObject(root, "flame", g_status.flame);
  cJSON_AddNumberToObject(root, "intrusion", g_status.intrusion);
  cJSON_AddStringToObject(root, "mode", mode_to_text(g_status.mode));
  cJSON_AddNumberToObject(root, "relay", g_status.relay);
  cJSON_AddNumberToObject(root, "buzzer", g_status.buzzer);
  cJSON_AddNumberToObject(root, "network", g_wifi_connected ? 1 : 0);
  cJSON_AddNumberToObject(root, "air780e", g_status.air780e);

  if (g_alarm.pending) {
    cJSON_AddStringToObject(root, "alarm_type", alarm_type_to_text(g_alarm.alarm_type));
    cJSON_AddStringToObject(root, "alarm_level", alarm_level_to_text(g_alarm.level));
    cJSON_AddStringToObject(root, "alarm_message", g_alarm.message);
  }
  if (g_ack_command_id > 0) {
    cJSON_AddNumberToObject(root, "ack_command_id", g_ack_command_id);
  }

  body = cJSON_PrintUnformatted(root);
  if (body == NULL) {
    cJSON_Delete(root);
    return false;
  }

  snprintf(url, sizeof(url), "%s/api/device/upload", SERVER_BASE);
  if (http_request(HTTP_METHOD_POST, url, body, resp, sizeof(resp), &status_code) == ESP_OK && status_code >= 200 && status_code < 300) {
    g_alarm.pending = false;
    g_ack_command_id = -1;
    g_upload_ok++;
    ok = true;
  } else {
    g_upload_fail++;
  }

  cJSON_free(body);
  cJSON_Delete(root);
  return ok;
}

static bool send_command_to_stm32(const char *cmd_type, const char *payload, int cmd_id) {
  if (cmd_type == NULL) {
    return false;
  }

  if (strcmp(cmd_type, "SET_MODE") == 0) {
    uint8_t data[4];
    data[0] = CTRL_SET_MODE;
    data[1] = mode_from_payload(payload);
    data[2] = (uint8_t)(cmd_id & 0xFF);
    data[3] = (uint8_t)((cmd_id >> 8) & 0xFF);
    send_frame(FRAME_CTRL, data, sizeof(data));
    return true;
  }

  if (strcmp(cmd_type, "SET_RELAY") == 0) {
    uint8_t data[4];
    data[0] = CTRL_SET_RELAY;
    data[1] = payload_is_on(payload) ? 1 : 0;
    data[2] = (uint8_t)(cmd_id & 0xFF);
    data[3] = (uint8_t)((cmd_id >> 8) & 0xFF);
    send_frame(FRAME_CTRL, data, sizeof(data));
    return true;
  }

  if (strcmp(cmd_type, "SILENCE") == 0) {
    uint8_t data[4];
    data[0] = CTRL_SILENCE;
    data[1] = 1;
    data[2] = (uint8_t)(cmd_id & 0xFF);
    data[3] = (uint8_t)((cmd_id >> 8) & 0xFF);
    send_frame(FRAME_CTRL, data, sizeof(data));
    return true;
  }

  if (strcmp(cmd_type, "SET_SMOKE_TH") == 0) {
    int threshold = (payload == NULL) ? 0 : atoi(payload);
    uint8_t data[5];
    data[0] = PARAM_SET_SMOKE_TH;
    data[1] = (uint8_t)(threshold & 0xFF);
    data[2] = (uint8_t)((threshold >> 8) & 0xFF);
    data[3] = (uint8_t)(cmd_id & 0xFF);
    data[4] = (uint8_t)((cmd_id >> 8) & 0xFF);
    send_frame(FRAME_PARAM, data, sizeof(data));
    return true;
  }

  return false;
}

static void poll_command(void) {
  char url[192];
  char resp[1024];
  int status_code = 0;

  if (!g_wifi_connected) {
    return;
  }

  snprintf(url, sizeof(url), "%s/api/device/cmd?device_id=%s", SERVER_BASE, DEVICE_ID);
  if (http_request(HTTP_METHOD_GET, url, NULL, resp, sizeof(resp), &status_code) != ESP_OK || status_code < 200 || status_code >= 300) {
    g_cmd_poll_fail++;
    return;
  }
  g_cmd_poll_ok++;

  cJSON *root = cJSON_Parse(resp);
  if (root == NULL) {
    return;
  }

  cJSON *has_command = cJSON_GetObjectItemCaseSensitive(root, "has_command");
  if (!cJSON_IsTrue(has_command)) {
    cJSON_Delete(root);
    return;
  }

  cJSON *command = cJSON_GetObjectItemCaseSensitive(root, "command");
  cJSON *id_item = (command != NULL) ? cJSON_GetObjectItemCaseSensitive(command, "id") : NULL;
  cJSON *type_item = (command != NULL) ? cJSON_GetObjectItemCaseSensitive(command, "command_type") : NULL;
  cJSON *payload_item = (command != NULL) ? cJSON_GetObjectItemCaseSensitive(command, "payload") : NULL;
  char payload_buf[32] = {0};
  const char *payload = "";

  if (!cJSON_IsNumber(id_item) || !cJSON_IsString(type_item)) {
    cJSON_Delete(root);
    return;
  }

  if (cJSON_IsString(payload_item) && payload_item->valuestring != NULL) {
    payload = payload_item->valuestring;
  } else if (cJSON_IsNumber(payload_item)) {
    snprintf(payload_buf, sizeof(payload_buf), "%d", payload_item->valueint);
    payload = payload_buf;
  }

  send_command_to_stm32(type_item->valuestring, payload, id_item->valueint);
  cJSON_Delete(root);
}

static void handle_frame(uint8_t type, const uint8_t *data, uint8_t len) {
  g_last_frame_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
  if (type == FRAME_ENV_DATA && len >= 10) {
    // [temp, hum, smokeL, smokeH, flame, intrusion, mode, relay, buzzer, air780e]
    g_status.temperature = (float)data[0];
    g_status.humidity = (float)data[1];
    g_status.smoke = (uint16_t)(data[2] | (data[3] << 8));
    g_status.flame = data[4];
    g_status.intrusion = data[5];
    g_status.mode = data[6];
    g_status.relay = data[7];
    g_status.buzzer = data[8];
    g_status.air780e = data[9];
    g_status.network = g_wifi_connected ? 1 : 0;
    g_status.valid = true;
    ESP_LOGI(TAG, "ENV t=%.0f h=%.0f smoke=%u flame=%u intr=%u mode=%u relay=%u buz=%u", g_status.temperature, g_status.humidity,
             g_status.smoke, g_status.flame, g_status.intrusion, g_status.mode, g_status.relay, g_status.buzzer);
  } else if (type == FRAME_ALARM && len >= 2) {
    g_alarm.pending = true;
    g_alarm.alarm_type = data[0];
    g_alarm.level = data[1];
    if (len > 2) {
      int msg_len = (int)len - 2;
      if (msg_len > (int)sizeof(g_alarm.message) - 1) {
        msg_len = (int)sizeof(g_alarm.message) - 1;
      }
      memcpy(g_alarm.message, &data[2], (size_t)msg_len);
      g_alarm.message[msg_len] = '\0';
    } else {
      strcpy(g_alarm.message, "alarm from stm32");
    }
    ESP_LOGW(TAG, "ALARM type=%u level=%u msg=%s", g_alarm.alarm_type, g_alarm.level, g_alarm.message);
  } else if (type == FRAME_ACK && len >= 2) {
    g_ack_command_id = (int)(data[0] | (data[1] << 8));
    ESP_LOGI(TAG, "ACK command_id=%d", g_ack_command_id);
  }
}

static void process_uart(void) {
  uint8_t tmp[64];
  int n = uart_read_bytes(UART_PORT, tmp, sizeof(tmp), 0);
  if (n > 0) {
    g_rx_bytes += (uint32_t)n;
  }
  for (int k = 0; k < n; ++k) {
    uint8_t b = tmp[k];
    if (g_rx_idx == 0 && b != HEAD1) {
      continue;
    }
    if (g_rx_idx == 1 && b != HEAD2) {
      g_rx_idx = 0;
      continue;
    }

    g_rx_buf[g_rx_idx++] = b;
    if (g_rx_idx >= 4) {
      uint8_t len = g_rx_buf[3];
      size_t frame_len = (size_t)len + 5;
      if (frame_len > sizeof(g_rx_buf) || len > PROTO_MAX_DATA) {
        g_rx_idx = 0;
        continue;
      }
      if (g_rx_idx == frame_len) {
        uint8_t type = g_rx_buf[2];
        uint8_t chk = g_rx_buf[frame_len - 1];
        uint8_t calc = calc_checksum(type, len, &g_rx_buf[4]);
        if (chk == calc) {
          g_rx_frames_ok++;
          handle_frame(type, &g_rx_buf[4], len);
        } else {
          g_rx_frames_bad++;
        }
        g_rx_idx = 0;
      }
    }
  }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  (void)arg;
  (void)event_data;

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    g_wifi_connected = false;
    xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    g_wifi_connected = true;
    xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static void wifi_init_sta(void) {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  wifi_config_t wifi_cfg = {0};

  g_wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  strncpy((char *)wifi_cfg.sta.ssid, WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
  strncpy((char *)wifi_cfg.sta.password, WIFI_PASS, sizeof(wifi_cfg.sta.password) - 1);
  wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
  wifi_cfg.sta.pmf_cfg.capable = true;
  wifi_cfg.sta.pmf_cfg.required = false;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "wifi_init_sta finished");
}

static void uart_init(void) {
  uart_config_t uart_config = {
      .baud_rate = UART_BAUD,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 1024, 0, 0, NULL, 0));
  ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  memset(&g_status, 0, sizeof(g_status));
  memset(&g_alarm, 0, sizeof(g_alarm));

  uart_init();
  wifi_init_sta();

  ESP_LOGI(TAG, "gateway start, UART RX=%d TX=%d", UART_RX_PIN, UART_TX_PIN);

  while (1) {
    uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
    process_uart();

    if (now_ms - g_last_upload_ms >= UPLOAD_INTERVAL_MS) {
      g_last_upload_ms = now_ms;
      post_upload();
    }
    if (now_ms - g_last_cmd_poll_ms >= CMD_INTERVAL_MS) {
      g_last_cmd_poll_ms = now_ms;
      poll_command();
    }
    if (now_ms - g_last_diag_ms >= DIAG_INTERVAL_MS) {
      g_last_diag_ms = now_ms;
      ESP_LOGI(TAG,
               "diag wifi=%d valid=%d rx_bytes=%lu frames_ok=%lu bad=%lu last_frame_ago=%llums smoke=%u upload_ok=%lu upload_fail=%lu cmd_ok=%lu cmd_fail=%lu",
               g_wifi_connected ? 1 : 0, g_status.valid ? 1 : 0, (unsigned long)g_rx_bytes, (unsigned long)g_rx_frames_ok,
               (unsigned long)g_rx_frames_bad, (unsigned long long)(g_last_frame_ms ? (now_ms - g_last_frame_ms) : 0), g_status.smoke,
               (unsigned long)g_upload_ok, (unsigned long)g_upload_fail, (unsigned long)g_cmd_poll_ok, (unsigned long)g_cmd_poll_fail);
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
