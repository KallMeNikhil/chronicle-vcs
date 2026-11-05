#include <cmath>
#include <random>

#include "bench_util.hpp"
#include "core/sha256.hpp"
#include "engine/bisect.hpp"
#include "engine/dag.hpp"
#include "engine/logwalk.hpp"

using namespace chr;

namespace bench {

static void synthDag(size_t n, Dag* dag, unsigned seed) {
  std::mt19937 rng(seed);
  std::vector<Id> ids(n);
  std::vector<int64_t> times(n);
  std::vector<std::vector<int>> parents(n);
  for (size_t i = 0; i < n; ++i) {
    ids[i] = sha256("c" + std::to_string(i));
    times[i] = 1700000000 + static_cast<int64_t>(i) * 60 + static_cast<int64_t>(rng() % 30);
    if (i > 0) parents[i].push_back(static_cast<int>(i) - 1);
    if (i > 20 && rng() % 20 == 0) parents[i].push_back(static_cast<int>(i) - 2 - static_cast<int>(rng() % 15));
  }
  dag->loadGraph(ids, times, parents);
}

static void b3() {
  Series& heap = add("B3", "log walk: priority_queue topo walk", "1+ (O((V+E) log V))");
  Series& sort = add("B3", "log walk: collect + merge sort", "1+ (O(V log V))");
  for (size_t n = 1 << 12; n <= (1 << 19); n <<= 1) {
    Dag dag;
    synthDag(n, &dag, 3);
    std::vector<int> tips = {static_cast<int>(n - 1)};
    heap.pts.push_back({double(n), timeIt([&] { keep(dateOrderWalk(dag, tips).size()); })});
    sort.pts.push_back({double(n), timeIt([&] { keep(sortedWalk(dag, tips).size()); })});
  }
}

static void b8() {
  Series& tests = add("B8", "bisect tests used vs N (linear history)", "1 (tests = log2 N)", "tests");
  Series& bound = add("B8", "bisect bound ceil(log2 N)+1", "1", "tests");
  tests.logX = bound.logX = true;
  for (size_t n = 1 << 6; n <= (1 << 14); n <<= 1) {
    std::vector<Id> ids(n);
    std::vector<int64_t> times(n);
    std::vector<std::vector<int>> parents(n);
    for (size_t i = 0; i < n; ++i) {
      ids[i] = sha256("b" + std::to_string(i));
      times[i] = static_cast<int64_t>(i);
      if (i) parents[i] = {static_cast<int>(i) - 1};
    }
    Dag dag;
    dag.loadGraph(ids, times, parents);
    size_t planted = n * 37 / 100 + 1;
    BisectState st;
    st.haveBad = true;
    st.bad = ids[n - 1];
    st.goods = {ids[0]};
    int used = 0;
    for (;;) {
      std::vector<int> list;
      bisectCandidates(dag, st, &list);
      if (list.size() <= 1) break;
      int pick = pickLinear(std::vector<char>(list.size(), 0));
      size_t idx = static_cast<size_t>(list[static_cast<size_t>(pick)]);
      ++used;
      if (idx >= planted) st.bad = ids[idx];
      else st.goods.push_back(ids[idx]);
    }
    tests.pts.push_back({double(n), double(used)});
    bound.pts.push_back({double(n), std::ceil(std::log2(double(n))) + 1});
  }
}

void benchGraph() {
  std::fprintf(stderr, "B3...\n");
  b3();
  std::fprintf(stderr, "B8...\n");
  b8();
}

}
