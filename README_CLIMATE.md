# Bryant HVAC climate state machine — v3.36 test candidate

## State ownership
Thermostat selected HVAC mode (`3B02`): 0=HEAT, 1=COOL, 5=OFF. This is independent of Atom climate policy. The Atom does not have a verified command to write thermostat operating mode.

| Climate display | Thermostat condition | Atom authority | Setpoint behavior |
|---|---|---|---|
| OFF | Thermostat reports 0x05 | None | No writes |
| AUTO | Thermostat reports 0x00/0x01 | Observer; no owned HOLD | If thermostat is in HEAT, target change sends `set_heat()` for default ~2h override; COOL target writing is not implemented |
| HEAT | Thermostat reports 0x00 and Atom-owned permanent HOLD | Heat target control | `set_heat()` preserves HOLD |
| COOL | Reserved, not activated | None yet | Requires verified cool setpoint transaction |

## State transitions
- Boot: Atom policy OFF, no ownership restored, no writes. After first valid `3B02`, OFF stays OFF for 0x05; 0x00/0x01 becomes AUTO. A pre-existing manual HOLD is never adopted.
- AUTO -> HEAT: explicit HA command while thermostat selected HEAT. If no existing manual HOLD, send `set_hold(1,true)`, verify and claim ownership. Existing manual HOLD: fail closed to AUTO (no takeover).
- HEAT -> AUTO: explicit HA command. Request `set_hold(1,false)` only if Atom owns the HOLD.
- HEAT -> AUTO: if a subsequent read detects manual HOLD removal, timed override, or thermostat switched from HEAT, relinquish without reassertion.
- AUTO + manual HOLD: remain AUTO. No adoption.
- AUTO + temperature change: only if thermostat is in HEAT, `set_heat(1,target)` and thermostat normally creates its default temporary override (~120 min). No implicit HOLD. AUTO setpoint writes are blocked when manual HOLD is active or HOLD state unknown. COOL override unsupported.
- Thermostat -> OFF (0x05): climate OFF; no claim that Atom switched the thermostat off.
- HA -> OFF while thermostat in HEAT/COOL: rejected/logged; thermostat mode-write protocol has not been established. Do not pretend this request succeeded.
- HA -> COOL: rejected/logged until a cool writer and mode policy are implemented.

## Limits and test points
- `bryant_set` has a verified heat/HOLD read-modify-write-verify engine; no verified cool writer or thermostat mode writer. No C++ changes in this package.
- AUTO temporary override behavior is established from previous writes *without* permanent HOLD, but validate with an explicit HA climate AUTO setpoint test.
- HOLD ownership is local volatile state, not proof of durable bus ownership. A manual HOLD appearing before Atom control remains manual.
- `set_heat()` no-op optimization compares temperature only. If the thermostat is already at the requested temperature but a timed override was desired, it will not force a new override.
- The 50-second polling interval determines detection latency. Transaction status is not a durable result API; verify failures in logs.
- A pending HOLD is treated as owned when a matching HOLD state is observed, not by a unique transaction identifier. Improve the C++ transaction result API before unattended operation.
- Climate `hvac_action` is intentionally not set; thermostat decides heating equipment operation.
- YAML is structurally edited but not ESPHome compiled or hardware tested.
