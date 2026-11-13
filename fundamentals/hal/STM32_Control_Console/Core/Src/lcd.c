/* Cooperative 20x4 LCD driver for the PCF8574 backpack.
 * Main-loop code owns display state. I2C callbacks only report completion.
 * I2C1 is dedicated to this driver; do not mix other transfers with it.
 */
#include "lcd.h"
#include "i2c.h"
#include <string.h>

#define LCD_ROWS 4U
#define LCD_COLUMNS 20U
#define LCD_CELLS (LCD_ROWS * LCD_COLUMNS)
#define LCD_TIMEOUT_MS 100U
#define LCD_RETRY_MS 1000U
#define LCD_BATCH_CHARS 4U
#define LCD_BYTES_PER_CHAR 6U

static const uint8_t row_offsets[LCD_ROWS] = {0x00U, 0x40U, 0x14U, 0x54U};

typedef enum {
  LCD_OFF, LCD_POWER_WAIT, LCD_INIT_START, LCD_INIT_WAIT,
  LCD_INIT_DELAY, LCD_READY, LCD_CURSOR_WAIT, LCD_DATA_START,
  LCD_DATA_WAIT, LCD_DATA_DELAY, LCD_FAULT
} LcdState;

typedef struct {
  uint8_t value;
  uint8_t length; /* 1: raw expander byte, 3: nibble, 6: LCD command. */
  uint8_t delay_ms;
} InitStep;

/* First drive Enable low, then restore 4-bit framing even after a failed byte. */
static const InitStep init_steps[] = {
  {LCD_BACKLIGHT, 1U, 1U},
  {0x03U, 3U, 5U}, {0x03U, 3U, 5U}, {0x03U, 3U, 5U},
  {0x02U, 3U, 1U}, {0x28U, 6U, 1U}, {0x08U, 6U, 1U},
  {0x01U, 6U, 2U}, {0x06U, 6U, 1U}, {0x0CU, 6U, 1U}
};

static LcdState state = LCD_OFF;
static HAL_StatusTypeDef fault_status = HAL_OK;
static uint32_t state_since;
static uint8_t init_index;
static char desired[LCD_CELLS];
static char displayed[LCD_CELLS];
static uint8_t cursor_row, cursor_column;
static uint8_t scan_index, pending_index;
static uint8_t pending_count;
static char pending_characters[LCD_BATCH_CHARS];
static uint8_t hardware_address = 0xFFU;

/* HAL keeps this pointer until the completion/error callback. */
static uint8_t tx_data[LCD_BATCH_CHARS * LCD_BYTES_PER_CHAR];
static volatile HAL_StatusTypeDef tx_status = HAL_OK;
static uint32_t tx_started;

static uint8_t Elapsed(uint32_t since, uint32_t duration) {
  return (uint32_t)(HAL_GetTick() - since) >= duration;
}

static void SetState(LcdState next) {
  state = next;
  state_since = HAL_GetTick();
}

static void StopTransfer(void) {
  HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
  HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);
  __HAL_I2C_DISABLE_IT(&hi2c1, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);
  __HAL_I2C_DISABLE(&hi2c1);
  HAL_NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
  HAL_NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
  tx_status = HAL_ERROR;
}

static HAL_StatusTypeDef Fail(HAL_StatusTypeDef status) {
  StopTransfer(); /* Stop hardware access before reusing tx_data. */
  fault_status = status;
  hardware_address = 0xFFU;
  SetState(LCD_FAULT);
  return status;
}

static HAL_StatusTypeDef BeginInitialization(void) {
  StopTransfer();
  HAL_StatusTypeDef status = HAL_I2C_DeInit(&hi2c1);
  if (status != HAL_OK) {
    return Fail(status);
  }
  status = HAL_I2C_Init(&hi2c1);
  if (status != HAL_OK) {
    return Fail(status);
  }
  /* MspInit restores GPIO, clock and NVIC. HAL Init resets the peripheral. */
  tx_status = HAL_OK;
  fault_status = HAL_OK;
  init_index = 0U;
  scan_index = 0U;
  hardware_address = 0xFFU;
  SetState(LCD_POWER_WAIT);
  return HAL_BUSY;
}

static void EncodeNibble(uint8_t nibble, uint8_t control, uint8_t *out) {
  uint8_t data = ((nibble & 0x0FU) << 4) | control | LCD_BACKLIGHT;
  out[0] = data;
  out[1] = data | LCD_EN;
  out[2] = data;
}

static HAL_StatusTypeDef StartTransfer(uint8_t value, uint8_t control,
                                        uint8_t length) {
  /* I2C1 has a single owner. Do not enter HAL's busy-bus waiting loop. */
  if (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY ||
      __HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) != RESET) {
    return HAL_BUSY;
  }

  if (length == 1U) {
    tx_data[0] = value;
  } else if (length == 3U) {
    EncodeNibble(value, control, tx_data);
  } else {
    EncodeNibble(value >> 4, control, tx_data);
    EncodeNibble(value, control, tx_data + 3);
  }

  tx_started = HAL_GetTick();
  tx_status = HAL_BUSY; /* Set before enabling interrupts in HAL. */
  HAL_StatusTypeDef status = HAL_I2C_Master_Transmit_IT(
      &hi2c1, LCD_I2C_ADDRESS, tx_data, length);
  if (status != HAL_OK) {
    tx_status = (status == HAL_BUSY) ? HAL_OK : status;
  }
  return status;
}

static HAL_StatusTypeDef StartTextTransfer(void) {
  /* Do not overwrite a buffer still owned by HAL. */
  if (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY ||
      __HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) != RESET) {
    return HAL_BUSY;
  }

  uint8_t remaining = LCD_COLUMNS - (pending_index % LCD_COLUMNS);
  pending_count = 0U;

  do {
    uint8_t index = pending_index + pending_count;
    uint8_t character = (uint8_t)desired[index];
    uint8_t offset = pending_count * LCD_BYTES_PER_CHAR;

    pending_characters[pending_count] = (char)character;
    EncodeNibble(character >> 4, LCD_RS, &tx_data[offset]);
    EncodeNibble(character, LCD_RS, &tx_data[offset + 3U]);

    pending_count++;
  } while (pending_count < LCD_BATCH_CHARS &&
           pending_count < remaining &&
           desired[pending_index + pending_count] !=
               displayed[pending_index + pending_count]);

  tx_started = HAL_GetTick();
  tx_status = HAL_BUSY;

  HAL_StatusTypeDef status = HAL_I2C_Master_Transmit_IT(
      &hi2c1, LCD_I2C_ADDRESS, tx_data,
      pending_count * LCD_BYTES_PER_CHAR);

  if (status != HAL_OK) {
    tx_status = (status == HAL_BUSY) ? HAL_OK : status;
  }

  return status;
}

static HAL_StatusTypeDef PollTransfer(void) {
  uint32_t interrupt_state = __get_PRIMASK();
  __disable_irq();
  if (tx_status == HAL_BUSY && Elapsed(tx_started, LCD_TIMEOUT_MS)) {
    tx_status = HAL_TIMEOUT;
  }
  HAL_StatusTypeDef status = tx_status;
  __set_PRIMASK(interrupt_state);
  return status;
}

/* The same deadline covers a bus that never permits a transfer to start. */
static HAL_StatusTypeDef CheckStart(HAL_StatusTypeDef status, LcdState next) {
  if (status == HAL_OK) {
    SetState(next);
  } else if (status != HAL_BUSY) {
    return Fail(status);
  } else if (Elapsed(state_since, LCD_TIMEOUT_MS)) {
    return Fail(HAL_TIMEOUT);
  }
  return HAL_OK;
}

HAL_StatusTypeDef LcdInit(void) {
  LcdClear();
  return BeginInitialization();
}

HAL_StatusTypeDef LcdClear(void) {
  memset(desired, ' ', sizeof(desired));
  cursor_row = 0U;
  cursor_column = 0U;
  return HAL_OK;
}

HAL_StatusTypeDef LcdSetCursor(uint8_t row, uint8_t column) {
  if (row >= LCD_ROWS || column >= LCD_COLUMNS) {
    return HAL_ERROR;
  }
  cursor_row = row;
  cursor_column = column;
  return HAL_OK;
}

HAL_StatusTypeDef LcdPrint(const char *text) {
  if (text == NULL) {
    return HAL_ERROR;
  }
  uint8_t length = 0U;
  uint8_t available = LCD_COLUMNS - cursor_column;
  /* Validate first; reject overlong strings without partially editing a row. */
  while (length < available && text[length] != '\0') {
    length++;
  }
  if (text[length] != '\0') {
    return HAL_ERROR;
  }
  memcpy(&desired[cursor_row * LCD_COLUMNS + cursor_column], text, length);
  cursor_column += length;
  return HAL_OK;
}

HAL_StatusTypeDef LcdUpdate(void) {
  HAL_StatusTypeDef status;
  for (uint8_t transitions = 0U; transitions < 4U; transitions++) {
		switch (state) {
			case LCD_OFF:
				return HAL_ERROR;

			case LCD_FAULT:
				if (Elapsed(state_since, LCD_RETRY_MS)) {
					return BeginInitialization();
				}
				return fault_status;

			case LCD_POWER_WAIT:
				if (Elapsed(state_since, 50U)) {
					SetState(LCD_INIT_START);
				}
				return HAL_BUSY;

			case LCD_INIT_START: {
				const InitStep *step = &init_steps[init_index];
				status = CheckStart(StartTransfer(step->value, 0U, step->length),
														LCD_INIT_WAIT);
				return status == HAL_OK ? HAL_BUSY : status;
			}

			case LCD_INIT_WAIT:
				status = PollTransfer();
				if (status == HAL_OK) {
					SetState(LCD_INIT_DELAY);
				} else if (status != HAL_BUSY) {
					return Fail(status);
				}
				return HAL_BUSY;

			case LCD_INIT_DELAY:
				if (!Elapsed(state_since, init_steps[init_index].delay_ms)) {
					return HAL_BUSY;
				}
				init_index++;
				if (init_index < sizeof(init_steps) / sizeof(init_steps[0])) {
					SetState(LCD_INIT_START);
					return HAL_BUSY;
				}
				/* Initialization cleared hardware DDRAM. Desired text can now be sent. */
				memset(displayed, ' ', sizeof(displayed));
				hardware_address = 0U;
				SetState(LCD_READY);
				return HAL_OK;

			case LCD_READY:
				/* Round-robin scan avoids starving later cells during frequent edits. */
				for (uint8_t checked = 0U; checked < LCD_CELLS; checked++) {
					uint8_t index = scan_index;
					scan_index = (scan_index + 1U) % LCD_CELLS;
					if (desired[index] != displayed[index]) {
						pending_index = index;
						SetState(LCD_DATA_START);
						break;
					}
				}
	      if (state == LCD_DATA_START) {
	        continue;
	      }
	      return HAL_OK;

			case LCD_DATA_START: {
				uint8_t address = row_offsets[pending_index / LCD_COLUMNS] +
													pending_index % LCD_COLUMNS;
				if (hardware_address != address) {
					return CheckStart(StartTransfer(0x80U | address, 0U, 6U),
														LCD_CURSOR_WAIT);
				}
				return CheckStart(StartTextTransfer(), LCD_DATA_WAIT);
			}

			case LCD_CURSOR_WAIT:
				status = PollTransfer();
				if (status == HAL_OK) {
					hardware_address = row_offsets[pending_index / LCD_COLUMNS] +
														 pending_index % LCD_COLUMNS;
					/* Subsequent I2C bytes provide command execution time at 100 kHz. */
					SetState(LCD_DATA_START);
					continue;
				} else if (status != HAL_BUSY) {
					return Fail(status);
				}
				return HAL_OK;

			case LCD_DATA_WAIT:
				status = PollTransfer();
				if (status == HAL_OK) {
					for (uint8_t i = 0U; i < pending_count; i++) {
					  displayed[pending_index + i] = pending_characters[i];
					}
					hardware_address += pending_count;
					SetState(LCD_DATA_DELAY);
				} else if (status != HAL_BUSY) {
					return Fail(status);
				}
				return HAL_OK;

	    case LCD_DATA_DELAY:
	      if (Elapsed(state_since, 1U)) {
	        SetState(LCD_READY);
	        continue;
	      }
	      return HAL_OK;

			default:
				return Fail(HAL_ERROR);
	}
 }
  return HAL_OK;
}


void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &hi2c1 && tx_status == HAL_BUSY) {
    tx_status = HAL_OK;
  }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &hi2c1 && tx_status == HAL_BUSY) {
    tx_status = HAL_ERROR;
  }
}
