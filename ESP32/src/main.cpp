#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

// -------------------------
// User config
// -------------------------
static const char* WIFI_SSID = "Tianxuan";
static const char* WIFI_PASS = "12345678910";
static const char* SERVER_BASE = "http://42.192.113.88";
static const char* DEVICE_ID = "HD-001";

// UART pins for ESP32-C3 SuperMini.
// Avoid GPIO20/21 to prevent conflict with onboard USB-UART on some boards.
static const int UART_RX_PIN = 6;  // from STM32 TX
static const int UART_TX_PIN = 7;  // to STM32 RX

// -------------------------
// Frame protocol
// Frame: AA 55 TYPE LEN DATA... CHECK
// CHECK = (TYPE + LEN + sum(DATA)) & 0xFF
// -------------------------
static const uint8_t HEAD1 = 0xAA;
static const uint8_t HEAD2 = 0x55;

enum FrameType : uint8_t {
  FRAME_ENV_DATA = 0x01,
  FRAME_ALARM = 0x02,
  FRAME_CTRL = 0x03,
  FRAME_PARAM = 0x04,
  FRAME_ACK = 0x05
};

enum CtrlCode : uint8_t {
  CTRL_SET_MODE = 0x01,      // value: 0 DISARM, 1 ARM, 2 AUTO
  CTRL_SET_RELAY = 0x02,     // value: 0 OFF, 1 ON
  CTRL_SILENCE = 0x03        // value: 1 execute
};

enum ParamCode : uint8_t {
  PARAM_SET_SMOKE_TH = 0x01  // value: uint16 threshold
};

struct RuntimeStatus {
  float temperature = 0;
  float humidity = 0;
  uint16_t smoke = 0;
  uint8_t flame = 0;
  uint8_t intrusion = 0;
  uint8_t mode = 0;
  uint8_t relay = 0;
  uint8_t buzzer = 0;
  uint8_t network = 1;
  uint8_t air780e = 1;
  bool valid = false;
};

struct AlarmPacket {
  bool pending = false;
  uint8_t alarmType = 0;
  uint8_t level = 0;
  String message;
};

RuntimeStatus g_status;
AlarmPacket g_alarm;
int g_ackCommandId = -1;

unsigned long g_lastUploadMs = 0;
unsigned long g_lastCmdPollMs = 0;
const unsigned long UPLOAD_INTERVAL_MS = 1000;
const unsigned long CMD_INTERVAL_MS = 1000;

// RX parser state
uint8_t rxBuf[256];
size_t rxIdx = 0;

uint8_t calcChecksum(uint8_t type, uint8_t len, const uint8_t* data) {
  uint16_t sum = type + len;
  for (uint8_t i = 0; i < len; ++i) {
    sum += data[i];
  }
  return static_cast<uint8_t>(sum & 0xFF);
}

void sendFrame(uint8_t type, const uint8_t* data, uint8_t len) {
  uint8_t chk = calcChecksum(type, len, data);
  Serial1.write(HEAD1);
  Serial1.write(HEAD2);
  Serial1.write(type);
  Serial1.write(len);
  if (len > 0) {
    Serial1.write(data, len);
  }
  Serial1.write(chk);
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200);
  }
}

String modeToText(uint8_t mode) {
  switch (mode) {
    case 1:
      return "ARM";
    case 2:
      return "AUTO";
    default:
      return "DISARM";
  }
}

String alarmTypeToText(uint8_t t) {
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

String alarmLevelToText(uint8_t lv) {
  switch (lv) {
    case 1:
      return "WARN";
    case 2:
      return "ALARM";
    default:
      return "INFO";
  }
}

bool postUpload() {
  if (WiFi.status() != WL_CONNECTED || !g_status.valid) {
    return false;
  }

  HTTPClient http;
  String url = String(SERVER_BASE) + "/api/device/upload";
  if (!http.begin(url)) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;
  doc["device_id"] = DEVICE_ID;
  doc["temperature"] = g_status.temperature;
  doc["humidity"] = g_status.humidity;
  doc["smoke"] = g_status.smoke;
  doc["flame"] = g_status.flame;
  doc["intrusion"] = g_status.intrusion;
  doc["mode"] = modeToText(g_status.mode);
  doc["relay"] = g_status.relay;
  doc["buzzer"] = g_status.buzzer;
  doc["network"] = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
  doc["air780e"] = g_status.air780e;

  if (g_alarm.pending) {
    doc["alarm_type"] = alarmTypeToText(g_alarm.alarmType);
    doc["alarm_level"] = alarmLevelToText(g_alarm.level);
    doc["alarm_message"] = g_alarm.message;
  }
  if (g_ackCommandId > 0) {
    doc["ack_command_id"] = g_ackCommandId;
  }

  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  http.end();
  if (code >= 200 && code < 300) {
    g_alarm.pending = false;
    g_ackCommandId = -1;
    return true;
  }
  return false;
}

uint8_t modeFromPayload(const String& payload) {
  if (payload == "ARM") return 1;
  if (payload == "AUTO") return 2;
  return 0;
}

bool sendCommandToSTM32(const String& cmdType, const String& payload, int cmdId) {
  if (cmdType == "SET_MODE") {
    uint8_t data[4];
    data[0] = CTRL_SET_MODE;
    data[1] = modeFromPayload(payload);
    data[2] = static_cast<uint8_t>(cmdId & 0xFF);
    data[3] = static_cast<uint8_t>((cmdId >> 8) & 0xFF);
    sendFrame(FRAME_CTRL, data, sizeof(data));
    return true;
  }
  if (cmdType == "SET_RELAY") {
    uint8_t data[4];
    data[0] = CTRL_SET_RELAY;
    data[1] = (payload == "1" || payload == "ON") ? 1 : 0;
    data[2] = static_cast<uint8_t>(cmdId & 0xFF);
    data[3] = static_cast<uint8_t>((cmdId >> 8) & 0xFF);
    sendFrame(FRAME_CTRL, data, sizeof(data));
    return true;
  }
  if (cmdType == "SILENCE") {
    uint8_t data[4];
    data[0] = CTRL_SILENCE;
    data[1] = 1;
    data[2] = static_cast<uint8_t>(cmdId & 0xFF);
    data[3] = static_cast<uint8_t>((cmdId >> 8) & 0xFF);
    sendFrame(FRAME_CTRL, data, sizeof(data));
    return true;
  }
  if (cmdType == "SET_SMOKE_TH") {
    int threshold = payload.toInt();
    uint8_t data[5];
    data[0] = PARAM_SET_SMOKE_TH;
    data[1] = static_cast<uint8_t>(threshold & 0xFF);
    data[2] = static_cast<uint8_t>((threshold >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(cmdId & 0xFF);
    data[4] = static_cast<uint8_t>((cmdId >> 8) & 0xFF);
    sendFrame(FRAME_PARAM, data, sizeof(data));
    return true;
  }
  return false;
}

void pollCommand() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  HTTPClient http;
  String url = String(SERVER_BASE) + "/api/device/cmd?device_id=" + DEVICE_ID;
  if (!http.begin(url)) {
    return;
  }
  int code = http.GET();
  if (code < 200 || code >= 300) {
    http.end();
    return;
  }

  String rsp = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, rsp);
  if (err) {
    return;
  }
  if (!doc["has_command"].as<bool>()) {
    return;
  }

  JsonObject cmd = doc["command"];
  int cmdId = cmd["id"] | 0;
  String cmdType = cmd["command_type"] | "";
  String payload = cmd["payload"] | "";
  sendCommandToSTM32(cmdType, payload, cmdId);
}

void handleFrame(uint8_t type, const uint8_t* data, uint8_t len) {
  if (type == FRAME_ENV_DATA && len >= 10) {
    // [temp, hum, smokeL, smokeH, flame, intrusion, mode, relay, buzzer, air780e]
    g_status.temperature = static_cast<float>(data[0]);
    g_status.humidity = static_cast<float>(data[1]);
    g_status.smoke = static_cast<uint16_t>(data[2] | (data[3] << 8));
    g_status.flame = data[4];
    g_status.intrusion = data[5];
    g_status.mode = data[6];
    g_status.relay = data[7];
    g_status.buzzer = data[8];
    g_status.air780e = data[9];
    g_status.network = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
    g_status.valid = true;
  } else if (type == FRAME_ALARM && len >= 2) {
    g_alarm.pending = true;
    g_alarm.alarmType = data[0];
    g_alarm.level = data[1];
    if (len > 2) {
      String msg = "";
      for (uint8_t i = 2; i < len; ++i) {
        msg += static_cast<char>(data[i]);
      }
      g_alarm.message = msg;
    } else {
      g_alarm.message = "alarm from stm32";
    }
  } else if (type == FRAME_ACK && len >= 2) {
    g_ackCommandId = static_cast<int>(data[0] | (data[1] << 8));
  }
}

void processSerial() {
  while (Serial1.available()) {
    uint8_t b = static_cast<uint8_t>(Serial1.read());
    if (rxIdx == 0 && b != HEAD1) {
      continue;
    }
    if (rxIdx == 1 && b != HEAD2) {
      rxIdx = 0;
      continue;
    }

    rxBuf[rxIdx++] = b;

    if (rxIdx >= 4) {
      uint8_t len = rxBuf[3];
      size_t frameLen = static_cast<size_t>(len) + 5;
      if (frameLen > sizeof(rxBuf)) {
        rxIdx = 0;
        continue;
      }
      if (rxIdx == frameLen) {
        uint8_t type = rxBuf[2];
        uint8_t chk = rxBuf[frameLen - 1];
        const uint8_t* data = &rxBuf[4];
        uint8_t calc = calcChecksum(type, len, data);
        if (chk == calc) {
          handleFrame(type, data, len);
        }
        rxIdx = 0;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  WiFi.mode(WIFI_STA);
  connectWiFi();
}

void loop() {
  connectWiFi();
  processSerial();

  unsigned long now = millis();
  if (now - g_lastUploadMs >= UPLOAD_INTERVAL_MS) {
    g_lastUploadMs = now;
    postUpload();
  }
  if (now - g_lastCmdPollMs >= CMD_INTERVAL_MS) {
    g_lastCmdPollMs = now;
    pollCommand();
  }
}
