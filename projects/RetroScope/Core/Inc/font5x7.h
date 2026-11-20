/*
 * font5x7.h
 *
 *  Created on: Nov 15, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_FONT5X7_H_
#define INC_FONT5X7_H_

#include <stdint.h>

#define FONT_WIDTH   5U
#define FONT_HEIGHT  7U
#define FONT_SPACING 1U

const uint8_t* Font5x7_GetCharacter(char character);

#endif /* INC_FONT5X7_H_ */
