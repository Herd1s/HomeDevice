#include "drivers.h"

#include "OLED.h"
#include "misc.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include <string.h>

static volatile uint32_t s_ms_tick = 0U;

static uint8_t s_uart1_rx_buf[128];
static volatile uint16_t s_uart1_rx_head = 0U;
static volatile uint16_t s_uart1_rx_tail = 0U;

static void gpio_init(void) {
  GPIO_InitTypeDef gpio;

  RCC_APB2PeriphClockCmd(
      RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);

  GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

  gpio.GPIO_Mode = GPIO_Mode_Out_PP;
  gpio.GPIO_Speed = GPIO_Speed_50MHz;
  gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
  GPIO_Init(GPIOB, &gpio);
  GPIO_ResetBits(GPIOB, GPIO_Pin_0);
  GPIO_SetBits(GPIOB, GPIO_Pin_1);

  gpio.GPIO_Mode = GPIO_Mode_IPU;
  gpio.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
  GPIO_Init(GPIOB, &gpio);

  gpio.GPIO_Mode = GPIO_Mode_IPU;
  gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8;
  GPIO_Init(GPIOA, &gpio);

  gpio.GPIO_Mode = GPIO_Mode_AIN;
  gpio.GPIO_Pin = GPIO_Pin_0;
  GPIO_Init(GPIOA, &gpio);
}

static void adc1_init(void) {
  ADC_InitTypeDef adc;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
  RCC_ADCCLKConfig(RCC_PCLK2_Div6);

  ADC_DeInit(ADC1);
  adc.ADC_Mode = ADC_Mode_Independent;
  adc.ADC_ScanConvMode = DISABLE;
  adc.ADC_ContinuousConvMode = DISABLE;
  adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
  adc.ADC_DataAlign = ADC_DataAlign_Right;
  adc.ADC_NbrOfChannel = 1;
  ADC_Init(ADC1, &adc);

  ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5);
  ADC_Cmd(ADC1, ENABLE);

  ADC_ResetCalibration(ADC1);
  while (ADC_GetResetCalibrationStatus(ADC1) == SET) {
  }
  ADC_StartCalibration(ADC1);
  while (ADC_GetCalibrationStatus(ADC1) == SET) {
  }
}

static void usart1_init(void) {
  GPIO_InitTypeDef gpio;
  USART_InitTypeDef usart;
  NVIC_InitTypeDef nvic;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

  gpio.GPIO_Pin = GPIO_Pin_9;
  gpio.GPIO_Speed = GPIO_Speed_50MHz;
  gpio.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &gpio);

  gpio.GPIO_Pin = GPIO_Pin_10;
  gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &gpio);

  usart.USART_BaudRate = 115200;
  usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  usart.USART_Parity = USART_Parity_No;
  usart.USART_StopBits = USART_StopBits_1;
  usart.USART_WordLength = USART_WordLength_8b;
  USART_Init(USART1, &usart);
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  USART_Cmd(USART1, ENABLE);

  nvic.NVIC_IRQChannel = USART1_IRQn;
  nvic.NVIC_IRQChannelCmd = ENABLE;
  nvic.NVIC_IRQChannelPreemptionPriority = 1;
  nvic.NVIC_IRQChannelSubPriority = 1;
  NVIC_Init(&nvic);
}

static void usart2_init(void) {
  GPIO_InitTypeDef gpio;
  USART_InitTypeDef usart;

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

  gpio.GPIO_Pin = GPIO_Pin_2;
  gpio.GPIO_Speed = GPIO_Speed_50MHz;
  gpio.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &gpio);

  gpio.GPIO_Pin = GPIO_Pin_3;
  gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &gpio);

  usart.USART_BaudRate = 115200;
  usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  usart.USART_Parity = USART_Parity_No;
  usart.USART_StopBits = USART_StopBits_1;
  usart.USART_WordLength = USART_WordLength_8b;
  USART_Init(USART2, &usart);
  USART_Cmd(USART2, ENABLE);
}

void Drivers_Init(void) {
  gpio_init();
  adc1_init();
  usart1_init();
  usart2_init();
  OLED_Init();
  SysTick_Config(SystemCoreClock / 1000U);
}

void Drivers_TickInc(void) { s_ms_tick++; }

uint32_t Drivers_GetMs(void) { return s_ms_tick; }

void USART1_IRQHandler(void) {
  if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
    uint8_t byte = (uint8_t)USART_ReceiveData(USART1);
    uint16_t next = (uint16_t)((s_uart1_rx_head + 1U) % sizeof(s_uart1_rx_buf));
    if (next != s_uart1_rx_tail) {
      s_uart1_rx_buf[s_uart1_rx_head] = byte;
      s_uart1_rx_head = next;
    }
    USART_ClearITPendingBit(USART1, USART_IT_RXNE);
  }
}

int Drivers_Uart1_ReadByte(uint8_t* out_byte) {
  if (s_uart1_rx_head == s_uart1_rx_tail) {
    return 0;
  }
  *out_byte = s_uart1_rx_buf[s_uart1_rx_tail];
  s_uart1_rx_tail = (uint16_t)((s_uart1_rx_tail + 1U) % sizeof(s_uart1_rx_buf));
  return 1;
}

static void uart_send_bytes(USART_TypeDef* usart, const uint8_t* data, uint16_t len) {
  uint16_t i;
  for (i = 0; i < len; ++i) {
    while (USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET) {
    }
    USART_SendData(usart, data[i]);
  }
  while (USART_GetFlagStatus(usart, USART_FLAG_TC) == RESET) {
  }
}

void Drivers_Uart1_Send(const uint8_t* data, uint16_t len) { uart_send_bytes(USART1, data, len); }

void Drivers_Uart2_Send(const uint8_t* data, uint16_t len) { uart_send_bytes(USART2, data, len); }

uint8_t Drivers_ReadDht11(int8_t* out_temp, uint8_t* out_humi) {
  // TODO: replace with real DHT11 timing implementation on PA8.
  *out_temp = 25;
  *out_humi = 50;
  return 1;
}

uint16_t Drivers_ReadMq2Adc(void) {
  uint32_t timeout = 100000;
  ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5);
  ADC_SoftwareStartConvCmd(ADC1, ENABLE);
  while ((ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) && timeout--) {
  }
  return (uint16_t)ADC_GetConversionValue(ADC1);
}

uint8_t Drivers_ReadFlame(void) { return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) ? 0U : 1U; }

uint8_t Drivers_ReadIntrusion(void) { return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) ? 0U : 1U; }

void Drivers_SetBuzzer(uint8_t on) {
  if (on) {
    GPIO_SetBits(GPIOB, GPIO_Pin_0);
  } else {
    GPIO_ResetBits(GPIOB, GPIO_Pin_0);
  }
}

void Drivers_SetRelay(uint8_t on) {
  if (on) {
    GPIO_ResetBits(GPIOB, GPIO_Pin_1);
  } else {
    GPIO_SetBits(GPIOB, GPIO_Pin_1);
  }
}

uint8_t Drivers_KeyReadRaw(uint8_t index) {
  switch (index) {
    case 0:
      return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) ? 0U : 1U;
    case 1:
      return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) ? 0U : 1U;
    case 2:
      return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) ? 0U : 1U;
    case 3:
      return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) ? 0U : 1U;
    default:
      return 0U;
  }
}

void Drivers_OledClear(void) { OLED_Clear(); }

void Drivers_OledPrint(uint8_t row, uint8_t col, const char* text) {
  char buf[17];
  uint8_t i = 0U;
  uint8_t max_chars;

  if (text == 0) {
    return;
  }
  if (row >= 4U || col >= 16U) {
    return;
  }

  max_chars = (uint8_t)(16U - col);
  while (text[i] != '\0' && i < max_chars) {
    buf[i] = text[i];
    i++;
  }
  buf[i] = '\0';
  OLED_ShowString((uint8_t)(row + 1U), (uint8_t)(col + 1U), buf);
}

void Drivers_OledFlush(void) {}

uint8_t Drivers_IsNightTime(void) {
  // TODO: replace with RTC based schedule.
  return 1U;
}
