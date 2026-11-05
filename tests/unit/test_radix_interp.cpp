#include <algorithm>
#include <random>

#include "core/sha256.hpp"
#include "ds/interpsearch.hpp"
#include "ds/radixsort.hpp"
#include "framework.hpp"

using namespace chr;

static Id randomId(std::mt19937_64& rng) {
  Id id;
  for (int i = 0; i < 32; i += 8) {
    uint64_t r = rng();
    for (int k = 0; k < 8; ++k) id.b[i + k] = static_cast<uint8_t>(r >> (8 * k));
  }
  return id;
}

static bool lessId(const IdEntry& a, const IdEntry& b) { return a.id < b.id; }

TEST(radix_sort_equals_stable_sort) {
  std::mt19937_64 rng(42);
  for (size_t n : {0u, 1u, 2u, 17u, 1000u, 20000u}) {
    std::vector<IdEntry> v(n);
    for (size_t i = 0; i < n; ++i) {
      v[i].id = (i % 3 == 2) ? v[i / 2].id : randomId(rng);
      v[i].offset = i;
    }
    std::vector<IdEntry> expect = v;
    std::stable_sort(expect.begin(), expect.end(), lessId);
    radixSortIds(v);
    bool same = true;
    for (size_t i = 0; i < n; ++i)
      if (!(v[i].id == expect[i].id) || v[i].offset != expect[i].offset) same = false;
    CHECK(same);
  }
}

static void checkSearch(std::vector<IdEntry>& v, std::mt19937_64& rng) {
  radixSortIds(v);
  int mismatches = 0;
  for (size_t i = 0; i < v.size(); ++i) {
    if (interpolationSearch(v, v[i].id) < 0) ++mismatches;
  }
  for (int i = 0; i < 20000; ++i) {
    Id probe = (i % 2) ? randomId(rng) : v[rng() % v.size()].id;
    if (i % 4 == 1) probe.b[31] ^= 1;
    size_t want = static_cast<size_t>(std::lower_bound(v.begin(), v.end(), probe,
                                                       [](const IdEntry& e, const Id& id) { return e.id < id; }) -
                                      v.begin());
    if (interpolationLowerBound(v, probe) != want) ++mismatches;
  }
  CHECK_EQ(mismatches, 0);
}

TEST(interpolation_matches_lower_bound_uniform) {
  std::mt19937_64 rng(5);
  std::vector<IdEntry> v(100000);
  for (auto& e : v) e.id = randomId(rng);
  checkSearch(v, rng);
}

TEST(interpolation_matches_lower_bound_clustered) {
  std::mt19937_64 rng(9);
  std::vector<IdEntry> v(100000);
  for (size_t i = 0; i < v.size(); ++i) {
    v[i].id = randomId(rng);
    if (i % 100 != 0) {
      for (int k = 0; k < 6; ++k) v[i].id.b[k] = 0;
    } else {
      v[i].id.b[0] = 0xff;
    }
    if (i % 1000 == 0) for (int k = 0; k < 8; ++k) v[i].id.b[k] = 0x80;
  }
  checkSearch(v, rng);
}

TEST(interpolation_uniform_probe_count_small) {
  std::mt19937_64 rng(11);
  std::vector<IdEntry> v(100000);
  for (auto& e : v) e.id = randomId(rng);
  radixSortIds(v);
  long total = 0;
  for (int i = 0; i < 1000; ++i) {
    int probes = 0;
    interpolationLowerBound(v, v[rng() % v.size()].id, &probes);
    total += probes;
  }
  CHECK(total / 1000.0 < 8.0);
}

TEST(interpolation_empty_and_tiny) {
  std::vector<IdEntry> v;
  Id x = sha256("x");
  CHECK_EQ(interpolationLowerBound(v, x), 0u);
  CHECK_EQ(interpolationSearch(v, x), -1L);
  v.push_back({x, 7});
  CHECK_EQ(interpolationSearch(v, x), 0L);
}
