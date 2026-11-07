# Bryant HVAC climate refactor — test candidate

## HA naming
ESPHome friendly_name is `Basement`; climate name is `HVAC` => expected `climate.basement_hvac` (subject to existing HA entity-registry IDs). Other names use `HVAC ...` to produce `basement_hvac_...`.

If HA has existing entity registry entries, change/remove their IDs manually in HA; a firmware rename does not guarantee an existing entity ID changes.

## Policy
- Atom climate OFF: thermostat runs its schedule. If this Atom climate session established HOLD, send CLEAR HOLD once. Does NOT change thermostat selected HVAC mode to OFF.
- Atom climate HEAT: establish or adopt permanent HOLD; then apply requested heat setpoints. Thermostat remains responsible for actual heating demand.
- If operator removes HOLD, Atom climate falls to OFF without reasserting it.
- Atom reboot starts OFF. Does not clear an operator's pre-existing HOLD.
- HOLD enable has no automatic retries after a 20s confirmation deadline; no-op checks and verification are performed by bryant_set.
- Explicit diagnostic HOLD buttons and raw TX remain available.

## Firmware change
Only C++ change: expose `BryantSet::is_busy() const` in `bryant_set.h`, so the YAML automation does not enqueue overlapping transactions. No framer, parser, or writer changes.

## Caveats
- This is a test candidate; not compiled against ESPHome in this environment.
- 3B03 polling interval is 50 seconds; manual HOLD removal is detected on the next update, not immediately.
- Existing transaction status strings are diagnostic, not a durable transaction result API. A future implementation should expose transaction result/ownership state to improve release error reporting.
- Existing HA entity IDs may need registry renaming after deployment.
- No `hvac_action` is reported: heating equipment state is not yet verified.
