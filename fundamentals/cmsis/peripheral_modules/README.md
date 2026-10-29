# Reusable Peripheral Modules — CMSIS

**Author: Pedro Valle-Cruz**

A small STM32F407 project separating LED control and button debounce into `.c`/`.h` modules. The demonstration toggles the red LED once per accepted button press. Holding or releasing the button does not toggle it again.

The CMSIS implementation and manual hardware checks are complete for the scope below. The separate [HAL counterpart](../../hal/peripheral_modules_hal/README.md) now has matching public interfaces and verified hardware behavior; the comparison is complete within the documented scope.

## Hardware

- STM32F407 Discovery MB997E / STM32F407VG, powered and programmed through ST-LINK USB
- PE7 red LED, PE8 yellow, PE9 green, PE11 white; each active-high LED has its own 330-ohm series resistor and cathode connected to STM32 GND
- PE10 normally open button to GND, with an internal pull-up
- Breadboard supply disconnected; all circuits share STM32 GND

The final demonstration uses PE7 and PE10. PE8 was also used for independent-output testing. The LED API permits PE7, PE8, PE9, and PE11; this module's tests did not separately exercise PE9 and PE11. Disconnect power before changing wiring.

## Module boundaries

| File | Responsibility |
| --- | --- |
| [Inc/led.h](Inc/led.h) | Public LED interface and usage contract |
| [Src/led.c](Src/led.c) | GPIOE register configuration, validation, and output control |
| [Inc/button.h](Inc/button.h) | Raw input and debounced press-event interface |
| [Src/button.c](Src/button.c) | PE10 configuration and private persistent debounce state |
| [Src/main.c](Src/main.c) | Millisecond timebase and application decisions |

Headers declare what callers can use. Source files implement those operations. The LED validation helper and button state are `static`, keeping them private to their implementation files. Each module enables GPIOE's clock itself, so initialization does not depend on the other module running first.

### LED interface

```c
void led_init(unsigned int pin);
void led_on(unsigned int pin);
void led_off(unsigned int pin);
void led_toggle(unsigned int pin);
```

Pass a **pin number**, not a bit mask. Supported pins are **7, 8, 9, and 11 on GPIOE**. Call `led_init(pin)` before controlling each LED. Unsupported pins return without hardware access or shifts, and no error status is returned.

Initialization clears the output latch before selecting output mode, then configures push-pull, no pull-up/down, and low output speed. Single-bit masks address OTYPER and BSRR; two-bit fields configure MODER, PUPDR, and OSPEEDR. Other pins are preserved.

On/off use direct BSRR writes. Toggle reads ODR and calls on/off; the complete read-and-write sequence is not atomic. The application must give the module exclusive control of those outputs. This is a GPIOE/wiring-specific reusable module, not a generic driver for every port or active-low LEDs.

### Button interface

```c
void button_init(void);
int button_is_pressed(void);
int button_update(uint32_t now);
```

- `button_init()` configures PE10 as input with pull-up and seeds debounce from the actual input. A button held at initialization is not a new press.
- `button_is_pressed()` returns the **raw logical state**: 1 pressed, 0 released. It translates the active-low electrical reading; it does not debounce.
- `button_update(now)` returns **1 once for a newly accepted press**, otherwise 0. Call it once per polling iteration with the current 32-bit millisecond timestamp and consume its result immediately; events are not queued.

Every raw change restarts a 20 ms stability interval. A stable transition updates the accepted state; only a transition to pressed reports an event. Release must also be accepted before another press can be recognized. Private state supports one button and one polling caller, not multiple independent instances or concurrent callers.

The button module does not configure SysTick or include HAL timing calls. Supplying `now` keeps the debounce algorithm independent of the particular timebase. Unsigned `now - button_changed_at` comparisons tolerate counter wrap when polled regularly; do not reset the running timebase independently of the debounce state or miss an entire counter period. Very short presses may be ignored.

## Application and timing

`main()` initializes the button first, then the red LED. A SysTick interrupt increments a volatile 32-bit counter. Each loop snapshots that counter, calls `button_update(now)`, and toggles PE7 only when an event is returned.

The application uses `SysTick_Config(16000U)` and checks for a nonzero failure result. It assumes the reset clock of **nominal 16 MHz HSI**, giving one interrupt per 16,000 core clock cycles, approximately 1 ms. If the core clock changes, the configuration must change too. Interrupt masking, debugger stops, and oscillator accuracy affect timing.

The loop polls continuously without blocking delays or low-power sleep. The interrupt only updates the counter; GPIO decisions and debounce run in the foreground.

## Build and run

1. Import this folder into STM32CubeIDE as an existing project named `peripheral_modules`.
2. Use the supplied CMSIS core/device headers under `Drivers/CMSIS`. Both Debug and Release configurations include their paths and define `STM32F407xx`; `DEBUG` belongs only to Debug.
3. Select this project and build it, then run/debug it through ST-LINK. Resume if stopped at `main()`.
4. Red starts off. Press and release to turn it on; the next press turns it off. Holding should cause no repeated changes.

This is an empty CubeIDE project configured for CMSIS, not a CubeMX-generated HAL project. Application code does not require CubeMX user-code markers. Build outputs and workspace metadata are excluded by the repository's ignore rules.

## Manual verification

The author performed and reported these checks during development:

| Check | Observed result |
| --- | --- |
| CMSIS project setup | Build completed without errors or warnings |
| PE7 initialization, on, off, toggle | Expected output states observed by stepping in the debugger |
| PE8 parameterized control | On/off toggling worked |
| PE7 and PE8 together | Changing one LED left the other unchanged |
| PE10 rejected by all LED functions | PE10 remained untouched during the debugger/register check |
| Private validation helper refactor | Independent control and rejection behaved as before |
| Raw button plus LED modules | Pressed red on, released red off |
| Debounced application | One toggle per press, no toggle on release |
| Long hold | One event only; a later fresh press worked |
| Button held through startup | No initial event; release then fresh press worked |
| Press debounce across tick wrap | Accepted after the wrapped elapsed interval, without repeat while held |

### Debounce wrap test

1. Break on `button_changed_at = now` after a fresh press and keep the button held.
2. While paused, set both `system_ticks` and the `button_update()` frame's local `now` to `0xFFFFFFF0`.
3. Step over the assignment so `button_changed_at` receives that value. Disable this breakpoint and break on `button_stable = button_raw` inside the accepted-change block.
4. Resume. At acceptance, the timestamp remained `0xFFFFFFF0` and `now` had wrapped to a small value of at least 4. The elapsed subtraction therefore reached at least 20 ms across zero.
5. Remove the breakpoints and resume: the LED toggled once while held, and release followed by a fresh press produced another toggle. Restart normally after injected counter values.

This directly exercised a **press transition** across wrap. A release transition was not separately forced across wrap. These are manual observations, not oscilloscope timing measurements or automated tests; rapid physical contact bounce was not instrumented.

The final comment/formatting cleanup preserved all executable tokens in the five application/module files. No fresh build or hardware run after that cosmetic cleanup has been recorded.

## What I learned and next step

This exercise separates an application's decisions from hardware access through public interfaces. It practices header guards, parameterized pin masks, validation before shifts, private helpers, persistent module state, and event-based debounce with an injected time value.

The completed [HAL counterpart](../../hal/peripheral_modules_hal/README.md) retains the public contracts, implements GPIO access with HAL, and passes `HAL_GetTick()` into the same debounce logic. Its README records the corresponding tests. The broader roadmap continues afterward with the sensor data logger, fault recovery, parser tests, and integrated monitoring station.

[Back to CMSIS Projects](../README.md) · [Back to Embedded C Fundamentals](../../README.md)
