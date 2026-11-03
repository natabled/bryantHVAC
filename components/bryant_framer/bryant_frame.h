#pragma once

#include <cstdint>
#include <cstddef>

namespace bryant_framer {

// Phase-1 frame shape matching the project spec. Payload/frame data is raw;
// device interpretation is deliberately left for the future parser component.
struct FrameView {
  uint32_t fid;       // monotonically increasing valid frame id
  uint32_t rcv_ms;    // ESP monotonic millis() at complete-frame detection
  uint8_t dest;
  uint8_t dest_bus;
  uint8_t src;
  uint8_t src_bus;
  uint8_t length;     // payload length from byte 4
  uint8_t pid;        // byte 5
  uint8_t extension;  // byte 6
  uint8_t function;   // byte 7
  const uint8_t *payload;
  size_t frame_size;
};

}  // namespace bryant_framer
