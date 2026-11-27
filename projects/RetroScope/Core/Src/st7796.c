/*
 * st7796.c
 *
 *  Created on: Nov 14, 2026
 *      Author: pedrol.valle-cruz
 */

#include <stdlib.h>
#include <stddef.h>

#include "gpio.h"
#include "spi.h"
#include "st7796.h"
#include "font5x7.h"

// Private function declarations
static void ST7796_Reset(void);
static HAL_StatusTypeDef ST7796_WriteCommand(uint8_t command);
static HAL_StatusTypeDef ST7796_WriteData(uint8_t data);

volatile uint32_t SpiTransferCycles = 0U;

// Public functions
static void ST7796_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1,
                                    uint16_t y1) {
  // Set column address range
  if (ST7796_WriteCommand(0x2A) != HAL_OK) {
    Error_Handler();
  }

  if (ST7796_WriteData(x0 >> 8) != HAL_OK ||
      ST7796_WriteData(x0 & 0xFF) != HAL_OK ||
      ST7796_WriteData(x1 >> 8) != HAL_OK ||
      ST7796_WriteData(x1 & 0xFF) != HAL_OK) {
    Error_Handler();
  }

  // Set row address range
  if (ST7796_WriteCommand(0x2B) != HAL_OK) {
    Error_Handler();
  }

  if (ST7796_WriteData(y0 >> 8) != HAL_OK ||
      ST7796_WriteData(y0 & 0xFF) != HAL_OK ||
      ST7796_WriteData(y1 >> 8) != HAL_OK ||
      ST7796_WriteData(y1 & 0xFF) != HAL_OK) {
    Error_Handler();
  }
}

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
  ST7796_FillRect(0, 0, ST7796_WIDTH, ST7796_HEIGHT, color);
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
  if (x >= ST7796_WIDTH || y >= ST7796_HEIGHT) {
    return;
  }

  // Select a one-pixel address window
  ST7796_SetAddressWindow(x, y, x, y);

  // Begin memory write
  if (ST7796_WriteCommand(0x2C) != HAL_OK) {
    Error_Handler();
  }

  // Send RGB565 color
  if (ST7796_WriteData(color >> 8) != HAL_OK ||
      ST7796_WriteData(color & 0xFF) != HAL_OK) {
    Error_Handler();
  }
}

// Bresenham's line algorithm
void ST7796_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     uint16_t color) {
  int32_t dx = abs((int32_t)x1 - x0);
  int32_t dy = -abs((int32_t)y1 - y0);

  int32_t sx = (x0 < x1) ? 1 : -1;
  int32_t sy = (y0 < y1) ? 1 : -1;

  int32_t error = dx + dy;

  while (1) {
    if (x0 >= 0 && y0 >= 0) {
      ST7796_DrawPixel((uint16_t)x0, (uint16_t)y0, color);
    }

    if (x0 == x1 && y0 == y1) {
      break;
    }

    int32_t e2 = 2 * error;

    if (e2 >= dy) {
      error += dy;
      x0 += sx;
    }

    if (e2 <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

void ST7796_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     uint16_t color) {
  // Reject invalid starting coordinates or empty rectangles
  if (x >= ST7796_WIDTH || y >= ST7796_HEIGHT || width == 0 || height == 0) {
    return;
  }

  // Clip rectangle to display boundaries
  if (width > ST7796_WIDTH - x) {
    width = ST7796_WIDTH - x;
  }

  if (height > ST7796_HEIGHT - y) {
    height = ST7796_HEIGHT - y;
  }

  // Configure the rectangular address window
  ST7796_SetAddressWindow(x, y, x + width - 1, y + height - 1);

  // Begin writing pixel data
  if (ST7796_WriteCommand(0x2C) != HAL_OK) {
    Error_Handler();
  }

  uint8_t pixel_data[128];

  // Prepare 64 RGB565 pixels
  for (uint16_t i = 0; i < sizeof(pixel_data); i += 2) {
    pixel_data[i] = (uint8_t)(color >> 8);
    pixel_data[i + 1] = (uint8_t)(color & 0xFF);
  }

  uint32_t remaining_pixels = (uint32_t)width * height;

  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

  while (remaining_pixels > 0) {
    uint32_t pixels_to_send = (remaining_pixels > 64) ? 64 : remaining_pixels;

    if (HAL_SPI_Transmit(&hspi1, pixel_data, (uint16_t)(pixels_to_send * 2),
                         1000) != HAL_OK) {
      HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
      Error_Handler();
    }

    remaining_pixels -= pixels_to_send;
  }

  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

void ST7796_DrawChar(uint16_t x, uint16_t y, char character, uint16_t color,
                     uint8_t scale) {
  if (scale == 0) {
    return;
  }

  const uint8_t *bitmap = Font5x7_GetCharacter(character);

  for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
    uint8_t row_data = bitmap[row];

    for (uint8_t column = 0; column < FONT_WIDTH; column++) {
      if (row_data & (1U << (4 - column))) {
        ST7796_FillRect(x + column * scale, y + row * scale, scale, scale,
                        color);
      }
    }
  }
}

void ST7796_DrawString(uint16_t x, uint16_t y, const char *text, uint16_t color,
                       uint8_t scale) {
  if (text == NULL || scale == 0) {
    return;
  }

  uint32_t cursor_x = x;
  uint32_t advance = (FONT_WIDTH + FONT_SPACING) * scale;

  while (*text != '\0') {
    if (cursor_x + FONT_WIDTH * scale > ST7796_WIDTH) {
      break;
    }

    ST7796_DrawChar((uint16_t)cursor_x, y, *text, color, scale);

    cursor_x += advance;
    text++;
  }
}

void ST7796_DrawRGB565Buffer(uint16_t x, uint16_t y, uint16_t width,
                             uint16_t height, const uint16_t *pixels) {
  if (pixels == NULL || width == 0U || height == 0U) {
    return;
  }

  if (x >= ST7796_WIDTH || y >= ST7796_HEIGHT) {
    return;
  }

  if (width > ST7796_WIDTH - x || height > ST7796_HEIGHT - y) {
    return;
  }

  ST7796_SetAddressWindow(x, y, x + width - 1U, y + height - 1U);

  ST7796_WriteCommand(0x2C);

  static uint8_t tx_buffer[1024];
  uint32_t total_pixels = (uint32_t)width * height;
  uint32_t index = 0U;

  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

  while (index < total_pixels) {
    uint16_t chunk = 0U;

    while (chunk < (sizeof(tx_buffer) / 2U) && index < total_pixels) {
      uint16_t color = pixels[index++];

      tx_buffer[chunk * 2U] = (uint8_t)(color >> 8);
      tx_buffer[chunk * 2U + 1U] = (uint8_t)color;

      chunk++;
    }

    uint32_t start_cycles = DWT->CYCCNT;

    HAL_SPI_Transmit(&hspi1, tx_buffer, chunk * 2U, HAL_MAX_DELAY);

    SpiTransferCycles += DWT->CYCCNT - start_cycles;
  }

  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}
