# Bryant parser v2 compile fix

Replace **both** `components/bryant_framer/` and `components/bryant_parser/` with the directories in this archive. The supplied `hvac_control.yaml` is unchanged.

- `FrameView` now has one definition, in `bryant_frame.h`. `FrameListener` is defined alongside it.
- The framer constructs the complete 12-field `FrameView`, including both bus-address bytes, PID, extension, payload pointer, and frame size.
- The parser uses the actual `dest` and `fid` member names.
- Headers include specific ESPHome headers instead of `esphome.h` to avoid generated umbrella-header include cycles.
- Conditional debug/verbose log statements have braces, avoiding empty-body warnings when compile-time logging removes the macro.
- The GCC Windows precompiled-header message is a build-system informational message, not a component warning.

No firmware compile was run here.
