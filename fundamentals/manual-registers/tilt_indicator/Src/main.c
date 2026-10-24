/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Pedro Valle-Cruz
 * @brief          : Register-level SPI tilt indicator with hysteresis and timeouts
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
#include <stdio.h>

typedef struct {
  volatile uint32_t CR;
  volatile uint32_t PLLCFGR;
  volatile uint32_t CFGR;
  volatile uint32_t CIR;
  volatile uint32_t AHB1RSTR;
  volatile uint32_t AHB2RSTR;
  volatile uint32_t AHB3RSTR;
  uint32_t RESERVED0;
  volatile uint32_t APB1RSTR;
  volatile uint32_t APB2RSTR;
  uint32_t RESERVED1[2];
  volatile uint32_t AHB1ENR;
  volatile uint32_t AHB2ENR;
  volatile uint32_t AHB3ENR;
  uint32_t RESERVED2;
  volatile uint32_t APB1ENR;
  volatile uint32_t APB2ENR;
} RCC_TypeDef;

typedef struct {
  volatile uint32_t MODER;
  volatile uint32_t OTYPER;
  volatile uint32_t OSPEEDR;
  volatile uint32_t PUPDR;
  volatile uint32_t IDR;
  volatile uint32_t ODR;
  volatile uint32_t BSRR;
  volatile uint32_t LCKR;
  volatile uint32_t AFR[2];
} GPIO_TypeDef;

typedef struct {
  volatile uint32_t CR1;
  volatile uint32_t CR2;
  volatile uint32_t SR;
  volatile uint32_t DR;
} SPI_TypeDef;

typedef struct {
  volatile uint32_t CSR;
  volatile uint32_t RVR;
  volatile uint32_t CVR;
} SYSTICK_TypeDef;

#define RCC_BASE                    0x40023800U
#define GPIOA_BASE                  0x40020000U
#define GPIOD_BASE                  0x40020C00U
#define GPIOE_BASE                  0x40021000U
#define SPI1_BASE                    0x40013000U
#define SYSTICK_BASE                0xE000E010U
#define LIS3DSH_WHO_AM_I_ADDR        0x0FU
#define LIS3DSH_WHO_AM_I_VALUE       0x3FU
#define LIS3DSH_CTRL_REG4_ADDR       0x20U
#define LIS3DSH_OUT_X_L_ADDR         0x28U
#define LIS3DSH_OUT_X_H_ADDR         0x29U
#define TILT_THRESHOLD               2000
#define TILT_RELEASE_THRESHOLD       1500
#define SAMPLE_INTERVAL_MS           100U
#define SPI_TIMEOUT_MS               10U

RCC_TypeDef * const RCC = (RCC_TypeDef *)RCC_BASE;
GPIO_TypeDef * const GPIOA = (GPIO_TypeDef *)GPIOA_BASE;
GPIO_TypeDef * const GPIOD = (GPIO_TypeDef *)GPIOD_BASE;
GPIO_TypeDef * const GPIOE = (GPIO_TypeDef *)GPIOE_BASE;
SPI_TypeDef * const SPI1 = (SPI_TypeDef *)SPI1_BASE;
SYSTICK_TypeDef * const SYSTICK = (SYSTICK_TypeDef *)SYSTICK_BASE;


typedef enum {
  TILT_LEVEL,
  TILT_LEFT,
  TILT_RIGHT
} tilt_state_t;

typedef enum {
  SPI_OK,
  SPI_TIMEOUT
} spi_status_t;

volatile uint32_t system_ticks = 0U;

void sensor_gpio_init(void);
void spi1_init(void);
spi_status_t spi1_transfer(uint8_t data, uint8_t *pReceived);
void sensor_select(void);
spi_status_t sensor_deselect(void);
spi_status_t sensor_read_register(uint8_t register_address, uint8_t *pValue);
spi_status_t sensor_write_register(uint8_t register_address, uint8_t value);
spi_status_t sensor_read_x(int16_t *pX);
void systick_init(void);
void SysTick_Handler(void);
void led_gpio_init(void);
void leds_show_left(void);
void leds_show_right(void);
void leds_show_level(void);

int main(void) {
  sensor_gpio_init();
  spi1_init();
  systick_init();
  led_gpio_init();

  /* Verify the sensor before enabling measurements. */
  uint8_t sensor_id;
  spi_status_t status = sensor_read_register(LIS3DSH_WHO_AM_I_ADDR, &sensor_id);
  if (status == SPI_TIMEOUT) {
      printf("Communication error\n");
      for (;;) {}
  }

  if (sensor_id != LIS3DSH_WHO_AM_I_VALUE) {
      printf("Unexpected ID\n");
      for (;;) {}
  }

  /* 50 Hz, block data update, and all three axes enabled. */
  status = sensor_write_register(LIS3DSH_CTRL_REG4_ADDR, 0x5FU);
  if (status == SPI_TIMEOUT) {
      printf("Configuration error\n");
      for (;;) {}
  }

  int16_t x_reading;
  uint32_t last_sample_tick = 0U;
  tilt_state_t tilt_state = TILT_LEVEL;

  for (;;) {
    if ((system_ticks - last_sample_tick) >= SAMPLE_INTERVAL_MS) {
      last_sample_tick = system_ticks;
      status = sensor_read_x(&x_reading);

      if (status == SPI_TIMEOUT) {
        leds_show_level();
        printf("Timeout error\n");
        for (;;) {}
      }

      /* Separate entry and release thresholds retain state near the boundary. */
      if (tilt_state == TILT_LEVEL) {
        if (x_reading < -TILT_THRESHOLD) {
          tilt_state = TILT_LEFT;
          leds_show_left();
        } else if (x_reading > TILT_THRESHOLD) {
          tilt_state = TILT_RIGHT;
          leds_show_right();
        } else {
          tilt_state = TILT_LEVEL;
          leds_show_level();
        }
      } else if (tilt_state == TILT_RIGHT) {
        if (x_reading < -TILT_THRESHOLD) {
          tilt_state = TILT_LEFT;
          leds_show_left();
        } else if (x_reading <= TILT_RELEASE_THRESHOLD) {
          tilt_state = TILT_LEVEL;
          leds_show_level();
          }
      } else if (tilt_state == TILT_LEFT) {
        if (x_reading > TILT_THRESHOLD) {
          tilt_state = TILT_RIGHT;
          leds_show_right();
        } else if (x_reading >= -TILT_RELEASE_THRESHOLD) {
          tilt_state = TILT_LEVEL;
          leds_show_level();
         }
      }
    }
  }
}

void sensor_gpio_init(void) {

  RCC->AHB1ENR |= (1U << 0) | (1U << 4);
  GPIOE->BSRR = (1U << 3);

  GPIOA->MODER &= ~((3U << 10) | (3U << 12) | (3U << 14));
  GPIOA->MODER |= (2U << 10) | (2U << 12) | (2U << 14);

  GPIOE->MODER &= ~(3U << 6);
  GPIOE->MODER |= (1U << 6);

  GPIOA->AFR[0] &= ~((0xFU << 20) | (0xFU << 24) | (0xFU << 28));
  GPIOA->AFR[0] |= (5U << 20) | (5U << 24) | (5U << 28);
}

void spi1_init(void) {

  RCC->APB2ENR |= (1U << 12);

  SPI1->CR1 &= ~(1U << 6);
  SPI1->CR1 |= (1U << 2);
  SPI1->CR1 &= ~(7U << 3);
  SPI1->CR1 |= (3U << 3);
  SPI1->CR1 |= (1U << 8) | (1U << 9);
  SPI1->CR1 |= (1U << 0) | (1U << 1);
  SPI1->CR1 &= ~((1U << 7) | (1U << 10) | (1U << 11) | (1U << 15));
  SPI1->CR1 |= (1U << 6);
}

spi_status_t spi1_transfer(uint8_t data, uint8_t *pReceived) {
  volatile uint8_t *pSPI1_DR = (volatile uint8_t *)&SPI1->DR;

  uint32_t wait_start_tick = system_ticks;

  while ((SPI1->SR & (1U << 1)) == 0) {
    if ((system_ticks - wait_start_tick) >= SPI_TIMEOUT_MS) {
      return SPI_TIMEOUT;
    }
  }
  *pSPI1_DR = data;

  /* Start a separate timeout budget for receiving the byte. */
  wait_start_tick = system_ticks;

  while ((SPI1->SR & (1U << 0)) == 0) {
    if ((system_ticks - wait_start_tick) >= SPI_TIMEOUT_MS) {
      return SPI_TIMEOUT;
    }
  }

  *pReceived = *pSPI1_DR;
  return SPI_OK;
}

void sensor_select(void) {
  GPIOE->BSRR = (1U << 19);
}

spi_status_t sensor_deselect(void) {
  uint32_t wait_start_tick = system_ticks;

  while ((SPI1->SR & (1U << 7)) != 0) {
    if ((system_ticks - wait_start_tick) >= SPI_TIMEOUT_MS) {
      GPIOE->BSRR = (1U << 3);
      return SPI_TIMEOUT;
    }
  }

  GPIOE->BSRR = (1U << 3);
  return SPI_OK;
}

spi_status_t sensor_read_register(uint8_t register_address, uint8_t *pValue) {
  uint8_t discarded_byte;

  sensor_select();
  spi_status_t status = spi1_transfer(register_address | (1U << 7), &discarded_byte);

  if (status == SPI_TIMEOUT) {
    sensor_deselect();
    return status;
  }

  status = spi1_transfer(0x00U, pValue);

  if (status == SPI_TIMEOUT) {
    sensor_deselect();
    return status;
  }

  return sensor_deselect();
}

spi_status_t sensor_write_register(uint8_t register_address, uint8_t value) {
  uint8_t discarded_byte;

  sensor_select();
  spi_status_t status = spi1_transfer(register_address & ~(1U << 7), &discarded_byte);

  if (status == SPI_TIMEOUT) {
    sensor_deselect();
    return status;
  }

  status = spi1_transfer(value, &discarded_byte);

  if (status == SPI_TIMEOUT) {
    sensor_deselect();
    return status;
  }

  return sensor_deselect();
}

spi_status_t sensor_read_x(int16_t *pX) {
  uint8_t low;
  uint8_t high;

  spi_status_t status = sensor_read_register(LIS3DSH_OUT_X_L_ADDR, &low);

  if (status == SPI_TIMEOUT) {
    return status;
  }

  status = sensor_read_register(LIS3DSH_OUT_X_H_ADDR, &high);

  if (status == SPI_TIMEOUT) {
    return status;
  }

  *pX = (int16_t)(((uint16_t)high << 8) | low);
  return SPI_OK;
}

void systick_init(void) {
  SYSTICK->CSR &= ~(1U << 0);
  SYSTICK->RVR = 15999U;
  SYSTICK->CVR = 0U;

  SYSTICK->CSR |= (1U << 2) | (1U << 1) | (1U << 0);
}

void SysTick_Handler(void) {
  system_ticks += 1U;
}

void led_gpio_init(void) {
  RCC->AHB1ENR |= (1U << 3);
  GPIOD->BSRR = (1U << 28) | (1U << 30);
  GPIOD->MODER &= ~(3U << 24);
  GPIOD->MODER |= (1U << 24);
  GPIOD->MODER &= ~(3U << 28);
  GPIOD->MODER |= (1U << 28);
}

void leds_show_left(void) {
  GPIOD->BSRR = (1U << 12) | (1U << 30);
}

void leds_show_right(void) {
  GPIOD->BSRR = (1U << 28) | (1U << 14);
}

void leds_show_level(void) {
  GPIOD->BSRR = (1U << 28) | (1U << 30);
}
