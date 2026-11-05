#include <algorithm>
#include <map>
#include <random>
#include <unordered_map>
#include <unordered_set>

#include "bench_util.hpp"
#include "core/sha256.hpp"
#include "ds/bplustree.hpp"
#include "ds/interpsearch.hpp"
#include "ds/radixsort.hpp"
#include "ds/trie.hpp"

using namespace chr;

namespace bench {

class ProbeSet {
 public:
  explicit ProbeSet(size_t expected = 16) {
    size_t cap = 16;
    while (cap < expected * 2) cap <<= 1;
    slots_.assign(cap, 0);
    used_.assign(cap, 0);
  }
  bool insert(uint64_t k) {
    if ((n_ + 1) * 2 > slots_.size()) grow();
    size_t i = mix(k) & (slots_.size() - 1);
    while (used_[i]) {
      if (slots_[i] == k) return false;
      i = (i + 1) & (slots_.size() - 1);
    }
    used_[i] = 1;
    slots_[i] = k;
    ++n_;
    return true;
  }
  bool contains(uint64_t k) const {
    size_t i = mix(k) & (slots_.size() - 1);
    while (used_[i]) {
      if (slots_[i] == k) return true;
      i = (i + 1) & (slots_.size() - 1);
    }
    return false;
  }

 private:
  static size_t mix(uint64_t x) {
    x ^= x >> 30, x *= 0xbf58476d1ce4e5b9ULL, x ^= x >> 27, x *= 0x94d049bb133111ebULL;
    return static_cast<size_t>(x ^ (x >> 31));
  }
  void grow() {
    std::vector<uint64_t> old = slots_;
    std::vector<char> oldUsed = used_;
    slots_.assign(old.size() * 2, 0);
    used_.assign(old.size() * 2, 0);
    n_ = 0;
    for (size_t i = 0; i < old.size(); ++i)
      if (oldUsed[i]) insert(old[i]);
  }
  std::vector<uint64_t> slots_;
  std::vector<char> used_;
  size_t n_ = 0;
};

static std::vector<Id> makeIds(size_t n, uint64_t seed) {
  std::vector<Id> v(n);
  for (size_t i = 0; i < n; ++i) v[i] = sha256(std::to_string(seed) + ":" + std::to_string(i));
  return v;
}

static void b1() {
  Series& chained = add("B1", "lookup: chained hash (unordered_map)", "1 (n lookups x O(1))");
  Series& probing = add("B1", "lookup: linear-probing hash", "1 (n lookups x O(1))");
  Series& binary = add("B1", "lookup: sorted array + lower_bound", "1+ (n x O(log n))");
  Series& interp = add("B1", "lookup: sorted array + interpolation", "1+ (n x O(log log n))");
  for (size_t n = 1 << 12; n <= (1 << 20); n <<= 2) {
    std::vector<Id> ids = makeIds(n, 1);
    std::vector<Id> queries = ids;
    std::shuffle(queries.begin(), queries.end(), std::mt19937(3));
    std::unordered_map<Id, uint64_t, IdHash> map;
    ProbeSet probe(n);
    std::vector<IdEntry> sorted(n);
    for (size_t i = 0; i < n; ++i) {
      map[ids[i]] = i;
      probe.insert(ids[i].prefix64());
      sorted[i] = {ids[i], i};
    }
    radixSortIds(sorted);
    chained.pts.push_back({double(n), timeIt([&] {
                             size_t hit = 0;
                             for (const Id& q : queries) hit += map.count(q);
                             keep(hit);
                           })});
    probing.pts.push_back({double(n), timeIt([&] {
                             size_t hit = 0;
                             for (const Id& q : queries) hit += probe.contains(q.prefix64());
                             keep(hit);
                           })});
    binary.pts.push_back({double(n), timeIt([&] {
                            size_t hit = 0;
                            for (const Id& q : queries)
                              hit += static_cast<size_t>(
                                  std::lower_bound(sorted.begin(), sorted.end(), q,
                                                   [](const IdEntry& e, const Id& id) { return e.id < id; }) -
                                  sorted.begin());
                            keep(hit);
                          })});
    interp.pts.push_back({double(n), timeIt([&] {
                            size_t hit = 0;
                            for (const Id& q : queries) hit += interpolationLowerBound(sorted, q);
                            keep(hit);
                          })});
  }
}

static std::vector<std::vector<int>> randomDag(size_t n, unsigned seed) {
  std::mt19937 rng(seed);
  std::vector<std::vector<int>> parents(n);
  for (size_t i = 1; i < n; ++i) {
    int span = static_cast<int>(std::min<size_t>(i, 50));
    parents[i].push_back(static_cast<int>(i) - 1 - static_cast<int>(rng() % static_cast<unsigned>(span)));
    if (rng() % 5 == 0) parents[i].push_back(static_cast<int>(i) - 1 - static_cast<int>(rng() % static_cast<unsigned>(span)));
  }
  return parents;
}

static void b2() {
  Series& chain = add("B2", "DAG traversal visited: unordered_set", "1 (O(V+E))");
  Series& probe = add("B2", "DAG traversal visited: linear probing", "1 (O(V+E))");
  for (size_t n = 1 << 12; n <= (1 << 20); n <<= 2) {
    std::vector<std::vector<int>> parents = randomDag(n, 7);
    auto run = [&](auto& visited, auto insert) {
      std::vector<int> stack = {static_cast<int>(n - 1)};
      size_t seen = 0;
      while (!stack.empty()) {
        int x = stack.back();
        stack.pop_back();
        if (!insert(visited, x)) continue;
        ++seen;
        for (int p : parents[static_cast<size_t>(x)]) stack.push_back(p);
      }
      keep(seen);
    };
    chain.pts.push_back({double(n), timeIt([&] {
                           std::unordered_set<uint64_t> v;
                           run(v, [](std::unordered_set<uint64_t>& s, int x) { return s.insert(uint64_t(x)).second; });
                         })});
    probe.pts.push_back({double(n), timeIt([&] {
                           ProbeSet v;
                           run(v, [](ProbeSet& s, int x) { return s.insert(uint64_t(x)); });
                         })});
  }
}

static void b4() {
  Series& trie = add("B4", "abbrev resolve: HexTrie (1000 queries)", "0 (O(prefix len))");
  Series& scan = add("B4", "abbrev resolve: linear scan (1000 queries)", "1 (O(n) per query)");
  for (size_t n = 1 << 10; n <= (1 << 17); n <<= 1) {
    std::vector<Id> ids = makeIds(n, 2);
    HexTrie t;
    for (const Id& id : ids) t.insert(id);
    std::vector<std::string> hexes;
    for (const Id& id : ids) hexes.push_back(id.hex());
    std::vector<std::string> qs;
    for (size_t i = 0; i < 1000; ++i) qs.push_back(hexes[(i * 7919) % n].substr(0, 10));
    trie.pts.push_back({double(n), timeIt([&] {
                          size_t u = 0;
                          for (const std::string& q : qs) u += t.resolve(q).count;
                          keep(u);
                        })});
    scan.pts.push_back({double(n), timeIt([&] {
                          size_t u = 0;
                          for (const std::string& q : qs)
                            for (const std::string& h : hexes) u += h.compare(0, q.size(), q) == 0;
                          keep(u);
                        })});
  }
}

static void b6() {
  using K = std::pair<int64_t, int32_t>;
  Series& tree = add("B6", "time range (k=100): B+ tree range scan", "0 (O(log n + k))");
  Series& scan = add("B6", "time range (k=100): linear scan", "1 (O(n))");
  for (size_t n = 1 << 12; n <= (1 << 20); n <<= 2) {
    std::vector<std::pair<K, int>> keys(n);
    for (size_t i = 0; i < n; ++i) keys[i] = {K{static_cast<int64_t>(i) * 60, static_cast<int32_t>(i)}, static_cast<int>(i)};
    BPlusTree<K, int> t(64);
    t.bulkLoad(keys);
    std::mt19937 rng(9);
    std::vector<int64_t> starts;
    for (int q = 0; q < 200; ++q) starts.push_back(static_cast<int64_t>(rng() % (n - 100)) * 60);
    tree.pts.push_back({double(n), timeIt([&] {
                          size_t got = 0;
                          for (int64_t s : starts) got += t.rangeScan(K{s, INT32_MIN}, K{s + 99 * 60, INT32_MAX}).size();
                          keep(got);
                        })});
    scan.pts.push_back({double(n), timeIt([&] {
                          size_t got = 0;
                          for (int64_t s : starts)
                            for (const auto& kv : keys) got += kv.first.first >= s && kv.first.first <= s + 99 * 60;
                          keep(got);
                        }, 1)});
  }
}

static void b10() {
  Series& map = add("B10", "staging index: std::map insert+iterate", "1+ (O(n log n))");
  Series& vec = add("B10", "staging index: sorted vector insert+iterate", "2 (O(n^2) moves)");
  for (size_t n = 1 << 10; n <= (1 << 16); n <<= 1) {
    std::vector<std::string> paths;
    std::mt19937 rng(11);
    for (size_t i = 0; i < n; ++i)
      paths.push_back("src/dir" + std::to_string(rng() % 100) + "/file" + std::to_string(rng()) + ".cpp");
    map.pts.push_back({double(n), timeIt([&] {
                         std::map<std::string, int> m;
                         for (const std::string& p : paths) m.emplace(p, 1);
                         size_t len = 0;
                         for (const auto& kv : m) len += kv.first.size();
                         keep(len);
                       })});
    vec.pts.push_back({double(n), timeIt([&] {
                         std::vector<std::string> v;
                         for (const std::string& p : paths) v.insert(std::lower_bound(v.begin(), v.end(), p), p);
                         size_t len = 0;
                         for (const std::string& s : v) len += s.size();
                         keep(len);
                       }, 1)});
  }
}

void benchStructures() {
  std::fprintf(stderr, "B1...\n");
  b1();
  std::fprintf(stderr, "B2...\n");
  b2();
  std::fprintf(stderr, "B4...\n");
  b4();
  std::fprintf(stderr, "B6...\n");
  b6();
  std::fprintf(stderr, "B10...\n");
  b10();
}

}
