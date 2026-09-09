# Three-ECU NRF24L01 Communication Design

**Status:** Approved for implementation

## Goal

Use one NRF24L01 on the vehicle ECU to receive independent wireless messages
from the steering ECU and the ranging ECU. The ranging ECU transmits once
every 20 ms. The vehicle ECU keeps the two data sources separate and does not
allow ranging data to directly drive an actuator.

## Topology

All nodes use the same RF channel. The vehicle NRF24L01 enables two receive
pipes with different five-byte addresses:

- Pipe 0 / `STEERING_ADDR`: steering ECU control frames.
- Pipe 1 / `RANGE_ADDR`: ranging ECU status frames.

The steering and ranging ECUs run in transmit mode. The vehicle ECU runs in
receive mode, accepts both pipes, and relies on the NRF24L01 automatic ACK and
retry mechanism. This avoids address switching and lets the hardware identify
the source of every received payload.

## Frames

### Steering frame

The existing eight-byte payload remains unchanged:

```text
0x01 | wheel high | wheel low | pedal | 0x02 | D/R/N | 0x03 | lamp
```

The vehicle accepts it only when its length is eight and all three marker
bytes match.

### Ranging frame

The ranging ECU sends this nine-byte payload every 20 ms:

```text
0xA5 | 0x5A | sequence | distance high | distance low |
suggested RPM high | suggested RPM low | status | CRC8
```

The vehicle accepts it only when its length, header, and CRC8 are valid. The
sequence field is available for diagnostics; the most recently valid frame is
the current ranging snapshot.

## Firmware Changes

- Replace the single `INIT_ADDR` configuration with explicit steering and
  ranging address constants.
- Configure both Pipe 0 and Pipe 1, including dynamic-payload support for both
  pipes.
- Add a non-blocking receive API that reports payload length and source pipe.
  Retain the current blocking API only for compatibility.
- Add pure protocol helpers for ranging-frame encode, decode, and CRC8 so the
  payload behavior can be host tested.
- In the ranging ECU branch, initialize the NRF24L01 transmit path and send a
  range frame every 20 ms. The first version reports the measured distance,
  marks the distance valid when nonzero, and leaves suggested RPM at zero.
- In the vehicle ECU branch, continuously poll the non-blocking API, validate
  each frame according to its source, and store separate steering and ranging
  snapshots with freshness timestamps. No motor, steering, or lighting behavior
  is added as part of this communication-only change.

## Failure Handling

- Invalid length, wrong pipe/frame combination, invalid marker bytes, and
  CRC failure are discarded without replacing the latest valid snapshot.
- A missing steering frame and a missing ranging frame are tracked
  independently. Later control work will use those freshness states for its
  stop/fail-safe rules.
- Send failure is handled by the existing NRF24L01 retry result; the next
  periodic frame is still sent.

## Verification

- Host tests cover CRC8, ranging-frame round trip, corrupt CRC rejection, and
  wrong-length rejection.
- Three Keil builds are checked: steering transmitter, vehicle dual-pipe
  receiver, and ranging transmitter.
- Bench verification confirms that steering messages arrive on Pipe 0 and
  ranging messages arrive on Pipe 1 while the ranging ECU transmits every
  20 ms.
