#include "engine/gc.hpp"

#include <stack>

#include "core/sha256.hpp"
#include "core/timeutil.hpp"
#include "engine/bisect.hpp"
#include "engine/dag.hpp"
#include "store/index.hpp"
#include "store/lock.hpp"
#include "store/state.hpp"
#include "util/fs.hpp"

namespace chr {

Status gcRoots(Repo& repo, std::vector<Id>* roots) {
  CHR_TRY(refTips(repo, roots));
  MergeState ms;
  CHR_TRY(ms.load(repo.dir));
  if (ms.active) roots->push_back(ms.theirs);
  BisectState bs;
  CHR_TRY(bs.load(repo.dir));
  if (bs.haveBad) roots->push_back(bs.bad);
  if (bs.haveNext) roots->push_back(bs.next);
  roots->insert(roots->end(), bs.goods.begin(), bs.goods.end());
  roots->insert(roots->end(), bs.skips.begin(), bs.skips.end());
  Index index;
  if (index.load(repo.path("index")).ok())
    for (const auto& kv : index.entries) roots->push_back(kv.second.id);
  return Status::Ok();
}

Status markReachable(ObjectStore& store, const std::vector<Id>& roots, IdSet* reach,
                     std::vector<std::pair<Id, Id>>* missing, bool skipUnreadable) {
  std::stack<std::pair<Id, Id>> st;
  for (const Id& r : roots) st.push({r, r});
  while (!st.empty()) {
    auto [id, from] = st.top();
    st.pop();
    if (reach->count(id)) continue;
    if (!store.has(id)) {
      if (missing) missing->push_back({id, from});
      continue;
    }
    reach->insert(id);
    ObjType t;
    std::string payload;
    Status got = store.get(id, &t, &payload);
    if (!got.ok()) {
      if (skipUnreadable) continue;
      return got;
    }
    if (t == ObjType::Commit) {
      Commit c;
      CHR_TRY(decodeCommit(payload, &c));
      st.push({c.tree, id});
      for (const Id& p : c.parents) st.push({p, id});
    } else if (t == ObjType::Tree) {
      std::vector<TreeEntry> entries;
      CHR_TRY(decodeTree(payload, &entries));
      for (const TreeEntry& e : entries) st.push({e.id, id});
    }
  }
  return Status::Ok();
}

static Status nameHints(Repo& repo, std::unordered_map<Id, uint32_t, IdHash>* hints) {
  std::vector<Id> tips;
  CHR_TRY(refTips(repo, &tips));
  if (tips.empty()) return Status::Ok();
  Dag dag;
  CHR_TRY(dag.load(repo.store, tips));
  std::stack<Id> trees;
  for (int i = 0; i < dag.size(); ++i) {
    Commit c;
    CHR_TRY(repo.store.readCommit(dag.node(i).id, &c));
    trees.push(c.tree);
  }
  IdSet seenTrees;
  while (!trees.empty()) {
    Id t = trees.top();
    trees.pop();
    if (!seenTrees.insert(t).second) continue;
    std::vector<TreeEntry> entries;
    CHR_TRY(repo.store.readTree(t, &entries));
    for (const TreeEntry& e : entries) {
      uint32_t h = static_cast<uint32_t>(sha256(e.name).prefix64() >> 32);
      hints->emplace(e.id, h);
      if (e.isDir()) trees.push(e.id);
    }
  }
  return Status::Ok();
}

static PackParams paramsOf(const Repo& repo) {
  PackParams p;
  p.window = static_cast<int>(repo.config.getInt("pack.window", 10));
  p.maxDepth = static_cast<int>(repo.config.getInt("pack.maxdepth", 50));
  p.minSaving = static_cast<int>(repo.config.getInt("pack.minsaving", 50));
  return p;
}

static Status collectInputs(Repo& repo, const std::vector<Id>& ids, std::vector<PackInput>* out) {
  std::unordered_map<Id, uint32_t, IdHash> hints;
  CHR_TRY(nameHints(repo, &hints));
  for (const Id& id : ids) {
    PackInput in;
    in.id = id;
    CHR_TRY(repo.store.get(id, &in.type, &in.payload));
    auto h = hints.find(id);
    in.nameHint = h == hints.end() ? 0 : h->second;
    out->push_back(std::move(in));
  }
  return Status::Ok();
}

Status packLoose(Repo& repo, bool keepLoose, PackWriteStats* stats) {
  std::vector<Id> loose;
  CHR_TRY(repo.store.loose().list(&loose));
  *stats = PackWriteStats();
  if (loose.empty()) return Status::Ok();
  std::vector<PackInput> inputs;
  CHR_TRY(collectInputs(repo, loose, &inputs));
  CHR_TRY(writePack(repo.store.dir() + "/pack", inputs, paramsOf(repo), stats));
  repo.store.reloadPacks();
  if (keepLoose) return Status::Ok();
  for (const Id& id : loose) {
    if (!repo.store.has(id)) return CorruptErr("object " + id.hex() + " missing after pack; loose copies kept");
    CHR_TRY(repo.store.loose().remove(id));
  }
  return Status::Ok();
}

Status runGc(Repo& repo, bool dryRun, int64_t graceSeconds, GcReport* out) {
  *out = GcReport();
  RepoLock gcLock;
  CHR_TRY(gcLock.acquire(repo.dir, "gc.lock"));
  std::vector<Id> roots;
  CHR_TRY(gcRoots(repo, &roots));
  IdSet reach;
  std::vector<std::pair<Id, Id>> missing;
  CHR_TRY(markReachable(repo.store, roots, &reach, &missing));
  if (!missing.empty())
    return CorruptErr("gc aborted: reachable object " + missing[0].first.hex() + " (referenced by " +
                      missing[0].second.hex() + ") is missing; run 'chronicle verify'");
  out->reachable = reach.size();
  std::vector<Id> loose;
  CHR_TRY(repo.store.loose().list(&loose));
  std::vector<Pack*> packs;
  CHR_TRY(repo.store.packs(&packs));
  out->oldPacks = packs.size();
  IdSet inPacks;
  for (Pack* p : packs)
    for (const IdEntry& e : p->index()) inPacks.insert(e.id);
  for (const Id& id : inPacks)
    if (!reach.count(id)) ++out->packedDropped;
  const int64_t now = nowSeconds();
  std::vector<Id> deleteLoose;
  for (const Id& id : loose) {
    if (reach.count(id)) {
      ++out->looseRepacked;
      deleteLoose.push_back(id);
    } else if (now - repo.store.loose().mtimeSeconds(id) >= graceSeconds) {
      ++out->looseDeleted;
      deleteLoose.push_back(id);
    } else {
      ++out->looseKeptGrace;
    }
  }
  if (dryRun) return Status::Ok();

  std::vector<Id> keep(reach.begin(), reach.end());
  std::vector<PackInput> inputs;
  CHR_TRY(collectInputs(repo, keep, &inputs));
  std::vector<std::string> oldFiles;
  for (Pack* p : packs) {
    oldFiles.push_back(p->idxPath());
    oldFiles.push_back(p->packPath());
  }
  if (!inputs.empty()) CHR_TRY(writePack(repo.store.dir() + "/pack", inputs, paramsOf(repo), &out->pack));
  std::string newStem = repo.store.dir() + "/pack/" + out->pack.name;
  for (const std::string& f : oldFiles)
    if (!startsWith(f, newStem + ".")) CHR_TRY(fsu::removeFile(f));
  repo.store.reloadPacks();
  for (const Id& id : deleteLoose) {
    if (reach.count(id) && !repo.store.has(id)) return CorruptErr("object " + id.hex() + " not in new pack");
    CHR_TRY(repo.store.loose().remove(id));
  }
  std::vector<std::string> tmp;
  if (fsu::isDir(repo.store.dir() + "/tmp") && fsu::listDir(repo.store.dir() + "/tmp", &tmp).ok())
    for (const std::string& t : tmp) fsu::removeFile(repo.store.dir() + "/tmp/" + t);
  return Status::Ok();
}

}
