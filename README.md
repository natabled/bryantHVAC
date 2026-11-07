# Bryant HVAC — ESPHome RS-485 integration

An experimental ESPHome integration for observing a Bryant/Carrier communicating HVAC system and making limited thermostat setpoint and HOLD changes over its RS-485 bus. The goal is a useful Home Assistant climate interface **without replacing the thermostat's own schedule, equipment logic, or safety controls**. When the ESP device is not controlling a HOLD, the thermostat remains responsible for its program.

## What it does

Four custom ESPHome components provide the bus interface:

- **`bryant_framer`** receives UART data, identifies frames, validates CRC, resynchronizes after errors, and exposes diagnostics.
- **`bryant_parser`** matches messages and publishes decoded readings, including thermostat temperature, operating mode, setpoints, HOLD state, and selected diagnostic fields.
- **`bryant_writer`** polls selected registers, queues transmissions, and conditionally acknowledges thermostat `3B0E` notifications during a short post-write window.
- **`bryant_set`** performs fresh-read → conditional write → acknowledgment → readback verification for heat setpoint and permanent HOLD changes. Unnecessary writes are skipped when the requested state is already present.

The example configuration provides a Home Assistant climate entity with **OFF** (thermostat reports off), **AUTO** (thermostat operates normally; ESP observes and can request a temporary heating override), and **HEAT** (ESP manages a permanent thermostat HOLD and heat setpoint). **COOL** control is reserved for future work. These are *ESP control-policy states*, not direct commands to the furnace or heat pump. See [README_CLIMATE.md](README_CLIMATE.md) for the state machine and limitations.

Additional entities expose raw protocol fields, framer health, transaction status, and optional manual test controls. Unverified fields should remain diagnostic until validated against equipment behavior.

## Key findings

- Observed bus functions include `0x0B` READ, `0x0C` WRITE, and `0x06` successful response. Frames include addresses, length, function, payload, and CRC.
- Thermostat register **`3B02`** reports selected mode: `0x00` heat, `0x01` cool, `0x05` off on the tested system.
- Register **`3B03`** contains zone setpoints and HOLD state. Zone 1 heat setpoint and permanent HOLD enable/clear have been exercised through read-modify-write-verify transactions.
- Changing a heat setpoint with permanent HOLD active preserved HOLD in testing. Without permanent HOLD, a heat setpoint change produced a temporary override (observed default approximately **120 minutes**).
- Thermostat-to-ESP **`3B0E`** follow-up notifications were observed after writes. A narrowly scoped responder sends an acknowledgment; complete thermostat acceptance and collision-free bus operation have **not** been conclusively established.
- In observed `0420` broadcasts, full-frame bytes **17–18** encode outdoor temperature as a signed big-endian 16-bit value divided by 16; byte **21** carries indoor humidity. For the current parser implementation, these correspond to field offsets **11** and **15** respectively.

## Setup and testing

Use `hvac_control.yaml` as a configuration example, adjusting ESP32 board, UART pins, RS-485 transceiver wiring, network settings, addresses, and ESPHome secrets for your installation. The example references the custom components through ESPHome `external_components`. Do not assume the example configuration or register mapping is portable to other equipment.

**Testing is limited to an M5Stack Atom ESP32-based device connected to one Bryant communicating HVAC installation.** Observations are empirical, incomplete, and may differ by thermostat, firmware, system configuration, or equipment generation. This project is not a certified interface, and the protocol has not been fully characterized.

## References

- [Climate control state machine and operational notes](README_CLIMATE.md)
- [ESPHome documentation](https://esphome.io/)
- [Home Assistant climate integration](https://www.home-assistant.io/integrations/climate/)

## Disclaimer

**Use at your own risk.** This is an independent experimental project, not affiliated with or endorsed by Bryant, Carrier, or their affiliates. The authors and contributors make no warranty of correctness, compatibility, safety, or fitness for a particular purpose and accept no responsibility or liability for equipment damage, loss of heating or cooling, property damage, or other consequences arising from its use. HVAC control and shared-bus communication can have unintended effects. Preserve the manufacturer's safety controls and a reliable means of returning control to the thermostat.
