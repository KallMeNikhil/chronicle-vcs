#include "exp/reconcile.hpp"

#include <algorithm>
#include <map>
#include <queue>
#include <stack>
#include <unordered_set>

#include "engine/dag.hpp"

namespace chr {

using IdSet = std::unordered_set<Id, IdHash>;

static IdSet idsOf(const Dag& dag) {
  IdSet s;
  for (int i = 0; i < dag.size(); ++i) s.insert(dag.node(i).id);
  return s;
}

static bool commonBase(const Dag& local, const IdSet& remote, Id* out) {
  std::vector<int> common;
  for (int i = 0; i < local.size(); ++i)
    if (remote.count(local.node(i).id)) common.push_back(i);
  if (common.empty()) return false;
  std::sort(common.begin(), common.end(), [&](int a, int b) {
    if (local.node(a).generation != local.node(b).generation)
      return local.node(a).generation > local.node(b).generation;
    return local.node(a).time > local.node(b).time;
  });
  *out = local.node(common[0]).id;
  return true;
}

Status compareRepos(Repo& local, Repo& other, std::vector<BranchReport>* out) {
  out->clear();
  std::vector<std::pair<std::string, Id>> lb, ob;
  CHR_TRY(local.refs.listBranches(&lb));
  CHR_TRY(other.refs.listBranches(&ob));
  std::map<std::string, BranchReport> byName;
  for (const auto& b : lb) {
    BranchReport& r = byName[b.first];
    r.name = b.first;
    r.here = true;
    r.local = b.second;
  }
  for (const auto& b : ob) {
    BranchReport& r = byName[b.first];
    r.name = b.first;
    r.there = true;
    r.remote = b.second;
  }
  for (auto& kv : byName) {
    BranchReport& r = kv.second;
    if (r.here && r.there && !(r.local == r.remote)) {
      Dag a, b;
      CHR_TRY(a.load(local.store, {r.local}));
      CHR_TRY(b.load(other.store, {r.remote}));
      IdSet sa = idsOf(a), sb = idsOf(b);
      for (const Id& id : sa) r.ahead += !sb.count(id);
      for (const Id& id : sb) r.behind += !sa.count(id);
      r.haveBase = commonBase(a, sb, &r.base);
    } else if (!r.here) {
      Dag b;
      CHR_TRY(b.load(other.store, {r.remote}));
      r.behind = static_cast<size_t>(b.size());
    }
    out->push_back(r);
  }
  return Status::Ok();
}

Status fetchObjects(Repo& local, Repo& other, const Id& tip, size_t* copied) {
  *copied = 0;
  std::stack<Id> st;
  st.push(tip);
  IdSet seen;
  while (!st.empty()) {
    Id id = st.top();
    st.pop();
    if (!seen.insert(id).second) continue;
    if (local.store.has(id)) continue;
    ObjType t;
    std::string payload;
    CHR_TRY(other.store.get(id, &t, &payload));
    if (t == ObjType::Commit) {
      Commit c;
      CHR_TRY(decodeCommit(payload, &c));
      st.push(c.tree);
      for (const Id& p : c.parents) st.push(p);
    } else if (t == ObjType::Tree) {
      std::vector<TreeEntry> entries;
      CHR_TRY(decodeTree(payload, &entries));
      for (const TreeEntry& e : entries) st.push(e.id);
    }
    Id stored;
    CHR_TRY(local.store.put(t, payload, &stored));
    ++*copied;
  }
  return Status::Ok();
}

}
