#pragma once
#include <cstdint>
#include <vector>

#include "core/id.hpp"

namespace chr {

struct IdEntry {
  Id id;
  uint64_t offset = 0;
};

inline void radixSortIds(std::vector<IdEntry>& v) {
  std::vector<IdEntry> tmp(v.size());
  for (int byte = 31; byte >= 0; --byte) {
    size_t count[257] = {0};
    for (const IdEntry& e : v) ++count[e.id.b[byte] + 1];
    bool trivial = false;
    for (int k = 1; k <= 256; ++k)
      if (count[k] == v.size()) trivial = true;
    if (trivial) continue;
    for (int k = 0; k < 256; ++k) count[k + 1] += count[k];
    for (const IdEntry& e : v) tmp[count[e.id.b[byte]]++] = e;
    v.swap(tmp);
  }
}

}
