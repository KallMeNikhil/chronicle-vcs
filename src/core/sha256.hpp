#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/id.hpp"

namespace chr {

class Sha256 {
 public:
  Sha256();
  void update(const void* data, size_t len);
  void update(const std::string& s) { update(s.data(), s.size()); }
  Id finish();

 private:
  void block(const uint8_t* p);
  uint32_t h_[8];
  uint8_t buf_[64];
  size_t bufLen_ = 0;
  uint64_t totalLen_ = 0;
};

inline Id sha256(const void* data, size_t len) {
  Sha256 h;
  h.update(data, len);
  return h.finish();
}

inline Id sha256(const std::string& s) { return sha256(s.data(), s.size()); }

}
