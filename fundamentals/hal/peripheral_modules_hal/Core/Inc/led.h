/**
 * @file   led.h
 * @author Pedro Valle-Cruz
 * @brief  Initialize and control active-high LEDs on GPIOE.
 * @note   Pass pin numbers 7, 8, 9, or 11, not bit masks.
 *         Call led_init(pin) before using that LED; it starts off.
 *         Unsupported pins are ignored, with no return value.
 *         The caller must give this module exclusive control of its LED outputs.
 */

#ifndef INC_LED_H_
#define INC_LED_H_

void led_init(unsigned int pin);
void led_on(unsigned int pin);
void led_off(unsigned int pin);
void led_toggle(unsigned int pin);

#endif /* INC_LED_H_ */
