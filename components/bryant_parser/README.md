# Bryant parser v2.01 parity (receive-only)

## Layout
- `components/bryant_framer/`: CRC-validated UART frame assembly and callback to parser.
- `components/bryant_parser/`: YAML-driven message matching and numeric field decoding.
- `hvac_control.yaml`: ESPHome config, numeric sensor and text sensor declarations, messages, fields.

## Adding a sensor for an existing message
1. Under top-level `sensor:` add a `platform: template` sensor with a unique `id`, name, unit, and accuracy.
2. Under `bryant_parser.fields:` add `name`, `message` (a name in `messages`), `sensor_id` (same as sensor ID), `offset` (zero-based payload byte offset), and optional `width` (8/16/24/32), `signed`, `endian`, `multiplier`, `add`, `mask`, `shift`, `bit_rules`, `default`.
3. Compile and flash; no edits to parser `__init__.py`, `.h`, or `.cpp` are required.

## Adding a new message/register
1. Under `bryant_parser.messages:` add a unique `name`, `match` containing `dst`, `src`, `function`, and `register`, plus `payload_min`.
2. Add sensor declarations and corresponding field definitions as above.
3. Use `log_matches: true` to see matches and decoded values. `log_unmatched: true` logs unmatched frames at VERBOSE level; set logger level to VERBOSE to see them. The framer's `log_valid_frames` can be turned off to reduce raw logs.

## Offsets
CRC-validated raw frame = 8-byte header + payload + 2-byte CRC. `offset: 0` means raw frame byte 8, `offset: 3` means raw frame byte 11. Register bytes are payload offsets 1 and 2, not stripped. Offset is in bytes; width is in bits.

## Date/time
`thermostat_time` (register 0202) and `thermostat_date` (0203) publish six numeric template sensors, preserving the v2.01 2-digit year. The parser also publishes a combined ISO local date-time string `YYYY-MM-DDTHH:MM:SS` via `text_sensor.hvac_datetime`. Year `00..99` is interpreted as 2000..2099 for this combined string. The source frames do not include a UTC offset, so the combined value is intentionally a **text sensor**, not a Home Assistant timestamp sensor with potentially misleading timezone semantics. To make a HA-native timestamp, supply a verified timezone/offset and convert accordingly.

## Notes
- Receive-only: no writes, polls, or UART TX commands were added.
- Parser requires the modified framer in this bundle; don't mix it with the older Git framer without listener support.
- Match checks dst/src/function/register, but not bus address, PID, or extension.
- `bit_rules` are evaluated in order; the first mask fully present wins, then `default`.
- `signed` uses two's-complement; do not combine with non-default mask/shift in this initial version.
- Unmatched frames remain in framer logs; schema additions are data-only for supported numeric formats.
