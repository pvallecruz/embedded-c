/*
 * retroscope_ui.c
 *
 *  Created on: Nov 15, 2026
 *      Author: pedrol.valle-cruz
 */
#include "retroscope_ui.h"

#include <stdint.h>

#include "st7796.h"

// Screen regions
#define HEADER_HEIGHT 34U
#define WAVEFORM_HEIGHT 224U
#define INFO_HEIGHT 62U

#define WAVEFORM_Y HEADER_HEIGHT
#define INFO_Y (HEADER_HEIGHT + WAVEFORM_HEIGHT)

// Interface colors (RGB565)
#define UI_BACKGROUND 0x0000U
#define UI_PANEL_COLOR 0x1948U
#define UI_WAVE_COLOR 0x0841U
#define UI_BORDER_COLOR 0x4A69U
#define UI_WAVEFORM_COLOR 0xFFE0U

// Graticule geometry
#define GRID_X 40U
#define GRID_Y 46U
#define GRID_WIDTH 400U
#define GRID_HEIGHT 200U
#define GRID_SPACING 40U

// Graticule colors (RGB565)
#define GRID_COLOR 0x2B4AU
#define GRID_AXIS_COLOR 0x5EAFU

// Waveform geometry
#define WAVEFORM_SAMPLES RETROSCOPE_WAVEFORM_SAMPLES

// Rendering buffer
#define RENDER_STRIP_WIDTH 20U
#define RENDER_BUFFER_WIDTH (RENDER_STRIP_WIDTH + 1U)
#define RENDER_BUFFER_SIZE (RENDER_BUFFER_WIDTH * (GRID_HEIGHT + 1U))

static uint16_t Waveform[WAVEFORM_SAMPLES];
static uint16_t RenderBuffer[RENDER_BUFFER_SIZE];
static uint16_t PreviousWaveform[WAVEFORM_SAMPLES];
static uint8_t PreviousWaveformValid = 0U;
static uint16_t NextStripX = GRID_X;
static uint8_t FrameActive = 0U;

volatile uint16_t StripsDrawn = 0U;

void RetroScope_DrawLayout(void) {
  // Clear entire screen
  ST7796_FillScreen(UI_BACKGROUND);

  // Header
  ST7796_FillRect(0, 0, ST7796_WIDTH, HEADER_HEIGHT, UI_PANEL_COLOR);

  // Waveform viewport
  ST7796_FillRect(0, WAVEFORM_Y, ST7796_WIDTH, WAVEFORM_HEIGHT, UI_WAVE_COLOR);

  // Bottom information bar
  ST7796_FillRect(0, INFO_Y, ST7796_WIDTH, INFO_HEIGHT, UI_PANEL_COLOR);

  // Separators
  ST7796_DrawLine(0, WAVEFORM_Y, ST7796_WIDTH - 1, WAVEFORM_Y, UI_BORDER_COLOR);

  ST7796_DrawLine(0, INFO_Y, ST7796_WIDTH - 1, INFO_Y, UI_BORDER_COLOR);
}

void RetroScope_DrawGrid(void) {
  const uint16_t x_end = GRID_X + GRID_WIDTH;
  const uint16_t y_end = GRID_Y + GRID_HEIGHT;

  const uint16_t x_center = GRID_X + GRID_WIDTH / 2;
  const uint16_t y_center = GRID_Y + GRID_HEIGHT / 2;

  // Draw vertical grid lines
  for (uint16_t x = GRID_X; x <= x_end; x += GRID_SPACING) {
    ST7796_DrawLine(x, GRID_Y, x, y_end, GRID_COLOR);
  }

  // Draw horizontal grid lines
  for (uint16_t y = GRID_Y; y <= y_end; y += GRID_SPACING) {
    ST7796_DrawLine(GRID_X, y, x_end, y, GRID_COLOR);
  }

  // Draw brighter vertical reference axis
  ST7796_DrawLine(x_center, GRID_Y, x_center, y_end, GRID_AXIS_COLOR);

  // Draw brighter horizontal reference axis
  ST7796_DrawLine(GRID_X, y_center, x_end, y_center, GRID_AXIS_COLOR);
}

void RetroScope_DrawLabels(void) {
  // Header title
  ST7796_DrawString(12, 10, "RETROSCOPE", ST7796_WHITE, 2);

  // Header status indicators
  ST7796_DrawString(360, 10, "RUN", ST7796_GREEN, 2);

  ST7796_DrawString(420, 10, "AUTO", ST7796_WHITE, 1);

  // Bottom information panel
  ST7796_DrawString(15, 273, "CH1", ST7796_GREEN, 2);

  ST7796_DrawString(110, 273, "V/DIV", ST7796_WHITE, 2);

  ST7796_DrawString(250, 273, "TIME/DIV", ST7796_WHITE, 2);
}

static void RetroScope_GetStripBounds(uint16_t start_x, uint16_t* top,
                                      uint16_t* bottom) {
  uint16_t first = start_x - GRID_X;

  uint16_t min_y = Waveform[first];
  uint16_t max_y = Waveform[first];

  for (uint16_t i = 0; i <= RENDER_STRIP_WIDTH; i++) {
    uint16_t index = first + i;
    uint16_t current_y = Waveform[index];

    if (current_y < min_y) min_y = current_y;
    if (current_y > max_y) max_y = current_y;

    if (PreviousWaveformValid) {
      uint16_t previous_y = PreviousWaveform[index];

      if (previous_y < min_y) min_y = previous_y;
      if (previous_y > max_y) max_y = previous_y;
    }
  }

  const uint16_t grid_bottom = GRID_Y + GRID_HEIGHT - 1U;

  if (min_y < GRID_Y) {
    min_y = GRID_Y;
  }

  if (max_y > grid_bottom) {
    max_y = grid_bottom;
  }

  *top = (min_y > GRID_Y) ? min_y - 1U : GRID_Y;

  *bottom = (max_y < grid_bottom) ? max_y + 1U : grid_bottom;
}

static uint8_t RetroScope_StripChanged(uint16_t start_x) {
  if (!PreviousWaveformValid) {
    return 1U;
  }

  uint16_t first = start_x - GRID_X;

  // Compare all 21 samples belonging to this strip.
  for (uint16_t i = 0; i <= RENDER_STRIP_WIDTH; i++) {
    uint16_t index = first + i;

    if (Waveform[index] != PreviousWaveform[index]) {
      return 1U;
    }
  }

  return 0U;
}

static void RetroScope_DrawWaveformStrip(uint16_t start_x) {
  uint16_t strip_top;
  uint16_t strip_bottom;

  RetroScope_GetStripBounds(start_x, &strip_top, &strip_bottom);

  uint16_t strip_height = strip_bottom - strip_top + 1U;

  uint16_t strip_width = RENDER_STRIP_WIDTH;

  // Final strip includes the rightmost endpoint.
  if (start_x + strip_width == GRID_X + GRID_WIDTH) {
    strip_width++;
  }

  // Prepare background and grid.
  for (uint16_t y = 0; y < strip_height; y++) {
    uint16_t screen_y = strip_top + y;

    for (uint16_t x = 0; x < strip_width; x++) {
      uint16_t screen_x = start_x + x;
      uint16_t color = UI_WAVE_COLOR;

      if ((screen_x - GRID_X) % GRID_SPACING == 0U ||
          (screen_y - GRID_Y) % GRID_SPACING == 0U) {
        color = GRID_COLOR;
      }

      if (screen_x == GRID_X + GRID_WIDTH / 2U ||
          screen_y == GRID_Y + GRID_HEIGHT / 2U) {
        color = GRID_AXIS_COLOR;
      }

      RenderBuffer[y * RENDER_BUFFER_WIDTH + x] = color;
    }
  }

  // Draw the current waveform.
  uint16_t first_sample = start_x - GRID_X;

  for (uint16_t x = 0; x < RENDER_STRIP_WIDTH; x++) {
    uint16_t index = first_sample + x;

    uint16_t y0 = Waveform[index];
    uint16_t y1 = Waveform[index + 1U];

    uint16_t min_y = (y0 < y1) ? y0 : y1;
    uint16_t max_y = (y0 > y1) ? y0 : y1;

    for (uint16_t y = min_y; y <= max_y; y++) {
      if (y >= strip_top && y <= strip_bottom) {
        uint16_t local_y = y - strip_top;

        RenderBuffer[local_y * RENDER_BUFFER_WIDTH + x] = UI_WAVEFORM_COLOR;
      }
    }
  }

  // Draw the last endpoint in the final strip.
  if (strip_width > RENDER_STRIP_WIDTH) {
    uint16_t final_y = Waveform[WAVEFORM_SAMPLES - 1U];

    if (final_y >= strip_top && final_y <= strip_bottom) {
      RenderBuffer[(final_y - strip_top) * RENDER_BUFFER_WIDTH +
                   RENDER_STRIP_WIDTH] = UI_WAVEFORM_COLOR;
    }
  }

  // Our buffer uses a 21-pixel row stride.
  // Pack active rows to match the LCD transfer width.
  for (uint16_t y = 1; y < strip_height; y++) {
    for (uint16_t x = 0; x < strip_width; x++) {
      RenderBuffer[y * strip_width + x] =
          RenderBuffer[y * RENDER_BUFFER_WIDTH + x];
    }
  }

  ST7796_DrawRGB565Buffer(start_x, strip_top, strip_width, strip_height,
                          RenderBuffer);
}

uint8_t RetroScope_IsFrameActive(void) {
  return FrameActive;
}

void RetroScope_BeginFrame(void) {
  if (FrameActive != 0U) {
    return;
  }

  NextStripX = GRID_X;
  StripsDrawn = 0U;
  FrameActive = 1U;
}

void RetroScope_RenderStep(void) {
  if (FrameActive == 0U) {
    return;
  }

  if (RetroScope_StripChanged(NextStripX)) {
    RetroScope_DrawWaveformStrip(NextStripX);
    StripsDrawn++;
  }

  NextStripX += RENDER_STRIP_WIDTH;

  if (NextStripX >= GRID_X + GRID_WIDTH) {
    for (uint16_t i = 0U; i < WAVEFORM_SAMPLES; i++) {
      PreviousWaveform[i] = Waveform[i];
    }

    PreviousWaveformValid = 1U;
    FrameActive = 0U;
  }
}

void RetroScope_LoadAdcWaveform(const uint16_t* samples) {
  if (samples == NULL) {
    return;
  }

  for (uint16_t i = 0U; i < WAVEFORM_SAMPLES; i++) {
    uint16_t adc = samples[i];

    if (adc > 4095U) {
      adc = 4095U;
    }

    // Map 0–4095 to the vertical graticule.
    uint32_t scaled = ((uint32_t)adc * (GRID_HEIGHT - 1U)) / 4095U;

    // Screen Y increases downward.
    Waveform[i] = GRID_Y + GRID_HEIGHT - 1U - (uint16_t)scaled;
  }
}
