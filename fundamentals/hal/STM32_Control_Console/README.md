# STM32 Control Console

An educational STM32F407G-DISC1 project using STM32Cube HAL, a 20x4 PCF8574 I2C LCD, a debounced 4x4 keypad, five external LEDs, and a PWM buzzer. This project belongs to `fundamentals/hal`: it practices peripheral drivers, cooperative state machines, and error recovery without an RTOS.

## Menu controls

| Screen | Controls |
| --- | --- |
| Main menu | `1`: LED Control; `2`: System Status; `3`: Timer Settings |
| LED Control | `1`–`5`: toggle green, yellow, red, white, blue; `#`: all off; `*`: back |
| System Status | Uptime and MCU temperature; `*`: back |
| Timer Settings | Enter 1–999 seconds; `D`: clear entry while idle; `#`: start, cancel a running timer, or reset a completed timer; `*`: back |

The countdown continues when leaving its menu. Completion starts a repeating alarm. While the alarm is active, `#` silences it globally and consumes that keypress: it does not also perform the current menu's action. Silencing the alarm leaves the countdown in its completed state until reset.

## Configuration

- CPU: 16 MHz HSI, configured in `main.c`.
- LCD: I2C1 at 100 kHz, PB8/SCL and PB9/SDA; address `0x27` (shifted left for HAL). Backpack mapping: RS bit 0, RW bit 1, Enable bit 2, backlight bit 3, data bits 4–7.
- Keypad: PD0–PD3 open-drain rows and PD8–PD11 input columns with pull-ups.
- LEDs: PE7–PE11, accessed through logical `LedId` identifiers. Status reports the GPIO output latch (ODR), not an independent measurement of the physical LED.
- Temperature: ADC1 internal temperature sensor and VREFINT; calibration constants and reference compensation are used in `temperature.c`.
- Buzzer: TIM9 channel 1 on PE5, driving the existing transistor circuit. The configured tone is 2 kHz at 50% duty, with 200 ms on / 800 ms off while the alarm is active. The buzzer is not driven directly from the GPIO.

Use the existing tested wiring and the project's `.ioc` for pin configuration. I2C1 event and error interrupts must remain enabled for the LCD driver.

## Module responsibilities

| Module | Responsibility |
| --- | --- |
| `main.c` | Services countdown, buzzer, keypad, global alarm acknowledgment, LCD, and console |
| `console.c` | Menu state and screen contents; receives a key through `ConsoleUpdate(char key)` |
| `lcd.c` | Screen buffers, asynchronous I2C transfers, initialization, and recovery |
| `keypad.c` | Matrix scanning and debounced key events |
| `led.c` | Logical LED operations and output-state reporting |
| `countdown.c` | Countdown timing and completion event |
| `buzzer.c` | PWM alarm pattern and acknowledgment state |
| `temperature.c` | ADC sampling and temperature conversion |

## LCD updates and recovery

`LcdClear()`, `LcdSetCursor()`, and `LcdPrint()` edit an 80-character desired-screen buffer. Text must fit within the selected row. `LcdUpdate()` sends changed text in the background through `HAL_I2C_Master_Transmit_IT()`.

Transfers group up to four adjacent changed characters without crossing a visible row boundary. A persistent 24-byte transmit buffer remains valid until HAL finishes. A separate character snapshot updates the displayed-screen buffer only after successful completion, so edits made during a transfer are not lost. A bounded loop advances through immediately ready states without waiting for hardware.

Initialization delays use elapsed-time checks. A transfer that cannot start or complete has a 100 ms software deadline, checked when the main loop services the driver. On an error, the driver stops the peripheral and retries initialization after one second. Successful recovery redraws the main menu; it does not reset LEDs, the countdown, or the alarm.

`LcdUpdate()` returns `HAL_BUSY` during initialization and `HAL_ERROR`/`HAL_TIMEOUT` during faults. `HAL_OK` means the LCD is initialized, not that every queued character has reached the display. I2C1 is dedicated to this driver.

Keypad scanning, countdown processing, and alarm acknowledgment run outside the LCD-ready condition. Other menu actions pause during LCD initialization/recovery. The existing `HAL_Delay(5)` remains in the main loop.

## Build and run

1. Import the existing project into STM32CubeIDE and select the Debug configuration.
2. Build with the project's STM32F407 settings and bundled HAL/CMSIS sources.
3. Flash/debug using ST-LINK. Let initialization and the main-menu redraw finish.
4. Check all menus, LED toggles, temperature updates, and a countdown completed from another menu.

Keep custom initialization and main-loop code inside CubeMX `USER CODE` sections when regenerating configuration.

## Validation checkpoint — 2026-11-13

User-reported board tests passed:

- Four-character transfers improved redraw speed without changing application behavior.
- LED control and background countdown/alarm operation remained functional.
- Global `#` acknowledgment did not also trigger the LED menu's all-off action.
- An injected transfer error recovered automatically to a usable main menu after approximately one second plus initialization/redraw.
- The active alarm could be acknowledged during that recovery interval.

The controlled injection used a breakpoint in `PollTransfer()` at `HAL_StatusTypeDef status = tx_status;`, changed `tx_status` to `HAL_ERROR`, disabled the breakpoint, and resumed execution. Keypad behavior was checked after resuming, not while the CPU was halted. Disable test breakpoints and reset after testing.

These results validate the injected software-error path. Recovery from physically stuck SDA/SCL, disconnected devices, or independent LCD power loss has not been hardware-validated. Peripheral reinitialization is not a guarantee that a physically held bus will recover.

## Remaining limits

- Redraws are incremental, not atomic whole-screen swaps.
- The application is not entirely nonblocking: the main-loop delay remains, and temperature conversion uses bounded ADC polling (10 ms timeout per conversion).
- MCU temperature is a die-temperature indication, not a calibrated ambient thermometer.
- Debugger pauses affect application timing; they are not evidence of uninterrupted real-time operation.

[Back to HAL projects](../README.md)
