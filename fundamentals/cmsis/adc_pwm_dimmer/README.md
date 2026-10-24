# ADC/PWM LED Dimmer with LCD Feedback

**Author: Pedro Valle-Cruz**

A potentiometer controls the onboard green LED while a 20×4 I²C LCD displays the commanded brightness percentage. This fundamentals project brings ADC sampling, timer PWM, I²C communication, and LCD control together using CMSIS register definitions rather than HAL drivers.

```text
LED DIMMER
Brightness: 75%
```

## Hardware and tools

- STM32F407 Discovery board (MB997E), onboard ST-LINK, and USB cable
- B50K linear potentiometer, breadboard, and jumper wires
- Freenove 2004 LCD with PCF8574 I²C backpack, tested at address `0x27`
- STM32CubeIDE, GNU Arm compiler, and STM32F407 CMSIS headers

## Connections

Make connections with power disconnected. These connections describe my tested setup using the board’s **3V supply** for the potentiometer and LCD.

| Component | STM32 connection |
| --- | --- |
| Potentiometer outer terminals | 3V and GND |
| Potentiometer center terminal (wiper) | PA1 / ADC1 channel 1 |
| LCD VCC | 3V |
| LCD GND | GND |
| LCD SCL | PB6 / I2C1_SCL, AF4 |
| LCD SDA | PB7 / I2C1_SDA, AF4 |
| Green LED | Onboard PD12 / TIM4_CH1, AF2 |

The backpack supplies the I²C pull-ups in this setup. A level converter and external breadboard supply are not used. Other LCD modules may have different voltage requirements, addresses, or backpack mappings; verify those before changing the wiring. The potentiometer wiper must stay within the ADC supply range.

## What I implemented

The application is in [Src/main.c](Src/main.c).

- **ADC:** 12-bit, right-aligned, single conversions on PA1. A 480-cycle sampling time accommodates the potentiometer’s source impedance.
- **PWM:** TIM4 channel 1 generates nominal 1 kHz PWM. The ADC range of 0–4095 maps to compare values of 0–1000, including fully off and fully on.
- **Timebase:** SysTick provides nominal millisecond ticks for delays, display scheduling, and I²C timeouts. A separate short SysTick delay handles ADC startup before the millisecond timebase is enabled.
- **I²C:** Polling master transfers at nominal 100 kHz, with acknowledgment checks, bounded waits, and STOP cleanup. An optional address probe remains available for diagnostics.
- **LCD:** Four-bit initialization, enable pulses, byte and string writes, and validated cursor positioning for all four rows.
- **Display:** ADC readings are rounded to a whole percentage. The screen is checked at most once per 100 ticks and rewritten only when the percentage changes. Trailing spaces erase old digits without clearing the entire display.
- **Error handling:** Every display operation propagates its result. The cached percentage changes only after a successful complete update. A communication error stops further LCD writes until reset, while the dimmer continues after the failed transfer returns.

The percentage describes the commanded duty cycle; it is not a measurement of perceived or optical brightness.

### Clock and register choices

The firmware assumes the reset clock tree with a nominal 16 MHz HSI system clock and no APB prescaling.

| Peripheral | Configuration |
| --- | --- |
| ADC clock | APB2 / 2 = 8 MHz |
| TIM4 | PSC = 15, ARR = 999 |
| SysTick | LOAD = 15999 for the millisecond timebase |
| I2C1 | FREQ = 16, CCR = 80, TRISE = 17 |

Recalculate these settings if the system clock changes. PWM starts at zero duty to avoid an initial LED flash during LCD initialization.

The backpack mapping is P0 = RS, P1 = R/W, P2 = enable, P3 = backlight, and P4–P7 = D4–D7. Each nibble is transferred with enable low, high, then low. LCD timing depends partly on these complete 100 kHz I²C transactions; revisit timing if transfers are batched or accelerated. The 20×4 DDRAM row offsets are `0x00`, `0x40`, `0x14`, and `0x54`.

## Build and run

1. Import this directory as an existing STM32CubeIDE project and use its Debug configuration.
2. Confirm the `STM32F407xx` symbol and the CMSIS Core and Device include paths are configured.
3. Build, connect the board through ST-LINK, and launch the debugger.
4. Resume execution and turn the potentiometer. The green LED should change brightness and the LCD should show the percentage.
5. Adjust the LCD contrast trimmer if the backlight works but characters are difficult to see.

SWV output is not required. Useful debugger expressions are `potentiometer_reading`, `system_ticks`, `lcd_init_status`, and `lcd_write_status`. Pause execution when inspecting ordinary expressions. Avoid continuously watching I²C status registers during transactions: some register-read sequences clear hardware flags.

## Verification

The original implementation was manually tested on hardware, including:

- ADC readings from 0 to 4095 and increasing readings as the knob turns clockwise
- LED off, intermediate brightness, fully on, and a complete knob sweep
- LCD initialization, text output, and percentage updates
- Display endpoints of 0% and 100%, including transitions from 100% back to a single digit without leftover digits

Review refinements passed an ARM source compilation with `-Wall -Wextra -Werror`. Temporary host checks covered all 101 percentage strings, all 80 cursor positions, invalid arguments, and error propagation at each step of a display update. An exhaustive arithmetic check covered all 4096 ADC inputs for bounded, monotonic PWM values and rounded percentages. Stubbed host checks do not verify peripheral behavior.

**Hardware regression after these refinements is still pending:** repeat the knob sweep and display transitions, check that startup begins with the LED off, and verify that an LCD communication failure is recorded while the dimmer remains responsive after the timeout.

## Limitations and next improvements

- ADC conversion polling currently has no timeout.
- Each I²C waiting phase has its own 10-tick timeout; a complete transaction can take longer than 10 ms. Timeouts require SysTick interrupts to keep running.
- I²C bus errors and arbitration loss are not classified separately. There is no automatic bus recovery or LCD reconnection; reset after correcting a communication fault.
- Resetting I2C1 after GPIO configuration resolved an observed startup BUSY condition. This does not recover a physically stuck bus.
- LCD transactions block foreground sampling. TIM4 continues generating PWM independently. The loop’s 5-tick pause does not establish an exact sampling period.
- Refresh limiting reduces display traffic but does not filter ADC noise near percentage boundaries.

Future work can add ADC timeouts, consolidate repeated I²C cleanup, and separate the ADC, PWM, I²C, and LCD code into small driver modules.

## What I learned

This project connected register-level peripheral setup with a complete user interaction: reading an analog input, controlling a hardware waveform, and presenting feedback. It also made error propagation, data formatting, LCD addressing, and the difference between successful compilation and hardware validation concrete.

Developed incrementally through hands-on implementation and guided review. I wrote and tested the application, then incorporated review improvements to comments, validation, display scheduling, and error handling.

[Back to Embedded C Fundamentals](../../README.md)
