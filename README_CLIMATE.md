# Bryant HVAC — Climate control policy

The ESPHome climate entity presents a simple Home Assistant interface while leaving actual heating/cooling demand, staging, and safety decisions to the Bryant thermostat. **The ESP device's control mode is distinct from the thermostat's selected HVAC mode.** The observed thermostat mode is read from `3B02`: `0x00` = HEAT, `0x01` = COOL, `0x05` = OFF. Writing that mode is not implemented.

## Modes

| Climate mode | Thermostat condition | ESP device behavior | Setpoint handling |
|---|---|---|---|
| **OFF** | Thermostat reports `0x05` | Observe only; no thermostat-mode writes | No setpoint writes |
| **AUTO** | Thermostat reports heat or cool | Observe thermostat; do not claim an existing manual HOLD | In thermostat HEAT, a heat-setpoint request can create the thermostat's default temporary override (observed ~2 hours). Cool override is not implemented |
| **HEAT** | Thermostat selected HEAT and ESP-owned permanent HOLD | Maintain control of heat setpoint while HOLD remains present | `set_heat()` updates setpoint while preserving permanent HOLD |
| **COOL** | Reserved | Not enabled until cool setpoint writing and validation are complete | Not implemented |

**AUTO** means *thermostat-managed*, not a claim that the thermostat is in an automatic heat/cool changeover mode. The thermostat alone decides whether equipment actually heats, runs the fan, or changes stages; `hvac_action` is not yet inferred from bus data.

## State transitions and ownership

- **Startup:** Start with no ESP-owned HOLD and no restored control. After a valid thermostat-mode reading, show OFF if the thermostat is off, or AUTO if it is in heat/cool. Never silently adopt an existing manual HOLD.
- **AUTO → HEAT:** Requires an explicit Home Assistant action (or automation), with the thermostat already in heating mode. Request permanent HOLD, then verify it. Do not take over a pre-existing manually selected HOLD.
- **HEAT → AUTO:** On explicit release, clear HOLD only if this ESP session established it. Do not clear a HOLD owned by an operator.
- **Manual HOLD removal:** If the thermostat leaves permanent HOLD while the ESP is in HEAT, relinquish control and return to AUTO; do **not** automatically reassert HOLD.
- **Manual HOLD enabled in AUTO:** Remain in AUTO. An explicit mode change is still required to grant the ESP control.
- **AUTO temporary override:** When the thermostat is heating and no manual permanent HOLD is active, a heat setpoint request uses `bryant_set.set_heat()`. The thermostat has been observed to create a default ~120-minute override. The duration is not explicitly written by this command. A separate `number.template` setpoint input can be used where Home Assistant hides the climate temperature slider in AUTO.
- **Thermostat OFF:** Report OFF when `3B02` indicates `0x05`. Selecting OFF in Home Assistant cannot be assumed to turn the thermostat off; a verified thermostat-mode write is not available.
- **COOL:** Control requests are reserved/rejected until the required writer and state policy have been verified.

## Transaction behavior

`bryant_set` uses a fresh `3B03` read before changes, skips writes when the requested state is already satisfied, and verifies a write by reading back. The heat-setpoint change flag is `0x04`; permanent HOLD changes use flag `0x02` and the zone 1 HOLD bit. The writer's `3B0E` acknowledgment responder is restricted to the configured register/address and a short window following a write; this is **not** a general-purpose reply to arbitrary bus traffic.

HOLD ownership is volatile local state, not a durable identity recorded by the thermostat. A matching HOLD observation during a pending request is not cryptographic or transaction-ID proof of ownership. No automatic write retries are performed. A setpoint no-op will not force a fresh timed override if the requested temperature already matches.

## Diagnostics and limitations

- Polling of `3B02`/`3B03` determines how quickly manual thermostat changes are reflected in the climate entity (example interval: **50 seconds**).
- Keep raw fields, framer synchronization counters, transaction status, and unverified equipment-stage indicators in Home Assistant diagnostics until validated.
- Useful supplementary entities include thermostat-selected mode, indoor/outdoor temperature, humidity, HOLD/override state, and remaining override minutes. Actual fan operation, heating action, heat-pump stage, gas stage, and defrost should be added only after confirming their protocol mappings.
- The current protocol work supports **heat setpoint and permanent HOLD**. Cool-setpoint writes and thermostat operating-mode writes are not verified or implemented.
- Temporary override duration is thermostat-defined in the tested behavior. AUTO setpoint changes during a manual permanent HOLD should be blocked rather than silently changing the operator's HOLD.
- A successful write/readback does not prove electrical bus reliability or universal compatibility. The responder and sync diagnostics remain experimental.

See [README.md](README.md) for scope, testing limitations, references, and disclaimer.
