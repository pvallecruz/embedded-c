/*
 * temperature.c
 *
 *  Created on: Nov 8, 2026
 *      Author: pedrol.valle-cruz
 */

#include "temperature.h"

#include "adc.h"
#include "stm32f4xx_ll_adc.h"

static HAL_StatusTypeDef TemperatureReadChannel(uint32_t channel,
                                                uint32_t* raw) {
  if (raw == NULL) {
    return HAL_ERROR;
  }

  ADC_ChannelConfTypeDef config = {0};
  config.Channel = channel;
  config.Rank = 1;
  config.SamplingTime = ADC_SAMPLETIME_480CYCLES;

  HAL_StatusTypeDef status = HAL_ADC_ConfigChannel(&hadc1, &config);
  if (status != HAL_OK) {
    return status;
  }

  status = HAL_ADC_Start(&hadc1);
  if (status != HAL_OK) {
    return status;
  }

  uint32_t sample = 0U;

  status = HAL_ADC_PollForConversion(&hadc1, 10U);
  if (status == HAL_OK) {
    sample = HAL_ADC_GetValue(&hadc1);
  }

  HAL_StatusTypeDef stop_status = HAL_ADC_Stop(&hadc1);

  if (status != HAL_OK) {
    return status;
  }

  if (stop_status != HAL_OK) {
    return stop_status;
  }

  *raw = sample;
  return HAL_OK;
}

HAL_StatusTypeDef TemperatureReadCelsius(float* temperature_c) {
  if (temperature_c == NULL) {
    return HAL_ERROR;
  }

  uint32_t cal1 = *TEMPSENSOR_CAL1_ADDR;
  uint32_t cal2 = *TEMPSENSOR_CAL2_ADDR;
  uint32_t vref_cal = *VREFINT_CAL_ADDR;

  if (cal1 == 0U || cal1 > 4095U || cal2 == 0U || cal2 > 4095U ||
      vref_cal == 0U || vref_cal > 4095U || cal2 <= cal1) {
    return HAL_ERROR;
  }

  uint32_t temperature_raw;
  uint32_t vref_raw;

  HAL_StatusTypeDef status =
      TemperatureReadChannel(ADC_CHANNEL_TEMPSENSOR, &temperature_raw);
  if (status != HAL_OK) {
    return status;
  }

  status = TemperatureReadChannel(ADC_CHANNEL_VREFINT, &vref_raw);
  if (status != HAL_OK) {
    return status;
  }

  if (vref_raw == 0U) {
    return HAL_ERROR;
  }

  /* Normalize the temperature reading to the factory reference voltage. */
  float corrected_raw =
      (float)temperature_raw * (float)vref_cal / (float)vref_raw;

  /* Interpolate between the two factory temperature calibration points. */
  float temperature = (corrected_raw - (float)cal1) *
                          (float)(TEMPSENSOR_CAL2_TEMP - TEMPSENSOR_CAL1_TEMP) /
                          (float)(cal2 - cal1) +
                      (float)TEMPSENSOR_CAL1_TEMP;

  *temperature_c = temperature;
  return HAL_OK;
}
