#include <algorithm>
#include <random>
#include <utility>

#include "ds/bplustree.hpp"
#include "framework.hpp"

using namespace chr;

using Key = std::pair<long long, int>;

static void runRandom(int m, int n, unsigned seed) {
  std::mt19937 rng(seed);
  BPlusTree<Key, int> t(m);
  std::vector<Key> oracle;
  bool invariantsOk = true;
  for (int i = 0; i < n; ++i) {
    Key k{static_cast<long long>(rng() % 5000), i};
    t.insert(k, i);
    oracle.insert(std::lower_bound(oracle.begin(), oracle.end(), k), k);
    if (!t.checkInvariants()) invariantsOk = false;
  }
  CHECK(invariantsOk);
  CHECK_EQ(t.size(), oracle.size());
  int bad = 0;
  for (int q = 0; q < 500; ++q) {
    long long a = static_cast<long long>(rng() % 5200) - 100, b = a + static_cast<long long>(rng() % 400);
    Key lo{a, -1}, hi{b, 1 << 30};
    auto got = t.rangeScan(lo, hi);
    auto l = std::lower_bound(oracle.begin(), oracle.end(), lo);
    auto u = std::upper_bound(oracle.begin(), oracle.end(), hi);
    if (static_cast<long>(got.size()) != u - l) ++bad;
    for (size_t i = 0; i < got.size() && l + static_cast<long>(i) < u; ++i)
      if (got[i].first != *(l + static_cast<long>(i)) || got[i].second != got[i].first.second) ++bad;
    Key probe{a, static_cast<int>(rng() % n)};
    auto lb = t.lowerBound(probe);
    auto ol = std::lower_bound(oracle.begin(), oracle.end(), probe);
    if (lb.valid() != (ol != oracle.end()) || (lb.valid() && t.key(lb) != *ol)) ++bad;
    auto fl = t.floor(probe);
    auto of = std::upper_bound(oracle.begin(), oracle.end(), probe);
    bool hasFloor = of != oracle.begin();
    if (fl.valid() != hasFloor || (hasFloor && t.key(fl) != *(of - 1))) ++bad;
  }
  CHECK_EQ(bad, 0);
}

TEST(bplustree_m4_random_vs_oracle) { runRandom(4, 3000, 1); }
TEST(bplustree_m64_random_vs_oracle) { runRandom(64, 20000, 2); }

TEST(bplustree_bulk_load) {
  for (int m : {4, 5, 64}) {
    for (size_t n : {0u, 1u, 3u, 10u, 65u, 1000u, 50000u}) {
      std::vector<std::pair<Key, int>> v;
      for (size_t i = 0; i < n; ++i) v.push_back({Key{static_cast<long long>(i * 3), 0}, static_cast<int>(i)});
      BPlusTree<Key, int> t(m);
      t.bulkLoad(v);
      CHECK(t.checkInvariants());
      CHECK_EQ(t.size(), n);
      if (n > 0) {
        auto all = t.rangeScan(v.front().first, v.back().first);
        CHECK_EQ(all.size(), n);
        auto f = t.floor(Key{4, 0});
        CHECK(n < 2 || (f.valid() && t.key(f).first == 3));
        CHECK(!t.floor(Key{-1, 0}).valid());
      }
      for (size_t i = 0; i < 200; ++i) t.insert(Key{static_cast<long long>(i * 7 + 1), 1}, 0);
      CHECK(t.checkInvariants());
    }
  }
  BPlusTree<Key, int> big(64);
  std::vector<std::pair<Key, int>> v;
  for (int i = 0; i < 100000; ++i) v.push_back({Key{i, 0}, i});
  big.bulkLoad(v);
  CHECK(big.height() <= 4);
}
