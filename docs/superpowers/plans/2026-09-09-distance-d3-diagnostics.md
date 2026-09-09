# D3 distance display correction and observability

User priority: resolve distance display first; defer unstable radio link. Modify and build only the main ECU. Keep the original Keil project/target so the user need not select another diagnostic project.

## Reproduced defect

After a complete numeric line, a gap of at least 500 ms incorrectly forced the parser back into its invalid-line state. Repeated one-second samples therefore never replaced the stale distance. A real C regression test expecting `420` from the next clean line failed with dashes before the fix.

Only partial/invalid lines are now reset across a long byte gap. Clean line boundaries remain ready to accept a new sample. Display freshness is still 500 ms, independent of parser synchronization.

## Main display

Normal readings keep the nine-character `D:  356mm` field. Unavailable readings now distinguish `NO RX`, `WAIT`, `ZERO`, `FORMAT`, `RX ERR`, and `STALE`. Empty lines cannot renew old positive or zero samples. The top-right `D3` mark identifies this normal-reception build without replacing the date/group number.

## Verification

- Slow-frame rejection and stale-zero-with-empty-lines were reproduced with failing tests before fixes.
- Main-only host suite: 850 passing checks; the bounded main-loop task and OLED bus/glyph/neighbor-preservation tests also pass.
- Main ECU Keil rebuild: 0 errors, 0 warnings; Code=19292, RO-data=1880, RW-data=156, ZI-data=2340.
- Independent read-only review found no actionable issues in the slow-frame fix, diagnostic states, or D3 placement.
- Current outputs: `Project/Output/can.hex`, `Project/Output/main_ecu_distance.hex`, and `Project/Output/main_ecu_distance_v3.hex`.

Pending physical observation: after downloading the original main ECU project and resetting, confirm `D3` and report the actual distance field. This fix does not prove the attached ranging ECU uses a slow output period, nor that serial bytes are present at PA10; the diagnostic status distinguishes those unresolved cases.

## Received hardware observation

User reports `NO RX` and confirms an added wire from ranging ECU PA9/TX to main ECU PA10/RX with common ground. Source inspection found no subsequent reconfiguration of PA10; the main image links the actual USART1 interrupt handler and calls RXNE/NVIC enable. `NO RX` means no events reached the parser, which still cannot distinguish absent electrical data from a receiver/main-loop problem. Host enumeration exposes Bluetooth COM5/COM6 only, so the inter-ECU UART stream is not directly available to the agent as a serial port. Next required evidence: observe ranging PA9 at 115200 8N1, without modifying the ranging firmware, and report whether complete numeric lines are present.
