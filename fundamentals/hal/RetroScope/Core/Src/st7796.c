/*
 * st7796.c
 *
 *  Created on: Nov 14, 2026
 *      Author: pedrol.valle-cruz
 */

#include "st7796.h"
#include "gpio.h"
#include "spi.h"

// Private function declarations
static void ST7796_Reset(void);
static HAL_StatusTypeDef ST7796_WriteCommand(uint8_t command);
static HAL_StatusTypeDef ST7796_WriteData(uint8_t data);

// Public functions
void ST7796_Init(void) {
  ST7796_Reset();

  // Exit sleep mode
  if (ST7796_WriteCommand(0x11) != HAL_OK) {
    Error_Handler();
  }
  HAL_Delay(120);

  // Set pixel format to RGB565 (16 bits per pixel)
  if (ST7796_WriteCommand(0x3A) != HAL_OK) {
    Error_Handler();
  }

  if (ST7796_WriteData(0x55) != HAL_OK) {
    Error_Handler();
  }

  // Configure landscape orientation
  if (ST7796_WriteCommand(0x36) != HAL_OK) {
    Error_Handler();
  }

  if (ST7796_WriteData(0x28) != HAL_OK) {
    Error_Handler();
  }

  // Turn the display on
  if (ST7796_WriteCommand(0x29) != HAL_OK) {
    Error_Handler();
  }

  HAL_Delay(20);
}

void ST7796_FillScreen(uint16_t color) {
  // Set column address: 0 to 479
  if (ST7796_WriteCommand(0x2A) != HAL_OK) {
    Error_Handler();
  }

  uint8_t columns[] = {
    0x00, 0x00,
    0x01, 0xDF
  };

  for (uint8_t i = 0; i < sizeof(columns); i++) {
    if (ST7796_WriteData(columns[i]) != HAL_OK) {
      Error_Handler();
    }
  }

  // Set row address: 0 to 319
  if (ST7796_WriteCommand(0x2B) != HAL_OK) {
    Error_Handler();
  }

  uint8_t rows[] = {
    0x00, 0x00,
    0x01, 0x3F
  };

  for (uint8_t i = 0; i < sizeof(rows); i++) {
    if (ST7796_WriteData(rows[i]) != HAL_OK) {
      Error_Handler();
    }
  }

  // Begin writing pixel data
  if (ST7796_WriteCommand(0x2C) != HAL_OK) {
    Error_Handler();
  }

  uint8_t pixel_data[128];

  // Each pixel contains two bytes (RGB565)
  for (uint16_t i = 0; i < sizeof(pixel_data); i += 2) {
    pixel_data[i] = (uint8_t)(color >> 8);
    pixel_data[i + 1] = (uint8_t)(color & 0xFF);
  }

  // Select display and enter data mode
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

  // 64 pixels per transmission
  const uint32_t pixel_count = ST7796_WIDTH * ST7796_HEIGHT;
  const uint32_t pixels_per_transfer = sizeof(pixel_data) / 2;

  for (uint32_t i = 0; i < pixel_count; i += pixels_per_transfer) {
    if (HAL_SPI_Transmit(&hspi1, pixel_data, sizeof(pixel_data), 1000) != HAL_OK) {
      HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
      Error_Handler();
    }
  }

  // Deselect display
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

// Private functions
static void ST7796_Reset(void) {
  // Ensure reset starts inactive (HIGH)
  HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(10);

  // Assert reset (LOW)
  HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(20);

  // Release reset (HIGH)
  HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(120);
}

static HAL_StatusTypeDef ST7796_WriteCommand(uint8_t command) {
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

  HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi1, &command, 1, 100);

  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  return status;
}

static HAL_StatusTypeDef ST7796_WriteData(uint8_t data) {
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

  HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi1, &data, 1, 100);

  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  return status;
}

void ST7796_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
  // Prevent coordinates outside the display boundaries
  if (x >= ST7796_WIDTH || y >= ST7796_HEIGHT) {
    return;
  }

  // Set column address
  if (ST7796_WriteCommand(0x2A) != HAL_OK) {
    Error_Handler();
  }

  if (ST7796_WriteData(x >> 8) != HAL_OK ||
      ST7796_WriteData(x & 0xFF) != HAL_OK ||
      ST7796_WriteData(x >> 8) != HAL_OK ||
      ST7796_WriteData(x & 0xFF) != HAL_OK) {
    Error_Handler();
  }

  // Set row address
  if (ST7796_WriteCommand(0x2B) != HAL_OK) {
    Error_Handler();
  }

  if (ST7796_WriteData(y >> 8) != HAL_OK ||
      ST7796_WriteData(y & 0xFF) != HAL_OK ||
      ST7796_WriteData(y >> 8) != HAL_OK ||
      ST7796_WriteData(y & 0xFF) != HAL_OK) {
    Error_Handler();
  }

  // Begin memory write
  if (ST7796_WriteCommand(0x2C) != HAL_OK) {
    Error_Handler();
  }

  // Send RGB565 pixel
  if (ST7796_WriteData(color >> 8) != HAL_OK ||
      ST7796_WriteData(color & 0xFF) != HAL_OK) {
    Error_Handler();
  }
}
