# Distance / radio regression investigation

Observed on hardware: group/date visible, distance always dashes, motor and steering do not respond to remote commands. No disconnect test has been reported. Verify only the main ECU, per user instruction.

## Confirmed software findings

1. The old OLED_ShowString renders each character through 22 separate I2C transactions. A test executing the real bitbang driver captured 198 transactions / 594 bytes for the nine-character distance field. New page bursts produce the same pixels with 8 transactions / 166 bytes and clip at column 16.
2. The main distance helper formerly drained until the producer was empty. An adversarial producer test consumed 1001 entries in one invocation. The helper now consumes at most 32 before yielding.
3. Radio polling and drive/stop decisions now precede distance and wheel-speed display work. Sampling and command timeout remain active; the 100 ms radio watchdog has not been relaxed.

Tests were observed failing before the changes and passing afterward. Main ECU rebuild: 0 errors, 0 warnings. No other ECU role was built for this investigation.

## Unresolved hardware cause

The unchanged-dashes case does not repeatedly redraw the distance field, so excess distance drawing alone does not explain the observed complete control loss. Read-only review and ARM disassembly confirmed the actual USART1 vector, saved-PRIMASK restoration, bounded ISR, and SR-then-DR error clearing; no definite UART freeze was identified. No confirmed hardware root cause is claimed.

Two main ECU images are prepared from the same V2 source:

- `Project/Output/main_ecu_distance_v2.hex`: normal distance reception.
- `Project/Output/main_ecu_uart_off.hex`: diagnostic copy omits USART_DistanceRx_Enable and labels the field UART OFF. Production source remains the normal version.

Pending: compare remote behavior between these two images with identical wiring. If only UART OFF works, focus on live USART activity/interrupts. If both work, the first-version regression no longer reproduces with V2. If neither works, investigate main-loop startup and valid radio packet reception before changing the distance protocol. A comparison only against V1 cannot isolate the UART interrupt because V2 also contains display/scheduling changes.
