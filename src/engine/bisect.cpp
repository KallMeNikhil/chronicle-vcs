#include "engine/bisect.hpp"

#include <algorithm>
#include <cstdint>
#include <queue>
#include <unordered_set>

#include "core/strutil.hpp"
#include "util/fs.hpp"

namespace chr {

Status BisectState::load(const std::string& chronicleDir) {
  *this = BisectState();
  std::string path = chronicleDir + "/BISECT";
  if (!fsu::exists(path)) return Status::Ok();
  std::string text;
  CHR_TRY(fsu::readFile(path, &text));
  active = true;
  for (const std::string& line : split(text, '\n')) {
    if (line.empty()) continue;
    size_t sp = line.find(' ');
    std::string k = line.substr(0, sp), v = sp == std::string::npos ? "" : line.substr(sp + 1);
    Id id;
    std::string un;
    if (k == "bad" && Id::fromHex(v, &bad)) haveBad = true;
    else if (k == "good" && Id::fromHex(v, &id)) goods.push_back(id);
    else if (k == "skip" && Id::fromHex(v, &id)) skips.push_back(id);
    else if (k == "next" && Id::fromHex(v, &next)) haveNext = true;
    else if (k == "orig" && unescapePath(v, &un)) orig = un;
    else if (k == "strategy") strategy = v;
    else if (k == "log" && unescapePath(v, &un)) log.push_back(un);
    else return CorruptErr("BISECT: bad line '" + line + "'");
  }
  return Status::Ok();
}

Status BisectState::save(const std::string& chronicleDir) const {
  std::string out = "orig " + escapePath(orig) + "\nstrategy " + strategy + "\n";
  if (haveBad) out += "bad " + bad.hex() + "\n";
  for (const Id& g : goods) out += "good " + g.hex() + "\n";
  for (const Id& s : skips) out += "skip " + s.hex() + "\n";
  if (haveNext) out += "next " + next.hex() + "\n";
  for (const std::string& l : log) out += "log " + escapePath(l) + "\n";
  return fsu::writeFileAtomic(chronicleDir + "/BISECT", out);
}

Status bisectCandidates(const Dag& dag, const BisectState& st, std::vector<int>* out) {
  out->clear();
  int b = dag.idxOf(st.bad);
  if (b < 0) return Err("bisect: bad commit not loaded");
  std::vector<char> in = dag.reach(b);
  for (const Id& g : st.goods) {
    int gi = dag.idxOf(g);
    if (gi < 0) continue;
    std::vector<char> ga = dag.reach(gi);
    for (size_t i = 0; i < in.size(); ++i)
      if (ga[i]) in[i] = 0;
  }
  auto older = [&](int x, int y) {
    const CommitNode &a = dag.node(x), &c = dag.node(y);
    if (a.time != c.time) return a.time > c.time;
    return c.id < a.id;
  };
  std::priority_queue<int, std::vector<int>, decltype(older)> heap(older);
  std::vector<int> pending(in.size(), 0);
  for (int i = 0; i < dag.size(); ++i) {
    if (!in[static_cast<size_t>(i)]) continue;
    for (int p : dag.node(i).parents)
      if (in[static_cast<size_t>(p)]) ++pending[static_cast<size_t>(i)];
    if (pending[static_cast<size_t>(i)] == 0) heap.push(i);
  }
  while (!heap.empty()) {
    int x = heap.top();
    heap.pop();
    out->push_back(x);
    for (int c : dag.node(x).children)
      if (in[static_cast<size_t>(c)] && --pending[static_cast<size_t>(c)] == 0) heap.push(c);
  }
  return Status::Ok();
}

static int nearestUnskipped(const std::vector<char>& skipped, int want) {
  int n = static_cast<int>(skipped.size()) - 1;
  for (int d = 0; d < n; ++d) {
    for (int cand : {want + d, want - d}) {
      if (cand >= 0 && cand < n && !skipped[static_cast<size_t>(cand)]) return cand;
    }
  }
  return -1;
}

int pickLinear(const std::vector<char>& skipped) {
  int n = static_cast<int>(skipped.size());
  return nearestUnskipped(skipped, (n - 1) / 2);
}

static int pickWeighted(const Dag& dag, const std::vector<int>& list, const std::vector<char>& skipped) {
  size_t n = list.size();
  size_t words = (n + 63) / 64;
  std::vector<int> pos(static_cast<size_t>(dag.size()), -1);
  for (size_t i = 0; i < n; ++i) pos[static_cast<size_t>(list[i])] = static_cast<int>(i);
  std::vector<std::vector<uint64_t>> rows(n, std::vector<uint64_t>(words, 0));
  int best = -1;
  size_t bestDist = SIZE_MAX;
  for (size_t i = 0; i < n; ++i) {
    rows[i][i / 64] |= uint64_t(1) << (i % 64);
    for (int p : dag.node(list[i]).parents) {
      int pi = pos[static_cast<size_t>(p)];
      if (pi < 0) continue;
      for (size_t w = 0; w < words; ++w) rows[i][w] |= rows[static_cast<size_t>(pi)][w];
    }
    size_t count = 0;
    for (uint64_t w : rows[i]) count += static_cast<size_t>(__builtin_popcountll(w));
    size_t dist = count * 2 > n ? count * 2 - n : n - count * 2;
    if (i + 1 < n && !skipped[i] && dist < bestDist) {
      bestDist = dist;
      best = static_cast<int>(i);
    }
  }
  return best;
}

Status bisectNext(ObjectStore& store, const BisectState& st, BisectStep* out) {
  *out = BisectStep();
  if (!st.haveBad || st.goods.empty()) return Err("bisect needs a bad and at least one good commit");
  std::vector<Id> tips = {st.bad};
  tips.insert(tips.end(), st.goods.begin(), st.goods.end());
  Dag dag;
  CHR_TRY(dag.load(store, tips));
  std::vector<int> list;
  CHR_TRY(bisectCandidates(dag, st, &list));
  out->remaining = list.size();
  if (list.empty()) return Err("bisect: the bad commit is an ancestor of a good commit");
  if (list.size() == 1) {
    out->done = true;
    out->firstBad = dag.node(list[0]).id;
    return Status::Ok();
  }
  std::unordered_set<Id, IdHash> skipSet(st.skips.begin(), st.skips.end());
  std::vector<char> skipped(list.size(), 0);
  for (size_t i = 0; i < list.size(); ++i) skipped[i] = skipSet.count(dag.node(list[i]).id) ? 1 : 0;
  int pick = -1;
  if (st.strategy == "weighted" && list.size() <= 5000) {
    pick = pickWeighted(dag, list, skipped);
  } else {
    if (st.strategy == "weighted") out->warning = "more than 5000 candidates: using the linear strategy";
    pick = pickLinear(skipped);
  }
  if (pick < 0) {
    out->onlySkipped = true;
    for (int x : list) out->left.push_back(dag.node(x).id);
    return Status::Ok();
  }
  out->test = dag.node(list[static_cast<size_t>(pick)]).id;
  return Status::Ok();
}

}
