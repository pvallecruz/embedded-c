# Reusable Peripheral Modules — HAL

**Author: Pedro Valle-Cruz**

A HAL implementation of the [CMSIS peripheral modules](../../cmsis/peripheral_modules/README.md), preserving their public interfaces and nonblocking debounce algorithm. The application toggles a red LED once per accepted button press. Release and continued holding do not toggle it again.

The CMSIS/HAL module comparison is complete for this GPIOE LED and single-button scope. The author wrote the application and module implementations incrementally with guided review and manually tested them on hardware.

## Hardware

STM32F407 Discovery MB997E / STM32F407VG, powered/programmed through ST-LINK USB. Use common STM32 GND with the breadboard supply disconnected.

| Connection | Pin | Configuration |
| --- | --- | --- |
| Red LED | PE7 | Active-high output |
| Yellow LED | PE8 | Active-high output; independent-control test |
| Green LED | PE9 | Supported by LED interface |
| White LED | PE11 | Supported by LED interface; initialize explicitly before use |
| Normally open button | PE10 | Input with internal pull-up; button connects to GND |

Each LED uses its own 330-ohm series resistor between GPIO and anode, with cathode to GND. The final demo uses PE7 and PE10. PE9/PE11 were not separately hardware-tested in this HAL module project. Disconnect power before changing wiring.

## Files and responsibilities

| File | Responsibility |
| --- | --- |
| [Core/Inc/led.h](Core/Inc/led.h) | LED API and contract |
| [Core/Src/led.c](Core/Src/led.c) | Private validation and HAL GPIO operations |
| [Core/Inc/button.h](Core/Inc/button.h) | Raw state and press-event API |
| [Core/Src/button.c](Core/Src/button.c) | HAL input configuration and private debounce state |
| [Core/Src/main.c](Core/Src/main.c) | Application decisions and HAL time snapshots |
| [Core/Src/gpio.c](Core/Src/gpio.c) | CubeMX-generated startup GPIO configuration |
| [Core/Inc/main.h](Core/Inc/main.h) | Generated labels, including USER_BUTTON on PE10 |
| [peripheral_modules_hal.ioc](peripheral_modules_hal.ioc) | CubeMX configuration |

### LED contract

```c
void led_init(unsigned int pin);
void led_on(unsigned int pin);
void led_off(unsigned int pin);
void led_toggle(unsigned int pin);
```

Pass GPIOE pin numbers **7, 8, 9, or 11**, not masks. Validation precedes any shift or peripheral access. Invalid pins are ignored without a status return; PE10 is deliberately excluded.

`led_init(pin)` enables GPIOE's clock, converts the pin to `1U << pin`, clears the output latch, then applies push-pull output, no pull-up/down, and low speed through `HAL_GPIO_Init()`. Call it before controlling each LED. On/off use `HAL_GPIO_WritePin()`, while toggle uses `HAL_GPIO_TogglePin()`. The caller must maintain exclusive ownership of those outputs: HAL's toggle reads the output latch before writing, so the complete operation is not atomic against competing writers.

CubeMX currently initializes PE7, PE8, and PE9 at startup. The module independently configures its selected pin to preserve the CMSIS initialization contract; PE11 becomes an output when `led_init(11U)` is called. The generated setup and module setup agree for the shared pins.

### Button contract

```c
void button_init(void);
int button_is_pressed(void);
int button_update(uint32_t now);
```

`button_init()` enables GPIOE, configures the generated `USER_BUTTON` label as input/pull-up, and seeds private raw/stable states from the actual button reading. A held button at startup is not a press event. The clock enable remains GPIOE-specific; moving the label to another port would require updating that dependency too.

`button_is_pressed()` compares `HAL_GPIO_ReadPin()` with `GPIO_PIN_RESET` and returns the raw logical state: **1 pressed, 0 released**.

Call `button_update(now)` regularly from one polling loop with a wrapping 32-bit millisecond timestamp. Every raw edge restarts a 20 ms stability interval. An accepted press transition returns 1 once; release updates the stable state but returns 0. Other calls return 0. Consume each result immediately: events are not queued. Very short presses may be ignored, and release must stabilize before another press can be accepted.

The module owns one button's persistent state; it is not a multiple-instance or concurrent-caller driver. Unsigned elapsed subtraction tolerates wrap with regular polling, provided the application does not miss a full counter period or independently reset the timebase while a transition is pending.

## Comparison with CMSIS

| Responsibility | CMSIS | HAL |
| --- | --- | --- |
| GPIO setup | Direct RCC, MODER, OTYPER, PUPDR, OSPEEDR operations | Clock macro, GPIO_InitTypeDef, HAL_GPIO_Init |
| Output set/reset | BSRR writes | HAL_GPIO_WritePin |
| Toggle | ODR test followed by on/off helper | HAL_GPIO_TogglePin |
| Raw input | IDR mask compared with zero | HAL_GPIO_ReadPin compared with GPIO_PIN_RESET |
| Time snapshot | Application system_ticks counter | HAL_GetTick |
| Public interfaces and debounce | Same contracts and algorithm | Same contracts and algorithm |

The button algorithm contains no HAL time calls. Passing time into `button_update()` lets the application change its timebase without rewriting debounce. The HAL GPIO definitions stay in implementation files rather than leaking into the public interfaces.

## Clock, generation, and build

Created with STM32CubeMX using STM32Cube FW_F4 V1.28.3, C, STM32CubeIDE output, and Generate Under Root. Necessary drivers are included in `Drivers/`. GPIO initialization is generated separately in `gpio.c`.

The generated clock setup uses nominal **16 MHz HSI**, PLL disabled, AHB/APB prescalers of one. HAL's default SysTick timebase supplies milliseconds: SysTick calls `HAL_IncTick()`, and `HAL_GetTick()` reads the counter. `main()` calls `HAL_Init()` and `SystemClock_Config()` before GPIO/module initialization. No `HAL_Delay()` is used in the application; polling remains nonblocking but does not sleep. Debugger halts, interrupt masking, and oscillator accuracy affect timing.

1. Import this directory into CubeIDE as `peripheral_modules_hal`.
2. Build the Debug configuration.
3. Launch **peripheral_modules_hal Debug** and verify the Debug view shows **peripheral_modules_hal.elf**. The CMSIS project has similar behavior, so observing the LED alone cannot identify which executable is running.
4. Resume if stopped at `main()`. Red starts off; each accepted press toggles it once.

Application edits in generated `main.c` belong inside the existing USER CODE markers; Keep User Code when re-generating is enabled. The module includes are protected there too. The separate `led.c/.h` and `button.c/.h` are application-owned files. Keep them in version control and review changes after regeneration. Generated comments outside user sections may return. Debug/Release outputs and workspace metadata are ignored by repository rules.

## Verification

The following results were confirmed manually on hardware with **peripheral_modules_hal.elf selected**, after correcting an earlier launch of the CMSIS executable:

- PE7/PE8 initialization and independent output control: both off, red on, both on, yellow only, both off. This exercised on, off, and toggle through HAL.
- All four LED functions rejected PE10. Its MODER, PUPDR, OSPEEDR, OTYPER fields and output bit stayed unchanged; red and yellow stayed on. PE10 remained input (`MODER = 0`) with pull-up (`PUPDR = 1`). This was a selected-field check, not an instrumented audit of every register bit.
- The final restored application toggled red once per accepted press, retained state on release, and did not repeat while held.
- Holding during startup generated no event; releasing and pressing again worked.
- A press debounce interval crossed HAL tick wraparound and produced one event, followed by normal release/repress behavior.

### Tick wrap procedure

With a fresh press held, pause in `button_update()` on `button_changed_at = now`. Set both HAL's `uwTick` and the current frame's local `now` to `0xFFFFFFF0`. Step over the assignment, disable that breakpoint, and break on `button_stable = button_raw` inside the stability check. Resume while holding.

At acceptance, `button_changed_at` remained `0xFFFFFFF0` and `now` had wrapped to a small value of at least 4: at least 20 ms had elapsed across zero. Remove breakpoints and resume; one event is reported while held, and a subsequent release/repress works. Restart normally afterward to clear debugger-injected state.

This directly tested a **press** interval across wrap, not a separate release interval. Timing is manually observed, not oscilloscope measured; contact bounce was not instrumented and there is no automated test suite yet.

### Final review and build

Comment/formatting cleanup preserved executable tokens and all CubeMX user-code markers in the five application/module files. The module includes were moved into their protected section without changing include order. The cleaned Debug sources rebuilt and linked successfully using CubeIDE's bundled GNU Tools for STM32, with no warnings or errors reported. The board was not reflashed after this cosmetic cleanup. Release was not independently built during closeout.

## Completion and next step

The CMSIS/HAL comparison is complete within the documented scope and verification limits. No performance or firmware-size superiority claim is made; that would require a controlled comparison of build configurations and linked code.

Next on the established roadmap is the **sensor data logger**, followed by fault recovery, automated parser tests, and the integrated monitoring station.

[CMSIS counterpart](../../cmsis/peripheral_modules/README.md) · [HAL index](../README.md) · [Fundamentals index](../../README.md)
