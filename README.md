# Bryant HVAC local ESPHome component — Phase 1 framer

This phase provides only UART draining, recognizable-header resynchronization,
length-based whole-frame consumption, Carrier CRC validation, counters, and
frame/error logging. The parser, climate component, and writer are intentionally
not present.

## Local component setup

Copy `components/bryant_framer` into the ESPHome configuration directory's
`components/` folder, then use `hvac_control.yaml` as a configuration template.
Adjust board and UART pins to match the actual RS-485 interface. `drain_interval`
is 7 ms (configurable 5–10 ms); the ring is fixed storage of 2048 bytes, with
configurable logical capacity up to that fixed limit.

## Framing assumptions sourced from project files

- Header is eight bytes: destination, destination bus, source, source bus,
  payload length, PID, extension, function.
- Bus byte is `0x01`; recognized address values are `20, 21, 40, 41, 50`;
  special `F1 F1 20 01` header is included as in the known-working YAML.
- Frame total size is `8 + payload_length + 2` (two CRC bytes low byte first).
- CRC calculation is copied algorithmically from `rs485-hvac-v1-926.yaml`:
  init `0x0000`, input bit reversal, polynomial `0x8005`, final reversal.

A CRC failure slides by one byte to enable recovery. A CRC-valid frame is consumed
as a whole, leaving the next frame at the ring head. Bytes preceding a recognized
header are counted as resync discards. The component currently logs valid frames
and errors only; it does not expose parser callbacks in this phase.
