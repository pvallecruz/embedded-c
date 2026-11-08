
/*
 * lcd.h
 *
 *  Created on: Nov 8, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_LCD_H_
#define INC_LCD_H_

#include "stm32f4xx_hal.h"

#define LCD_I2C_ADDRESS (0x27U << 1)
#define LCD_RS          (1U << 0)
#define LCD_RW          (1U << 1)
#define LCD_EN          (1U << 2)
#define LCD_BACKLIGHT   (1U << 3)

HAL_StatusTypeDef LcdInit(void);
HAL_StatusTypeDef LcdWriteByte(uint8_t data);
HAL_StatusTypeDef LcdPrint(const char *text);
HAL_StatusTypeDef LcdSetCursor(uint8_t row, uint8_t column);
HAL_StatusTypeDef LcdClear(void);

#endif /* INC_LCD_H_ */
