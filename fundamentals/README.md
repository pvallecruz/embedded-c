# Embedded C Fundamentals

Small, hands-on projects exploring how embedded software interacts with real hardware.

This collection documents my progress with register-level C programming and HAL comparisons on the STM32F407 Discovery board. Each project grows through practical milestones: understand the hardware, implement a feature, test it on the board, and explain what I learned.

## Implementation approaches

All projects currently run without an operating system. Here, the folders distinguish how the application accesses hardware; “bare-metal” is an umbrella description, not a separate category.

| Folder | Approach |
| --- | --- |
| [manual-registers](manual-registers/README.md) | Manually defined peripheral addresses, register layouts, and bit operations |
| [cmsis](cmsis/README.md) | Direct register access using CMSIS core and STM32 device headers |
| [hal](hal/README.md) | STM32 HAL APIs with CubeMX-generated initialization |

Project names remain unchanged so existing build configurations and comparisons stay recognizable.

## Project index

| Project | Focus | Current milestone |
| --- | --- | --- |
| [Button-Controlled LED Controller](manual-registers/button_led_controller/README.md) | GPIO, bit manipulation, SysTick, state tracking, and debouncing | One LED toggle per debounced press; manually tested on hardware |
| [Reaction Timer](manual-registers/reaction_timer/README.md) | State machines, elapsed-time measurement, pseudorandom delays, debouncing, and SWV output | Replayable game with early-press rejection and best-time tracking; manually tested on hardware |
| [SPI Tilt Indicator](manual-registers/tilt_indicator/README.md) | SPI, signed sensor readings, timed sampling, hysteresis, and communication timeouts | Left/right LED feedback with startup validation; hardware behavior and simulated failure paths manually tested |
| [ADC/PWM LED Dimmer](cmsis/adc_pwm_dimmer/README.md) | CMSIS registers, ADC, PWM, I²C, and LCD feedback | Knob-controlled LED with percentage display tested on hardware; review refinements awaiting regression test |
| [UART Command Console](cmsis/uart_command_console/README.md) | USART2 interrupts, circular buffering, command parsing, and GPIO control | Four-LED commands and regression checks passed on hardware; overrun/queue-full recovery tested, framing/noise recovery simulated |
| [Breadboard Traffic Light](cmsis/breadboard_traffic_light/README.md) | External GPIO wiring, SysTick, state machines, debouncing, and elapsed-time checks | Traffic sequence and pedestrian indication tested on hardware; initial-red timer wrap verified |
| [Breadboard Traffic Light — HAL Comparison](hal/breadboard_traffic_light_hal/README.md) | CubeMX, HAL GPIO, HAL tick timing, state machines, and debouncing | Normal and pedestrian behavior manually tested; initial and requested red timing across wrap verified, with coverage limits documented |
| [Reusable Peripheral Modules — CMSIS](cmsis/peripheral_modules/README.md) | Public interfaces, private state, validated GPIO control, and timebase-independent debounce | CMSIS LED/button milestone tested, including press debounce across wrap; HAL counterpart planned |

Each project README contains its hardware setup, implementation details, run instructions, learning notes, and verification results. New projects will be added here as the collection grows.

## Next milestones

- [x] Build and verify a debounced button-controlled LED
- [x] Build and verify a reaction timer with varying delays and best-time tracking
- [ ] Add demonstration photos or videos to the project documentation
- [ ] Extend the LED controller with short/long presses and multiple patterns
- [x] Build and verify an SPI tilt indicator with hysteresis and communication timeouts
- [x] Build an ADC/PWM dimmer with LCD percentage feedback
- [ ] Retest the dimmer after review refinements
- [x] Build and verify a UART console with interrupt-driven reception and validated LED commands
- [x] Build a breadboard traffic light with external LEDs and a pedestrian button
- [x] Build a separate HAL traffic-light comparison using the same hardware and behavior
- [x] Build and verify CMSIS LED and debounced-button modules
- [ ] Build a HAL counterpart with matching module interfaces and repeat verification
- [ ] Add ADC timeout handling

## Learning approach

These are learning projects, developed incrementally with guided review. I write the application code, work through the register settings and design decisions, and test each milestone on the board.

The goal is to understand and explain the implementation, including mistakes corrected along the way. Completed features, manual verification, limitations, and future ideas are documented separately as each project evolves.

## Opening projects after the folder reorganization

The projects now live one level deeper, under their implementation category. Existing CubeIDE workspace entries may still point to their old locations.

For an existing workspace, remove each stale project entry from Project Explorer **without selecting “Delete project contents on disk”**, then use **File → Import → General → Existing Projects into Workspace** to select the new project directory. Leave **Copy projects into workspace** unchecked. Alternatively, import the projects from their new locations into a fresh workspace. Preserve the existing project names.

Run **Project → Clean**, then rebuild the projects you intend to use so generated build files reflect the new locations. Open the HAL `.ioc` from its new `hal/breadboard_traffic_light_hal` folder for future CubeMX edits. Source files are unchanged by this reorganization; rebuild and hardware behavior after the move have not yet been verified.
