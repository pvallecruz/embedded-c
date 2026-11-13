#ifndef INC_LCD_H_
#define INC_LCD_H_

#include "stm32f4xx_hal.h"

#define LCD_I2C_ADDRESS (0x27U << 1)
#define LCD_RS          (1U << 0)
#define LCD_RW          (1U << 1)
#define LCD_EN          (1U << 2)
#define LCD_BACKLIGHT   (1U << 3)

/* Start initialization. HAL_BUSY means initialization is in progress. */
HAL_StatusTypeDef LcdInit(void);
/* Call from the main loop. HAL_OK means initialized, not necessarily flushed.
 * Initialization returns HAL_BUSY; faults return HAL_ERROR/HAL_TIMEOUT.
 * Faults are retried once per second without waiting in this function.
 */
HAL_StatusTypeDef LcdUpdate(void);

/* Main-loop-only screen-buffer operations; HAL_OK means text was accepted.
 * Cursor coordinates are zero-based. Text must fit in the current row.
 * No I2C transfer or delay occurs in these functions.
 */
HAL_StatusTypeDef LcdPrint(const char *text);
HAL_StatusTypeDef LcdSetCursor(uint8_t row, uint8_t column);
HAL_StatusTypeDef LcdClear(void);

#endif /* INC_LCD_H_ */
