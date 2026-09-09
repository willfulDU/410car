# Main ECU Distance Display Implementation Plan

> Execute inline in the current user-selected `13.52测距` branch. The user approved the proposed UART distance display on 2026-09-08.

**Goal:** Display the existing ranging ECU's millimetre readings on OLED row 2, column 6, by updating only the vehicle main ECU firmware.

**Architecture:** USART1 RXNE interrupts enqueue bytes with reception timestamps. Main-loop code parses complete decimal lines and formats a fixed-width field. Keep the current ranging and steering ECU programs, UART baud rate and wiring. Receive continuously, redraw changed text at most every 200 ms, and replace readings at least 500 ms old with dashes.

**Tech Stack:** STM32F103C8, Keil ARMCC 5, STM32 standard peripheral library, C, MSVC host tests.

## File boundaries and acceptance

- `User/distance_display/distance_display.h/.c`: hardware-independent parsing, validity, receive-gap handling, timestamp wraparound and `D:  356mm` / `D:-----mm` formatting. Zero indicates the existing sensor driver's failure result. Accept 1–5 decimal digits through 65535; discard text and malformed lines. Synchronize at a line ending on startup and after lost bytes.
- `User/usart/bsp_usart.c/.h`: main-role-only 128-entry interrupt FIFO. Capture arrival times, clear USART SR/DR errors, and report lost bytes as a discontinuity. On overflow flush queued data and discard the damaged line. Preserve original source encoding and non-main behavior.
- `User/main.c`: main-role initialization and a 200 ms changed-field display task, executed before radio-related early returns. Retain row 1 date/group number, row 3 control state and row 4 wheel speed. Initialize the distance field to dashes.
- `Project/RVMDK（uv5）/Fire_F103C8.uvprojx`: register the new module plus the existing wheel-speed/differential modules omitted by the baseline project. Baseline Keil build reports `cannot open source input file wheel_speed.h`.
- `tests/distance_display/`: executable C tests of real parser and UART code with peripheral stubs; PowerShell runner uses local MSVC.
- `README.md`: wiring, field meaning, refresh/timeout, build and stationary bench checks.

## Execution

- [x] Write host tests first and confirm missing feature is detected. Cover LF/CRLF, split lines, debug text, zero, overlong/out-of-range values, startup resynchronization, timeout at exactly 500 ms, recovery with unchanged distance, counter wrap, FIFO wrap/overflow and USART receive errors.
- [x] Implement and run the parser/formatter and UART tests. Formatting must always occupy nine characters and a terminator, preventing leftover digits.
- [x] Integrate main-loop reception and display. Use the byte reception timestamp, not dequeue time, for freshness. Keep distance updates outside the radio-valid branch. Do not call the sensor driver from the main ECU.
- [x] Repair the evidenced project registration omission, add distance module, then rebuild the main ECU using local Keil. User subsequently narrowed verification to the main ECU; no further checks of other roles are required.
- [x] Review changes, run affected checks, document result and remaining physical verification. Leave changes reviewable on the current branch; do not flash hardware automatically.

## Verification result

- Main ECU Keil full rebuild: 0 errors, 0 warnings. Code 18788 bytes, RO-data 1880 bytes, RW-data 156 bytes, ZI-data 2340 bytes.
- MSVC host suite: 841 passing checks covering the actual C parser/formatter and USART FIFO/error handling. Most repeated checks exercise FIFO wraparound.
- Independent read-only code review: no actionable findings. `git diff --check` passed.
- Main ECU HEX: `Project/Output/can.hex`; identical delivery copy: `Project/Output/main_ecu_distance.hex`.
- No hardware flash or bench validation was performed.

## Bench verification

Measure a stationary target at several distances; verify millimetres appear in the second-row field. Disconnect the ranging UART and confirm dashes after 500 ms plus at most one 200 ms display interval; reconnect and confirm recovery even when the distance equals the previous value. With the steering controller powered off but the vehicle receiver module connected, verify distance continues updating. Existing pure-number protocol does not convey VL53L0X RangeStatus or provide a checksum, so it cannot detect every physical measurement error or digit substitution.
