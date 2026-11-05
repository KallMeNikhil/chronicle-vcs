#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

#include "ds/radixsort.hpp"

namespace chr {

__extension__ typedef unsigned __int128 chr_u128;

inline size_t interpolationLowerBound(const std::vector<IdEntry>& v, const Id& target,
                                      int* probes = nullptr) {
  size_t lo = 0, hi = v.size();
  const uint64_t t = target.prefix64();
  int n = 0;
  while (lo < hi && n < 24) {
    uint64_t kl = v[lo].id.prefix64();
    uint64_t kh = v[hi - 1].id.prefix64();
    if (t <= kl) break;
    if (t > kh) {
      lo = hi;
      break;
    }
    chr_u128 num = static_cast<chr_u128>(t - kl) * static_cast<chr_u128>(hi - 1 - lo);
    size_t pos = lo + static_cast<size_t>(num / (kh - kl));
    ++n;
    if (v[pos].id < target)
      lo = pos + 1;
    else
      hi = pos;
  }
  if (probes) *probes = n;
  auto it = std::lower_bound(v.begin() + static_cast<long>(lo), v.begin() + static_cast<long>(hi), target,
                             [](const IdEntry& e, const Id& id) { return e.id < id; });
  return static_cast<size_t>(it - v.begin());
}

inline long interpolationSearch(const std::vector<IdEntry>& v, const Id& target) {
  size_t i = interpolationLowerBound(v, target);
  if (i < v.size() && v[i].id == target) return static_cast<long>(i);
  return -1;
}

}
