#include "engine/blame.hpp"

#include <algorithm>
#include <stack>
#include <unordered_set>

#include "engine/diff.hpp"

namespace chr {

int Blamer::rep(int x, const std::unordered_map<int, int>& touched) {
  while (x >= 0 && !touched.count(x)) {
    const std::vector<int>& ps = dag_.node(x).parents;
    x = ps.empty() ? -1 : ps[0];
  }
  return x;
}

Status Blamer::compute(int c, int pathId, const HistEvent& ev, const std::unordered_map<int, int>& touched) {
  State st;
  if (ev.kind != 'D') {
    std::string content;
    CHR_TRY(store_.readBlob(ev.blob, &content));
    if (isBinary(content)) return Err("blame: binary file '" + ev.path + "'");
    auto lines = std::make_shared<std::vector<std::string>>(splitLines(content));
    auto origin = std::make_shared<std::vector<int>>(lines->size(), -1);
    for (int p : dag_.node(c).parents) {
      int r = rep(p, touched);
      if (r < 0) continue;
      const State& ps = memo_.at((static_cast<Key>(r) << 32) | static_cast<uint32_t>(pathId));
      if (!ps.present) continue;
      ++diffs_;
      for (const DiffOp& op : diffLines(*ps.lines, *lines))
        if (op.kind == '=' && (*origin)[static_cast<size_t>(op.b)] < 0)
          (*origin)[static_cast<size_t>(op.b)] = (*ps.origin)[static_cast<size_t>(op.a)];
    }
    for (int& o : *origin)
      if (o < 0) o = c;
    st.present = true;
    st.origin = origin;
    st.lines = lines;
  }
  memo_[(static_cast<Key>(c) << 32) | static_cast<uint32_t>(pathId)] = st;
  return Status::Ok();
}

Status Blamer::blame(const Id& rev, const std::string& path, std::vector<int>* origins,
                     std::vector<std::string>* lines) {
  if (!useMemo_) memo_.clear();
  int R = dag_.idxOf(rev);
  if (R < 0) return Err("blame: revision not loaded");
  int pathId = pathIds_.emplace(path, static_cast<int>(pathIds_.size())).first->second;
  std::unordered_map<int, int> touched;
  if (const std::vector<int>* evs = hist_.forPath(path))
    for (int e : *evs) {
      int ci = dag_.idxOf(hist_.events()[static_cast<size_t>(e)].commit);
      if (ci >= 0) touched[ci] = e;
    }
  int top = rep(R, touched);
  std::vector<int> todo;
  std::unordered_set<int> seen;
  std::stack<int> st;
  if (top >= 0) st.push(top);
  while (!st.empty()) {
    int x = st.top();
    st.pop();
    if (!seen.insert(x).second) continue;
    if (memo_.count((static_cast<Key>(x) << 32) | static_cast<uint32_t>(pathId))) continue;
    todo.push_back(x);
    for (int p : dag_.node(x).parents) {
      int r = rep(p, touched);
      if (r >= 0) st.push(r);
    }
  }
  std::sort(todo.begin(), todo.end(), [&](int a, int b) {
    return dag_.node(a).generation != dag_.node(b).generation ? dag_.node(a).generation < dag_.node(b).generation
                                                              : a < b;
  });
  for (int x : todo)
    CHR_TRY(compute(x, pathId, hist_.events()[static_cast<size_t>(touched.at(x))], touched));
  if (top < 0) return Err("blame: no such path '" + path + "' in that revision");
  const State& res = memo_.at((static_cast<Key>(top) << 32) | static_cast<uint32_t>(pathId));
  if (!res.present) return Err("blame: no such path '" + path + "' in that revision");
  *origins = *res.origin;
  *lines = *res.lines;
  return Status::Ok();
}

}
