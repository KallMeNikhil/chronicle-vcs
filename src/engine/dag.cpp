#include "engine/dag.hpp"

#include <algorithm>
#include <queue>
#include <stack>
#include <unordered_set>

namespace chr {

Status Dag::load(ObjectStore& store, const std::vector<Id>& tips) {
  nodes_.clear();
  index_.clear();
  std::vector<std::vector<Id>> parentIds;
  std::queue<Id> q;
  for (const Id& t : tips) q.push(t);
  while (!q.empty()) {
    Id id = q.front();
    q.pop();
    if (index_.count(id)) continue;
    Commit c;
    CHR_TRY(store.readCommit(id, &c));
    index_[id] = static_cast<int>(nodes_.size());
    CommitNode n;
    n.id = id;
    n.time = c.time;
    nodes_.push_back(n);
    parentIds.push_back(c.parents);
    for (const Id& p : c.parents)
      if (!index_.count(p)) q.push(p);
  }
  for (size_t i = 0; i < nodes_.size(); ++i) {
    for (const Id& p : parentIds[i]) {
      int pi = index_.at(p);
      nodes_[i].parents.push_back(pi);
      nodes_[static_cast<size_t>(pi)].children.push_back(static_cast<int>(i));
    }
  }
  std::vector<int> missing(nodes_.size());
  std::queue<int> ready;
  for (size_t i = 0; i < nodes_.size(); ++i) {
    missing[i] = static_cast<int>(nodes_[i].parents.size());
    if (missing[i] == 0) ready.push(static_cast<int>(i));
  }
  size_t done = 0;
  while (!ready.empty()) {
    int x = ready.front();
    ready.pop();
    ++done;
    int32_t g = 0;
    for (int p : nodes_[static_cast<size_t>(x)].parents) g = std::max(g, nodes_[static_cast<size_t>(p)].generation);
    nodes_[static_cast<size_t>(x)].generation = g + 1;
    for (int c : nodes_[static_cast<size_t>(x)].children)
      if (--missing[static_cast<size_t>(c)] == 0) ready.push(c);
  }
  if (done != nodes_.size()) return CorruptErr("commit graph contains a cycle");
  return Status::Ok();
}

Status Dag::loadGraph(const std::vector<Id>& ids, const std::vector<int64_t>& times,
                      const std::vector<std::vector<int>>& parents) {
  nodes_.clear();
  index_.clear();
  for (size_t i = 0; i < ids.size(); ++i) {
    CommitNode n;
    n.id = ids[i];
    n.time = times[i];
    int32_t g = 0;
    for (int p : parents[i]) {
      if (p < 0 || static_cast<size_t>(p) >= i) return Err("loadGraph: parents must precede children");
      n.parents.push_back(p);
      nodes_[static_cast<size_t>(p)].children.push_back(static_cast<int>(i));
      g = std::max(g, nodes_[static_cast<size_t>(p)].generation);
    }
    n.generation = g + 1;
    index_[ids[i]] = static_cast<int>(i);
    nodes_.push_back(n);
  }
  return Status::Ok();
}

int Dag::idxOf(const Id& id) const {
  auto it = index_.find(id);
  return it == index_.end() ? -1 : it->second;
}

bool Dag::isAncestor(int a, int b) const {
  const int32_t ga = node(a).generation;
  std::unordered_set<int> seen;
  std::stack<int> st;
  st.push(b);
  while (!st.empty()) {
    int x = st.top();
    st.pop();
    if (x == a) return true;
    if (node(x).generation <= ga || !seen.insert(x).second) continue;
    for (int p : node(x).parents) st.push(p);
  }
  return false;
}

std::vector<char> Dag::reach(int x) const {
  std::vector<char> in(nodes_.size(), 0);
  std::queue<int> q;
  q.push(x);
  in[static_cast<size_t>(x)] = 1;
  while (!q.empty()) {
    int y = q.front();
    q.pop();
    for (int p : node(y).parents)
      if (!in[static_cast<size_t>(p)]) {
        in[static_cast<size_t>(p)] = 1;
        q.push(p);
      }
  }
  return in;
}

std::vector<int> Dag::mergeBases(int a, int b) const {
  std::vector<char> A = reach(a), B = reach(b);
  std::vector<int> common;
  for (int i = 0; i < size(); ++i)
    if (A[static_cast<size_t>(i)] && B[static_cast<size_t>(i)]) common.push_back(i);
  std::sort(common.begin(), common.end(), [&](int x, int y) {
    if (node(x).generation != node(y).generation) return node(x).generation > node(y).generation;
    return x < y;
  });
  std::unordered_set<int> mark;
  std::vector<int> best;
  for (int c : common) {
    if (mark.count(c)) continue;
    best.push_back(c);
    std::queue<int> q;
    for (int p : node(c).parents) q.push(p);
    while (!q.empty()) {
      int y = q.front();
      q.pop();
      if (!mark.insert(y).second) continue;
      for (int p : node(y).parents) q.push(p);
    }
  }
  return best;
}

int Dag::chooseBase(const std::vector<int>& bases) const {
  int pick = -1;
  for (int b : bases) {
    if (pick < 0 || node(b).time > node(pick).time ||
        (node(b).time == node(pick).time && node(b).id < node(pick).id))
      pick = b;
  }
  return pick;
}

Status refTips(Repo& repo, std::vector<Id>* out) {
  out->clear();
  std::vector<std::pair<std::string, Id>> branches;
  CHR_TRY(repo.refs.listBranches(&branches));
  for (const auto& b : branches) out->push_back(b.second);
  HeadInfo h;
  CHR_TRY(repo.refs.readHead(&h));
  if (!h.unborn) out->push_back(h.id);
  return Status::Ok();
}

}
