# ESP32-C3 Gateway (ESP-IDF)

This program is a network gateway between STM32 and server, implemented with standard ESP-IDF project layout.

## Features

- UART protocol parsing from STM32 (`AA 55 TYPE LEN DATA CHECK`)
- Upload status to server: `POST /api/device/upload`
- Poll remote command: `GET /api/device/cmd`
- Convert command to UART frame and forward to STM32

## Project Layout

- `CMakeLists.txt`
- `main/CMakeLists.txt`
- `main/main.c`

## Before Build

Edit these constants in `main/main.c`:

- `WIFI_SSID`
- `WIFI_PASS`
- `SERVER_BASE`
- `DEVICE_ID`
- `UART_RX_PIN` / `UART_TX_PIN`

Recommended for ESP32-C3 SuperMini:

- `UART_RX_PIN = 6`
- `UART_TX_PIN = 7`

Wiring with STM32:

- STM32 `PA9(TX1)` -> ESP32 `GPIO6(RX)`
- STM32 `PA10(RX1)` <- ESP32 `GPIO7(TX)`
- `GND` <-> `GND`

## Build / Flash (ESP-IDF)

```powershell
cd ESP32
idf.py set-target esp32c3
idf.py build
idf.py -p COM16 flash monitor
```

If your port is not `COM16`, replace it with your actual serial port.

## Notes

- This folder still keeps old `platformio.ini`/`src` for reference, but active build path is ESP-IDF (`main/`).
