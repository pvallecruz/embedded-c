/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Pedro Valle-Cruz
 * @brief          : Register-level ADC/PWM LED dimmer with I2C LCD percentage display
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include <stdint.h>
#include "stm32f4xx.h"

/* Timing assumes the reset clock tree: HSI, AHB, APB1 and APB2 at 16 MHz. */
#define ADC_MAX_READING 4095U
#define PWM_PERIOD_COUNTS 1000U
#define I2C_TIMEOUT_MS 10U
#define LCD_ADDRESS 0x27U
#define LCD_BACKLIGHT 0x08U
#define LCD_ENABLE 0x04U
#define LCD_ROWS 4U
#define LCD_COLUMNS 20U
#define LCD_REFRESH_MS 100U

/* INVALID_ARGUMENT is a caller error, not an acknowledgment from hardware. */
typedef enum {
  I2C_OK,
  I2C_TIMEOUT,
  I2C_NACK,
  I2C_INVALID_ARGUMENT
} i2c_status_t;

void adc1_init(void);
void adc_startup_delay(void);
uint16_t adc1_read(void);
void led_pwm_init(void);
void i2c1_init(void);
void systick_init(void);
i2c_status_t i2c1_probe(uint8_t address);
i2c_status_t i2c1_write_byte(uint8_t address, uint8_t data);
i2c_status_t lcd_write_nibble(uint8_t nibble, uint8_t rs);
void delay_ms(uint32_t duration_ms);
i2c_status_t lcd_init(void);
i2c_status_t lcd_write_byte(uint8_t value, uint8_t rs);
i2c_status_t lcd_write_string(const char *text);
i2c_status_t lcd_set_cursor(uint8_t row, uint8_t column);
static void percentage_to_string(uint32_t val, char *buffer);
static i2c_status_t lcd_show_brightness(uint32_t percentage);

/* Volatile keeps the ADC observation visible in the debugger. */
volatile uint16_t potentiometer_reading = 0U;
/* Written by SysTick_Handler and read by foreground delays/timeouts. */
volatile uint32_t system_ticks = 0U;

volatile i2c_status_t lcd_write_status = I2C_TIMEOUT;
volatile i2c_status_t lcd_init_status = I2C_TIMEOUT;

int main(void) {
  /* The short ADC startup wait owns SysTick before the 1 ms timebase starts. */
  adc1_init();
  systick_init();
  led_pwm_init();
  i2c1_init();

  /* 101 is outside the valid percentage range, forcing the first row update. */
  uint32_t last_brightness_pct = 101U;
  lcd_init_status = lcd_init();
  if (lcd_init_status == I2C_OK) {
    lcd_write_status = lcd_write_string("LED DIMMER");
  }
  uint32_t last_display_tick = system_ticks - LCD_REFRESH_MS;

  for (;;) {
    potentiometer_reading = adc1_read();
    /* CCR1 == ARR + 1 gives fully on; CCR1 == 0 gives fully off. */
    TIM4->CCR1 = (potentiometer_reading * PWM_PERIOD_COUNTS) / ADC_MAX_READING;
    /* Round the displayed command to the nearest percent; this is not a
       measurement of optical brightness, which is not perceptually linear. */
    uint32_t brightness_pct =
        (potentiometer_reading * 100U + ADC_MAX_READING / 2U) / ADC_MAX_READING;

    if (lcd_init_status == I2C_OK && lcd_write_status == I2C_OK &&
        (system_ticks - last_display_tick) >= LCD_REFRESH_MS) {
      last_display_tick = system_ticks;
      if (brightness_pct != last_brightness_pct) {
        lcd_write_status = lcd_show_brightness(brightness_pct);
        /* Only cache a value after the whole row was written successfully.
           A failure latches the status and stops LCD writes until reset;
           potentiometer sampling and hardware PWM continue. */
        if (lcd_write_status == I2C_OK) {
          last_brightness_pct = brightness_pct;
        }
      }
    }

    /* A short polling pause; LCD transfers add time, so this is not a fixed
       5 ms sampling period. PWM runs independently in TIM4. */
    delay_ms(5U);
  }
}

/* PA1 is ADC1 channel 1. Long sampling accommodates the B50K potentiometer. */
void adc1_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  GPIOA->MODER &= ~(GPIO_MODER_MODER1);
  GPIOA->MODER |= GPIO_MODER_MODER1;
  GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD1);

  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  ADC1->CR1 &= ~(ADC_CR1_RES);
  ADC1->SQR1 &= ~(ADC_SQR1_L);

  ADC1->SQR3 &= ~(ADC_SQR3_SQ1);
  ADC1->SQR3 |= ADC_SQR3_SQ1_0;

  /* SMP1=111 selects 480 ADC cycles; resolution remains 12 bits. */
  ADC1->SMPR2 &= ~(ADC_SMPR2_SMP1);
  ADC1->SMPR2 |= ADC_SMPR2_SMP1;

  ADC1->CR2 &= ~(ADC_CR2_CONT | ADC_CR2_ALIGN);

  ADC->CCR &= ~(ADC_CCR_ADCPRE);

  ADC1->CR2 |= ADC_CR2_ADON;

  adc_startup_delay();

}

/* One-shot nominal 3 us wait at 16 MHz; call only before systick_init(). */
void adc_startup_delay(void) {
  SysTick->CTRL = 0U;
  SysTick->LOAD = 47U;
  SysTick->VAL = 0U;
  SysTick->CTRL = (SysTick_CTRL_CLKSOURCE_Msk) | (SysTick_CTRL_ENABLE_Msk);
  while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U) {
  }
  SysTick->CTRL = 0U;
}

/* Blocking single conversion. Reading DR consumes the result and clears EOC.
   Unlike I2C waits, this ADC wait currently has no timeout. */
uint16_t adc1_read(void) {
  ADC1->CR2 |= (ADC_CR2_SWSTART);
  while ((ADC1->SR & ADC_SR_EOC) == 0U) {
  }
  return ADC1->DR;
}

/* TIM4 CH1 drives PD12/AF2: 16 MHz / 16 / 1000 = nominal 1 kHz PWM. */
void led_pwm_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
  TIM4->CR1 &= ~(TIM_CR1_CEN);
  TIM4->CR1 &= ~(TIM_CR1_DIR | TIM_CR1_CMS);
  TIM4->PSC = 15U;
  TIM4->ARR = PWM_PERIOD_COUNTS - 1U;
  TIM4->CCMR1 &= ~(TIM_CCMR1_CC1S);
  TIM4->CCMR1 &= ~(TIM_CCMR1_OC1M);
  TIM4->CCMR1 |= (TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1);
  TIM4->CCMR1 |= TIM_CCMR1_OC1PE;
  TIM4->CCER &= ~(TIM_CCER_CC1P);
  TIM4->CCER |= TIM_CCER_CC1E;
  TIM4->CCR1 = 0U;
  GPIOD->MODER &= ~(GPIO_MODER_MODER12);
  GPIOD->MODER |= GPIO_MODER_MODER12_1;
  GPIOD->AFR[1] &= ~GPIO_AFRH_AFSEL12;
  GPIOD->AFR[1] |= GPIO_AFRH_AFSEL12_1;
  GPIOD->OTYPER &= ~(GPIO_OTYPER_OT12);
  GPIOD->PUPDR &= ~(GPIO_PUPDR_PUPD12);
  /* Load the buffered prescaler/compare value before starting the counter. */
  TIM4->EGR = TIM_EGR_UG;
  TIM4->CR1 |= TIM_CR1_CEN;
}

/* PB6=SCL and PB7=SDA use AF4/open-drain; backpack pull-ups share 3V. */
void i2c1_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
  GPIOB->MODER &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
  GPIOB->MODER |= (GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1);
  GPIOB->AFR[0] &= ~(GPIO_AFRL_AFSEL6 | GPIO_AFRL_AFSEL7);
  GPIOB->AFR[0] |= (GPIO_AFRL_AFSEL6_2 | GPIO_AFRL_AFSEL7_2);
  GPIOB->OTYPER |= (GPIO_OTYPER_OT6 | GPIO_OTYPER_OT7);
  /* Reset after GPIO setup resolved the observed startup BUSY blockage.
     This is initialization, not recovery from a physically stuck bus. */
  RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
  RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
  I2C1->CR1 &= ~(I2C_CR1_PE);
  I2C1->CR2 &= ~(I2C_CR2_FREQ);
  /* APB1=16 MHz, standard mode: CCR=16 MHz/(2*100 kHz), TRISE=16+1. */
  I2C1->CR2 |= 16U;
  I2C1->CCR &= ~(I2C_CCR_FS);
  I2C1->CCR &= ~(I2C_CCR_CCR);
  I2C1->CCR |= 80U;
  I2C1->TRISE = 17U;
  I2C1->CR1 |= I2C_CR1_PE;
}

/* Start the shared nominal 1 ms timebase after ADC startup is complete. */
void systick_init(void) {
  SysTick->CTRL = 0U;
  SysTick->LOAD = 15999U;
  SysTick->VAL = 0U;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                  SysTick_CTRL_TICKINT_Msk |
                  SysTick_CTRL_ENABLE_Msk;
}

void SysTick_Handler(void) {
  system_ticks++;
}

/* Optional diagnostic: address-only write transaction, no LCD data sent.
   Each waiting phase and STOP cleanup gets a separate timeout budget. */
i2c_status_t i2c1_probe(uint8_t address) {
  if (address > 0x7FU) {
    return I2C_INVALID_ARGUMENT;
  }
  uint32_t start_tick = system_ticks;
  while ((I2C1->SR2 & I2C_SR2_BUSY) != 0U) {
    if ((system_ticks - start_tick) >= I2C_TIMEOUT_MS) {
      return I2C_TIMEOUT;
    }
  }

  I2C1->CR1 |= I2C_CR1_START;

  start_tick = system_ticks;

  while ((I2C1->SR1 & I2C_SR1_SB) == 0U) {
    if ((system_ticks - start_tick) >= I2C_TIMEOUT_MS) {
      I2C1->CR1 &= ~I2C_CR1_START;
      if ((I2C1->SR2 & I2C_SR2_MSL) != 0U) {
        I2C1->CR1 |= I2C_CR1_STOP;

        uint32_t stop_start_tick = system_ticks;

        while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
          if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
            return I2C_TIMEOUT;
          }
        }
      }
      return I2C_TIMEOUT;
    }
  }

  I2C1->DR = address << 1;

  start_tick = system_ticks;

  while ((I2C1->SR1 & I2C_SR1_ADDR) == 0U) {
    if ((I2C1->SR1 & I2C_SR1_AF) != 0U) {
      I2C1->CR1 |= I2C_CR1_STOP;
      I2C1->SR1 &= ~I2C_SR1_AF;

      uint32_t stop_start_tick = system_ticks;

      while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
        if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
          return I2C_TIMEOUT;
        }
      }
      return I2C_NACK;
    }

    if ((system_ticks - start_tick) >= I2C_TIMEOUT_MS) {
      uint32_t stop_start_tick = system_ticks;
      I2C1->CR1 |= I2C_CR1_STOP;
      while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
        if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
          return I2C_TIMEOUT;
        }
      }
      return I2C_TIMEOUT;
    }
  }

  /* Read SR1 then SR2 to release the acknowledged-address phase. */
  (void)I2C1->SR1;
  (void)I2C1->SR2;

  I2C1->CR1 |= I2C_CR1_STOP;
  uint32_t stop_start_tick = system_ticks;
  while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
    if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
      return I2C_TIMEOUT;
    }
  }
  return I2C_OK;

}

/* Polling master write. Timeout/NACK paths request STOP; no automatic
   bus recovery or separate arbitration/bus-error classification is provided. */
i2c_status_t i2c1_write_byte(uint8_t address, uint8_t data) {
  if (address > 0x7FU) {
    return I2C_INVALID_ARGUMENT;
  }
  uint32_t start_tick = system_ticks;
  while ((I2C1->SR2 & I2C_SR2_BUSY) != 0U) {
    if ((system_ticks - start_tick) >= I2C_TIMEOUT_MS) {
      return I2C_TIMEOUT;
    }
  }

  I2C1->CR1 |= I2C_CR1_START;

  start_tick = system_ticks;

  while ((I2C1->SR1 & I2C_SR1_SB) == 0U) {
    if ((system_ticks - start_tick) >= I2C_TIMEOUT_MS) {
      I2C1->CR1 &= ~I2C_CR1_START;
      if ((I2C1->SR2 & I2C_SR2_MSL) != 0U) {
        I2C1->CR1 |= I2C_CR1_STOP;

        uint32_t stop_start_tick = system_ticks;

        while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
          if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
            return I2C_TIMEOUT;
          }
        }
      }
      return I2C_TIMEOUT;
    }
  }

  I2C1->DR = address << 1;

  start_tick = system_ticks;

  while ((I2C1->SR1 & I2C_SR1_ADDR) == 0U) {
    if ((I2C1->SR1 & I2C_SR1_AF) != 0U) {
      I2C1->CR1 |= I2C_CR1_STOP;
      I2C1->SR1 &= ~I2C_SR1_AF;

      uint32_t stop_start_tick = system_ticks;

      while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
        if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
          return I2C_TIMEOUT;
        }
      }
      return I2C_NACK;
    }

    if ((system_ticks - start_tick) >= I2C_TIMEOUT_MS) {
      uint32_t stop_start_tick = system_ticks;
      I2C1->CR1 |= I2C_CR1_STOP;
      while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
        if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
          return I2C_TIMEOUT;
        }
      }
      return I2C_TIMEOUT;
    }
  }

  /* Ordered reads clear ADDR; debugger register reads can also affect it. */
  (void)I2C1->SR1;
  (void)I2C1->SR2;

  start_tick = system_ticks;

  while ((I2C1->SR1 & I2C_SR1_TXE) == 0U) {
    if (system_ticks - start_tick >= I2C_TIMEOUT_MS) {
      uint32_t stop_start_tick = system_ticks;
      I2C1->CR1 |= I2C_CR1_STOP;
      while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
        if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
          return I2C_TIMEOUT;
        }
      }
      return I2C_TIMEOUT;
    }
  }

  /* TXE only means DR is empty; BTF below waits for actual byte completion. */
  I2C1->DR = data;
  start_tick = system_ticks;

  while ((I2C1->SR1 & I2C_SR1_BTF) == 0U) {
    if ((I2C1->SR1 & I2C_SR1_AF) != 0U) {
      I2C1->CR1 |= I2C_CR1_STOP;
      I2C1->SR1 &= ~I2C_SR1_AF;

      uint32_t stop_start_tick = system_ticks;

      while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
        if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
          return I2C_TIMEOUT;
        }
      }
      return I2C_NACK;
    }
    if ((system_ticks - start_tick) >= I2C_TIMEOUT_MS) {
      I2C1->CR1 |= I2C_CR1_STOP;
      uint32_t stop_start_tick = system_ticks;

      while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
        if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
          return I2C_TIMEOUT;
        }
      }
      return I2C_TIMEOUT;
    }
  }

  I2C1->CR1 |= I2C_CR1_STOP;
  uint32_t stop_start_tick = system_ticks;
  while ((I2C1->CR1 & I2C_CR1_STOP) != 0U) {
    if ((system_ticks - stop_start_tick) >= I2C_TIMEOUT_MS) {
      return I2C_TIMEOUT;
    }
  }
  return I2C_OK;
}

/* PCF8574: P7:4=data, P3=backlight, P2=E, P1=R/W, P0=RS.
   Three complete 100 kHz I2C writes keep data stable across E low-high-low.
   Revisit LCD pulse/command timing if bus speed or transfer batching changes. */
i2c_status_t lcd_write_nibble(uint8_t nibble, uint8_t rs) {
  uint8_t output = ((nibble & 0x0FU) << 4) | LCD_BACKLIGHT | (rs & 0x01U);
  i2c_status_t status = i2c1_write_byte(LCD_ADDRESS, output);
  if (status != I2C_OK) {
    return status;
  }
  status = i2c1_write_byte(LCD_ADDRESS, (output | LCD_ENABLE));
  if (status != I2C_OK) {
    return status;
  }
  return i2c1_write_byte(LCD_ADDRESS, output);
}

/* Unsigned subtraction handles tick wrap for these short waits. Interrupts
   must remain enabled; a tick-based delay has up to one tick of quantization. */
void delay_ms(uint32_t duration_ms) {
  uint32_t start_tick = system_ticks;
  while ((system_ticks - start_tick) < duration_ms) {

  }
}

/* Startup nibbles establish 4-bit mode before sending full command bytes.
   Delays include margin for tick quantization, especially after clear. */
i2c_status_t lcd_init(void) {
  delay_ms(50U);
  i2c_status_t status = lcd_write_nibble(0x03U, 0U);
  if (status != I2C_OK) {
    return status;
  }

  delay_ms(6U);
  status = lcd_write_nibble(0x03U, 0U);
  if (status != I2C_OK) {
    return status;
  }

  delay_ms(6U);
  status = lcd_write_nibble(0x03U, 0U);
  if (status != I2C_OK) {
    return status;
  }

  delay_ms(2U);
  status = lcd_write_nibble(0x02U, 0U);
  if (status != I2C_OK) {
    return status;
  }

  delay_ms(2U);
  status = lcd_write_byte(0x28U, 0U);
  if (status != I2C_OK) {
    return status;
  }

  delay_ms(2U);
  status = lcd_write_byte(0x08U, 0U);
  if (status != I2C_OK) {
    return status;
  }

  delay_ms(2U);
  status = lcd_write_byte(0x01U, 0U);
  if (status != I2C_OK) {
    return status;
  }

  delay_ms(3U);
  status = lcd_write_byte(0x06U, 0U);
  if (status != I2C_OK) {
    return status;
  }

  delay_ms(2U);
  status = lcd_write_byte(0x0CU, 0U);
  if (status != I2C_OK) {
    return status;
  }

  delay_ms(2U);
  return I2C_OK;
}

/* LCD 4-bit protocol sends the upper nibble first; RS selects command/data. */
i2c_status_t lcd_write_byte(uint8_t value, uint8_t rs) {
  i2c_status_t status = lcd_write_nibble(value >> 4U, rs);
  if (status != I2C_OK) {
    return status;
  }

  return lcd_write_nibble(value & 0x0FU, rs);
}

/* Caller supplies a null-terminated string; this helper does not wrap rows. */
i2c_status_t lcd_write_string(const char *text) {
  if (text == 0U) {
    return I2C_INVALID_ARGUMENT;
  }
  while (*text != '\0') {
    i2c_status_t status = lcd_write_byte(*text, 1U);
    if (status != I2C_OK) {
      return status;
    }
    text++;
  }
  return I2C_OK;
}

/* A 20x4 LCD uses non-linear DDRAM row starts: 00, 40, 14, 54. */
i2c_status_t lcd_set_cursor(uint8_t row, uint8_t column) {
  if (row >= LCD_ROWS || column >= LCD_COLUMNS) {
    return I2C_INVALID_ARGUMENT;
  }
  uint8_t row_address = 0x00U;

  switch (row) {
    case 0U: row_address = 0x00U; break;
    case 1U: row_address = 0x40U; break;
    case 2U: row_address = 0x14U; break;
    case 3U: row_address = 0x54U; break;
    default:
      return I2C_INVALID_ARGUMENT;
  }

  uint8_t command = 0x80U | (row_address + column);

  return lcd_write_byte(command, 0U);
}

/* Private formatter for 0..100 only; caller supplies at least four bytes. */
static void percentage_to_string(uint32_t val, char *buffer) {

  if (val == 100U) {
    buffer[0] = '1';
    buffer[1] = '0';
    buffer[2] = '0';
    buffer[3] = '\0';
  } else if (val >= 10U) {
    buffer[0] = '0' + (val / 10U);
    buffer[1] = '0' + (val % 10U);
    buffer[2] = '\0';
  } else {
    buffer[0] = '0' + val;
    buffer[1] = '\0';
  }
}

/* Rewrite a bounded field without clearing the display, which would flicker. */
static i2c_status_t lcd_show_brightness(uint32_t percentage) {
  if (percentage > 100U) {
    return I2C_INVALID_ARGUMENT;
  }
  char percentage_str[4];  /* Three digits plus the terminating null byte. */
  percentage_to_string(percentage, percentage_str);

  i2c_status_t status = lcd_set_cursor(1U, 0U);
  if (status != I2C_OK) {
    return status;
  }
  status = lcd_write_string("Brightness: ");
  if (status != I2C_OK) {
    return status;
  }
  status = lcd_write_string(percentage_str);
  if (status != I2C_OK) {
    return status;
  }
  /* Spaces erase trailing digits when, for example, 100% becomes 9%. */
  return lcd_write_string("%   ");
}
