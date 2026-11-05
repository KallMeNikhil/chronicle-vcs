#pragma once
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>

#include "core/strutil.hpp"

namespace chr {

struct Id {
  uint8_t b[32] = {};

  std::string hex() const { return toHex(b, 32); }
  std::string shortHex(size_t n = 8) const { return hex().substr(0, n); }
  std::string raw() const { return std::string(reinterpret_cast<const char*>(b), 32); }
  bool isZero() const {
    for (uint8_t x : b)
      if (x) return false;
    return true;
  }
  uint64_t prefix64() const {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | b[i];
    return v;
  }

  static bool fromHex(const std::string& h, Id* out) {
    if (h.size() != 64) return false;
    for (int i = 0; i < 32; ++i) {
      int hi = hexVal(h[2 * i]), lo = hexVal(h[2 * i + 1]);
      if (hi < 0 || lo < 0) return false;
      out->b[i] = static_cast<uint8_t>(hi * 16 + lo);
    }
    return true;
  }
  static Id fromRaw(const char* p) {
    Id id;
    std::memcpy(id.b, p, 32);
    return id;
  }

  bool operator==(const Id& o) const { return std::memcmp(b, o.b, 32) == 0; }
  bool operator!=(const Id& o) const { return !(*this == o); }
  bool operator<(const Id& o) const { return std::memcmp(b, o.b, 32) < 0; }
};

struct IdHash {
  size_t operator()(const Id& id) const { return static_cast<size_t>(id.prefix64()); }
};

inline std::ostream& operator<<(std::ostream& o, const Id& id) { return o << id.hex(); }

}
