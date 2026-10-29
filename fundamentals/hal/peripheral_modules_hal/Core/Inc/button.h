/**
 * @file   button.h
 * @author Pedro Valle-Cruz
 * @brief  Read and debounce the active-low PE10 button.
 * @note   Call button_init() before reading or updating the button.
 *         PE10 uses an internal pull-up; pressed connects the pin to GND.
 *         Call button_update(now) regularly from one polling loop, using a
 *         wrapping 32-bit millisecond timebase. This module does not start it.
 *         A button held at initialization does not generate a press event.
 */

#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_
#include <stdint.h>

void button_init(void);
int button_is_pressed(void);
int button_update(uint32_t now);

#endif /* INC_BUTTON_H_ */
