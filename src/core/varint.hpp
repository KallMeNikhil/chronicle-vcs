#pragma once
#include <cstdint>
#include <string>

namespace chr {

inline void putVarint(std::string& out, uint64_t v) {
  while (v >= 0x80) {
    out += static_cast<char>((v & 0x7F) | 0x80);
    v >>= 7;
  }
  out += static_cast<char>(v);
}

inline bool getVarint(const std::string& in, size_t& pos, uint64_t* out) {
  uint64_t v = 0;
  for (int shift = 0; shift < 64; shift += 7) {
    if (pos >= in.size()) return false;
    uint8_t b = static_cast<uint8_t>(in[pos++]);
    v |= static_cast<uint64_t>(b & 0x7F) << shift;
    if (!(b & 0x80)) {
      *out = v;
      return true;
    }
  }
  return false;
}

}
