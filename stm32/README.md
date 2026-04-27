# STM32F103C8T6 Firmware Skeleton

This folder provides a Standard-Peripheral-Library friendly firmware structure.

## 1. File layout

- `Inc/app_types.h`: shared data model
- `Inc/protocol.h` + `Src/protocol.c`: UART frame protocol
- `Src/main.c`: scheduler + alarm logic + command handling
- `Src/drivers_port_template.c`: hardware abstraction template
- `Src/air780e_sms.c`: AT command SMS sending

## 2. Pin mapping (recommended)

- `PA0`: MQ2 ADC
- `PA8`: DHT11 data
- `PB12`: flame digital input
- `PB13`: intrusion digital input
- `PB10/PB11`: OLED I2C (soft)
- `PB0`: buzzer
- `PB1`: relay
- `PA9/PA10`: USART1 <-> ESP32-C3
- `PA2/PA3`: USART2 <-> Air780E
- `PB14/PB15/PA6/PA7`: KEY1..KEY4

## 3. Next migration steps

1. Replace all stub functions in `drivers_port_template.c` with real SPL code.
2. Add UART IRQ ring buffer in your board support package.
3. Map key scan polarity based on hardware pull-up/pull-down.
4. Verify Air780E AT response parsing if you need delivery retry.

