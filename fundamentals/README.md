# Embedded C Fundamentals

Small, hands-on projects exploring how embedded software interacts with real hardware.

This collection documents my progress with register-level C programming on the STM32F407 Discovery board. Each project grows through practical milestones: understand the hardware, implement a feature, test it on the board, and explain what I learned.

## Project index

| Project | Focus | Current milestone |
| --- | --- | --- |
| [Button-Controlled LED Controller](button_led_controller/README.md) | GPIO, bit manipulation, SysTick, state tracking, and debouncing | One LED toggle per debounced press; manually tested on hardware |
| [Reaction Timer](reaction_timer/README.md) | State machines, elapsed-time measurement, pseudorandom delays, debouncing, and SWV output | Replayable game with early-press rejection and best-time tracking; manually tested on hardware |
| [SPI Tilt Indicator](tilt_indicator/README.md) | SPI, signed sensor readings, timed sampling, hysteresis, and communication timeouts | Left/right LED feedback with startup validation; hardware behavior and simulated failure paths manually tested |

Each project README contains its hardware setup, implementation details, run instructions, learning notes, and verification results. New projects will be added here as the collection grows.

## Next milestones

- [x] Build and verify a debounced button-controlled LED
- [x] Build and verify a reaction timer with varying delays and best-time tracking
- [ ] Add demonstration photos or videos to the project documentation
- [ ] Extend the LED controller with short/long presses and multiple patterns
- [x] Build and verify an SPI tilt indicator with hysteresis and communication timeouts
- [ ] Choose the next fundamentals project and document its requirements before coding

## Learning approach

These are learning projects, developed incrementally with guided review. I write the application code, work through the register settings and design decisions, and test each milestone on the board.

The goal is to understand and explain the implementation, including mistakes corrected along the way. Completed features, manual verification, limitations, and future ideas are documented separately as each project evolves.
