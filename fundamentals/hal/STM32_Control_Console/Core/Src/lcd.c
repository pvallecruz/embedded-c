/*
 * lcd.c
 *
 *  Created on: Nov 8, 2026
 *      Author: pedrol.valle-cruz
 */

#include "lcd.h"
#include "i2c.h"

HAL_StatusTypeDef LcdWriteByte(uint8_t data) {
  return HAL_I2C_Master_Transmit(
      &hi2c1,
      LCD_I2C_ADDRESS,
      &data,
      1,
      100);
}

static HAL_StatusTypeDef LcdPulseEnable(uint8_t data) {
	HAL_StatusTypeDef status;

	// Enable HIGH
	status = LcdWriteByte(data | LCD_EN);

	if (status != HAL_OK) {
		return status;
	}

	// Enable LOW: LCD latches data on falling edge
	return LcdWriteByte(data & (uint8_t) ~LCD_EN);

}


static HAL_StatusTypeDef LcdSendNibble(uint8_t nibble,
                                       uint8_t control) {
  // Place the four data bits on PCF8574 pins P4-P7.
  uint8_t data = ((nibble & 0x0FU) << 4) |
                 control |
                 LCD_BACKLIGHT;

  // Set data and control signals before pulsing Enable.
  HAL_StatusTypeDef status = LcdWriteByte(data);

  if (status != HAL_OK) {
    return status;
  }

  // Pulse Enable to latch the nibble into the LCD.
  return LcdPulseEnable(data);
}


static HAL_StatusTypeDef LcdSendByte(uint8_t value,
                                    uint8_t control) {
  // Extract the upper four bits.
  uint8_t upper_nibble = (value >> 4) & 0x0FU;

  // Extract the lower four bits.
  uint8_t lower_nibble = value & 0x0FU;

  // Transmit the upper nibble first.
  HAL_StatusTypeDef status = LcdSendNibble(upper_nibble, control);

  if (status != HAL_OK) {
    return status;
  }

  // Transmit the lower nibble.
  return LcdSendNibble(lower_nibble, control);
}

static HAL_StatusTypeDef LcdSendCommand(uint8_t command) {
  HAL_StatusTypeDef status = LcdSendByte(command, 0U);

  if (status != HAL_OK) {
    return status;
  }

  // Clear and Home commands require longer execution time.
  if (command == 0x01U || command == 0x02U) {
    HAL_Delay(2);
  } else {
    HAL_Delay(1);
  }

  return HAL_OK;
}

static HAL_StatusTypeDef LcdSendData(uint8_t data) {
  HAL_StatusTypeDef status = LcdSendByte(data, LCD_RS);

  if (status == HAL_OK) {
    HAL_Delay(1);
  }

  return status;
}


HAL_StatusTypeDef LcdPrint(const char *text) {
  if (text == NULL) {
    return HAL_ERROR;
  }

  while (*text != '\0') {
    HAL_StatusTypeDef status = LcdSendData((uint8_t)*text);

    if (status != HAL_OK) {
      return status;
    }

    text++;
  }

  return HAL_OK;
}


HAL_StatusTypeDef LcdSetCursor(uint8_t row, uint8_t column) {
  // DDRAM starting addresses for each visible LCD row.
  static const uint8_t row_offsets[4] = {
    0x00U, 0x40U, 0x14U, 0x54U
  };

  // Validate coordinates for a 20x4 display.
  if (row >= 4U || column >= 20U) {
    return HAL_ERROR;
  }

  // Calculate the target DDRAM address.
  uint8_t address = row_offsets[row] + column;

  // Set bit 7 to issue the Set DDRAM Address command.
  return LcdSendCommand(0x80U | address);
}

HAL_StatusTypeDef LcdClear(void) {
  return LcdSendCommand(0x01U);
}


HAL_StatusTypeDef LcdInit(void) {
  HAL_StatusTypeDef status;

  // Wait for the LCD power supply to stabilize.
  HAL_Delay(50);

  // Force the LCD into a known initialization state.
  for (uint8_t i = 0; i < 3; i++) {
    status = LcdSendNibble(0x03U, 0U);

    if (status != HAL_OK) {
      return status;
    }

    HAL_Delay(5);
  }

  // Switch the LCD to 4-bit communication mode.
  status = LcdSendNibble(0x02U, 0U);

  if (status != HAL_OK) {
    return status;
  }

  HAL_Delay(1);

  // Configure 4-bit mode, 2-line interface, 5x8 font.
  // This also applies to common 20x4 LCD controllers.
  status = LcdSendCommand(0x28U);

  if (status != HAL_OK) {
    return status;
  }

  // Turn the display off during configuration.
  status = LcdSendCommand(0x08U);

  if (status != HAL_OK) {
    return status;
  }

  // Clear the display.
  status = LcdSendCommand(0x01U);

  if (status != HAL_OK) {
    return status;
  }

  // Set entry mode: increment cursor, no display shift.
  status = LcdSendCommand(0x06U);

  if (status != HAL_OK) {
    return status;
  }

  // Display on, cursor off, blinking off.
  return LcdSendCommand(0x0CU);
}






