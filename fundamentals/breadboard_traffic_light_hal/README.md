# Breadboard Traffic Light — HAL Comparison

**Author: Pedro Valle-Cruz**

A nonblocking STM32F407 traffic light with a debounced pedestrian button and a white crossing indicator, implemented with STM32Cube HAL. This separate exercise reproduces the behavior of the [CMSIS traffic light](../breadboard_traffic_light/README.md) using the same hardware. The CMSIS project remains unchanged for comparison.

CubeMX generates hardware initialization; the application state machine, elapsed-time checks, debounce, and pedestrian logic were written incrementally by the author with guided review. This is a learning demonstration, not a controller for real road traffic.

## Hardware and wiring

STM32F407 Discovery MB997E with STM32F407VG, powered and programmed through ST-LINK USB. The breadboard power supply is disconnected.

| Connection | Pin | Configuration |
| --- | --- | --- |
| Red LED | PE7 | Active-high push-pull output, low speed |
| Yellow LED | PE8 | Active-high push-pull output, low speed |
| Green LED | PE9 | Active-high push-pull output, low speed |
| White pedestrian LED | PE11 | Active-high push-pull output, low speed |
| Normally open button | PE10 | Input with internal pull-up; other contact to GND |

Each LED has its own 330-ohm series resistor: GPIO → resistor → LED anode, with its cathode connected to common STM32 GND. Outputs have no pull-up/down and initialize low. Released button reads `GPIO_PIN_SET`; pressed reads `GPIO_PIN_RESET`. Disconnect power before changing wiring.

## Behavior

Startup selects red with white off. The sequence repeats **red → green → yellow → red**.

| Phase | Duration | Pedestrian behavior |
| --- | --- | --- |
| Red without request | 10 s | White off |
| Green without request | 10 s | White off |
| Green with accepted request | At least 2 s total from green entry | Ends once the minimum has elapsed |
| Yellow | 5 s | Pending request retained; white off |
| Red with request | 10 s | White steady for 8 s, then toggles every 250 ms for the final 2 s |

A raw button change restarts a 20 ms stability interval. Only an accepted transition to pressed during green latches a request. Multiple presses coalesce into one request. Holding does not repeat it, and a button held at startup must be released and pressed again. Presses accepted during red or yellow do not queue a future request; classification uses the phase when debounce accepts the transition.

Red turns on before white. The first white toggle occurs at eight seconds of requested red; 250 ms toggles produce an approximately 500 ms full blink cycle. At red's end, white turns off and the request clears before green turns on.

## Project and build

- Configuration: [breadboard_traffic_light_hal.ioc](breadboard_traffic_light_hal.ioc)
- Application: [Core/Src/main.c](Core/Src/main.c)
- Pin labels: [Core/Inc/main.h](Core/Inc/main.h)
- Interrupt handler: [Core/Src/stm32f4xx_it.c](Core/Src/stm32f4xx_it.c)
- HAL driver: [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c](Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c)

Created in standalone STM32CubeMX for STM32F407VGT6, using **STM32Cube FW_F4 V1.28.3**, C, STM32CubeIDE output, and Generate Under Root. Necessary library files are copied into the project. Debug is Serial Wire; the HAL timebase is SysTick.

1. Import this directory as an existing STM32CubeIDE project. If it is already in the workspace, open the existing entry.
2. Select `breadboard_traffic_light_hal` and build it.
3. Connect the Discovery board through ST-LINK USB and run/debug this project. Resume if execution stops at `main()`.
4. Verify normal timing, then press during green to request the crossing indication.

To change hardware configuration, open the `.ioc` in CubeMX and regenerate for STM32CubeIDE. Keep application edits within the existing `USER CODE BEGIN` / `USER CODE END` regions and retain those markers. Keep User Code when re-generating is enabled. Generated explanatory comments outside those regions may return after regeneration. Review changes and rebuild afterward. `Debug/` and `Release/` are ignored build outputs.

## HAL and CMSIS comparison

| CMSIS version | HAL version | Purpose |
| --- | --- | --- |
| RCC GPIOE enable bit | `__HAL_RCC_GPIOE_CLK_ENABLE()` | Enable the GPIO peripheral clock |
| `MODER`, `OTYPER`, `OSPEEDR`, `PUPDR` | `GPIO_InitTypeDef` and `HAL_GPIO_Init()` | Configure pins |
| Direct `BSRR` writes | `HAL_GPIO_WritePin()` | Set/reset LED outputs |
| `IDR` read | `HAL_GPIO_ReadPin()` | Sample the button |
| Output-latch check and LED helpers | `HAL_GPIO_TogglePin()` | Toggle white |
| `system_ticks` | `HAL_GetTick()` | Read elapsed-time reference |

The application retains the enum and `switch` state machine. Each loop takes one time snapshot, checks the button, and evaluates the current phase. No `HAL_Delay()` is used for sequencing. The loop polls continuously; it does not sleep.

### Clock and timebase

The generated clock setup selects nominal **16 MHz HSI**, with AHB/APB prescalers of one. `HAL_Init()` initializes HAL and its default tick; clock configuration updates the timebase for the selected clock.

The default HAL tick setting is 1 kHz. `HAL_InitTick()` uses `HAL_SYSTICK_Config(SystemCoreClock / (1000U / uwTickFreq))`: at 16 MHz and `uwTickFreq = 1`, this is 16,000 clock cycles per interrupt. `SysTick_Handler()` calls `HAL_IncTick()`, which increments `uwTick`; `HAL_GetTick()` returns the counter in milliseconds.

Unsigned 32-bit `now - timestamp` comparisons tolerate wraparound for these short intervals, provided the loop checks regularly and does not miss a complete counter period. Blink timestamps advance to the observed time, so stalls can delay or skip visible blinks rather than replay them.

## Verification

The author reported the following manual hardware results during development:

- Generated-project baseline built with no errors or warnings.
- All four LEDs worked individually; white followed the active-low button.
- Red toggled every second while white continued responding to the button, demonstrating nonblocking timing.
- Normal red 10 s, green 10 s, yellow 5 s sequence repeated with white off.
- Early green press respected the two-second minimum; a later press ended green promptly after debounce.
- Requested red showed white steady for eight seconds and blinking for two, then off before green.
- Holding through another cycle did not create another request.
- Fresh presses during yellow and red left subsequent cycles normal.
- Holding at startup did not create a request; release and a fresh green-phase press worked.
- Multiple green-phase presses produced one crossing indication, not queued crossings.

### Counter-wrap checks

1. **Initial red:** paused before `state_started` initialization, set `uwTick` to `0xFFFFF000`, and resumed. The counter wrapped after approximately 4.1 seconds; initial red still lasted approximately ten seconds.
2. **Requested red:** paused in the yellow-to-red transition on `state_started = now`, with a request pending. Set both `uwTick` and the captured local `now` to `0xFFFFE100`, then resumed so both red timestamps used that value. Wrap occurred approximately 7.94 seconds later, just before the blink interval. White stayed steady for approximately eight seconds, blinked for two, and turned off before green.

The second test checks requested-red elapsed timing across wrap and the later transition into blinking. It does not separately force a 250 ms blink interval across wrap. **Button debounce was not directly forced across wrap.** Remove test breakpoints and restart normally after debugger counter changes.

These are author-observed manual tests, with approximate timing rather than instrumented measurements or automated coverage. The final source cleanup changed comments and formatting only; executable tokens and CubeMX markers were compared and preserved. A fresh build after that cleanup was not independently verified.

## Completion and limitations

The agreed HAL comparison behavior and manual functional checks are complete, with wrap coverage limited as described above. Commit and push remain the author's steps.

This project does not implement multiple traffic directions, low-power operation, a watchdog, or fault recovery for a corrupted enum state. Very short button presses may be ignored. Debugger halts, prolonged interrupt masking, oscillator accuracy, and execution stalls affect timing.

This comparison sits between the completed CMSIS traffic light and the next roadmap project: reusable peripheral modules. The remaining roadmap continues with a sensor data logger, fault recovery, automated parser tests, and an integrated monitoring station.

## What I learned

HAL packages familiar register operations into reusable APIs. CubeMX makes pin and clock choices visible and generates their initialization, while the application still requires deliberate timing, state transitions, debounce, and request handling. Tracing the drivers connected the HAL GPIO calls and SysTick timebase to the register-level implementation.

[Back to Embedded C Fundamentals](../README.md)
