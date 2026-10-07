# Button-Controlled LED Controller


A small application that turns the onboard green LED on or off with each press of the USER button. The LED keeps its state after release, and holding the button does not repeatedly toggle it.

The application configures GPIO and SysTick through memory-mapped registers, without HAL calls in the application code. STM32CubeIDE supplies the project, startup, and runtime support files.

**[Explore the project](./) · [Read the application code](Src/main.c)**

## Hardware and tools

- STM32F407 Discovery board
- USB data cable connected to the onboard ST-LINK port
- STM32CubeIDE

No external components or jumper wires are needed for the current version.

| Onboard component | Pin | Behavior |
| --- | --- | --- |
| USER button (B1) | PA0 | HIGH when pressed |
| Green LED | PD12 | On when the output is HIGH |

## What I implemented

- **GPIO initialization:** enable GPIOA and GPIOD clocks through RCC, then configure PA0 as an input and PD12 as an output.
- **Input reading:** read GPIOA's input data register and mask bit 0 to obtain the button state.
- **LED control:** write to GPIOD's BSRR to set or reset PD12 without modifying other outputs.
- **Timekeeping:** configure SysTick to increment a shared `volatile` counter through `SysTick_Handler()`.
- **Debouncing:** accept a changed button state only after the raw reading has remained unchanged for the configured interval.
- **State tracking:** toggle the LED on an accepted press, retain its state while held, and accept release before the next press.

## How debouncing works

1. Read the button during each pass through the main loop.
2. Whenever the raw reading changes, record the current tick count.
3. Wait until 20 ticks have elapsed without another detected change.
4. Accept the new stable state. If it is a press, toggle the LED once.

The main loop continues running throughout this process; there is no blocking delay loop.

SysTick uses the processor clock with a reload value of `15999`. At **16 MHz**, this produces a nominal **1 ms tick**, making the debounce interval approximately **20 ms**. If the processor clock changes, the reload value must be updated.

## Build and run

1. Clone or download this repository.
2. In STM32CubeIDE, import `fundamentals/button_led_controller` as an existing project. A separate IDE workspace can be used; do not copy the project if you want to keep working in the repository.
3. Connect the board through its ST-LINK USB port.
4. Build the project and start an STM32 debug session for it.
5. Resume execution if the debugger stops at `main()`.
6. Press and release the USER button to toggle the green LED. Use USER, not RESET.

The current application does not require a serial terminal or SWV console.

## Hardware verification

The following manual checks passed on the board during development on **October 4, 2026**:

| Check | Observed result |
| --- | --- |
| Resume, suspend, and inspect `system_ticks` in the debugger | Counter increased between observations |
| Press and release repeatedly | LED toggled once per tested press |
| Hold the button for several seconds | LED retained its state after the initial toggle |
| Release the button | LED retained its state |

These are manual functional checks. The exact tick period has not been independently measured, and the debounce behavior has not been exhaustively tested across all input patterns.

## What I learned

- A register's address and the value read through its pointer are different things.
- Clearing a bit field requires an inverted mask; setting bits uses OR masks.
- BSRR uses separate set and reset bits, with a write of `1` requesting either action.
- A button level tells me whether it is pressed now; comparing states lets me detect a new press.
- Software state and hardware actions are separate: `led_state` remembers the state, while `led_on()` and `led_off()` change the output.
- Interrupt-driven timekeeping allows input filtering without stopping the main loop.

## Next milestones

- [x] Configure and control the green LED
- [x] Read the USER button
- [x] Toggle the LED on a new press
- [x] Add SysTick timekeeping and button debounce
- [ ] Distinguish short and long presses
- [ ] Add multiple LED patterns
- [ ] Pause and resume patterns with a long press


[Back to Embedded C Fundamentals](../README.md)
