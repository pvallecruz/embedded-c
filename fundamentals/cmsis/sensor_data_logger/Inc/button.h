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

#ifndef BUTTON_H_
#define BUTTON_H_

#include <stdint.h>

/* Return 1 only for a newly accepted press after 20 ms of stability; else 0. */
int button_update(uint32_t now);

/* Configure PE10 and seed debounce from its current state. */
void button_init(void);

/* Raw state only: 1 = pressed, 0 = released. No debounce is performed. */
int button_is_pressed(void);

#endif /* BUTTON_H_ */
