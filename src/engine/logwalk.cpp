#include "engine/logwalk.hpp"

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace chr {

static std::unordered_set<int> reachable(const Dag& dag, const std::vector<int>& tips) {
  std::unordered_set<int> seen;
  std::queue<int> q;
  for (int t : tips)
    if (seen.insert(t).second) q.push(t);
  while (!q.empty()) {
    int x = q.front();
    q.pop();
    for (int p : dag.node(x).parents)
      if (seen.insert(p).second) q.push(p);
  }
  return seen;
}

namespace {
struct NewerFirst {
  const Dag* dag;
  bool operator()(int x, int y) const {
    const CommitNode &a = dag->node(x), &b = dag->node(y);
    if (a.time != b.time) return a.time < b.time;
    if (a.generation != b.generation) return a.generation < b.generation;
    return a.id < b.id;
  }
};
}

std::vector<int> dateOrderWalk(const Dag& dag, const std::vector<int>& tips) {
  std::unordered_set<int> in = reachable(dag, tips);
  std::unordered_map<int, int> pending;
  for (int x : in) {
    int n = 0;
    for (int c : dag.node(x).children)
      if (in.count(c)) ++n;
    pending[x] = n;
  }
  std::priority_queue<int, std::vector<int>, NewerFirst> heap(NewerFirst{&dag});
  for (int x : in)
    if (pending[x] == 0) heap.push(x);
  std::vector<int> out;
  out.reserve(in.size());
  while (!heap.empty()) {
    int x = heap.top();
    heap.pop();
    out.push_back(x);
    for (int p : dag.node(x).parents)
      if (--pending[p] == 0) heap.push(p);
  }
  return out;
}

std::vector<int> sortedWalk(const Dag& dag, const std::vector<int>& tips) {
  std::unordered_set<int> in = reachable(dag, tips);
  std::vector<int> out(in.begin(), in.end());
  NewerFirst less{&dag};
  std::stable_sort(out.begin(), out.end(), [&](int x, int y) { return less(y, x); });
  return out;
}

}
