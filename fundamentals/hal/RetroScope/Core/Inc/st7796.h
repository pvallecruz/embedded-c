/*
 * st7796.h
 *
 *  Created on: Nov 14, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_ST7796_H_
#define INC_ST7796_H_

#include "stm32f4xx_hal.h"

#define ST7796_WIDTH   480U
#define ST7796_HEIGHT  320U

#define ST7796_RED     0xF800U
#define ST7796_GREEN   0x07E0U
#define ST7796_BLUE    0x001FU
#define ST7796_BLACK  0x0000U
#define ST7796_WHITE  0xFFFFU

void ST7796_Init(void);
void ST7796_FillScreen(uint16_t color);
void ST7796_DrawPixel(uint16_t x, uint16_t y, uint16_t color);




#endif /* INC_ST7796_H_ */
