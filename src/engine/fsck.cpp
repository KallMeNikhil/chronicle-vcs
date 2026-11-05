#include "engine/fsck.hpp"

#include <unordered_map>

#include "engine/gc.hpp"
#include "store/pack.hpp"

namespace chr {

Status runFsck(Repo& repo, FsckReport* out) {
  *out = FsckReport();
  std::vector<Pack*> packs;
  CHR_TRY(repo.store.packs(&packs));
  for (Pack* p : packs) {
    Status s = p->verifyTrailer();
    if (!s.ok()) out->corrupt.push_back(s.msg);
  }
  std::unordered_map<Id, ObjType, IdHash> types;
  std::vector<std::pair<Id, Id>> refs;
  IdSet bad;
  CHR_TRY(repo.store.forEachId([&](const Id& id) {
    ++out->checked;
    repo.store.clearCache();
    ObjType t;
    std::string payload;
    Status s = repo.store.get(id, &t, &payload);
    if (!s.ok()) {
      out->corrupt.push_back(s.msg);
      bad.insert(id);
      return Status::Ok();
    }
    types[id] = t;
    if (t == ObjType::Commit) {
      Commit c;
      if (!decodeCommit(payload, &c).ok()) {
        out->corrupt.push_back("unparsable commit " + id.hex());
        return Status::Ok();
      }
      refs.push_back({c.tree, id});
      for (const Id& p : c.parents) refs.push_back({p, id});
    } else if (t == ObjType::Tree) {
      std::vector<TreeEntry> entries;
      if (!decodeTree(payload, &entries).ok()) {
        out->corrupt.push_back("unparsable tree " + id.hex());
        return Status::Ok();
      }
      for (const TreeEntry& e : entries) refs.push_back({e.id, id});
    }
    return Status::Ok();
  }));
  for (const auto& r : refs)
    if (!types.count(r.first) && !bad.count(r.first)) out->missing.push_back(r);
  std::vector<Id> roots;
  CHR_TRY(gcRoots(repo, &roots));
  for (const Id& r : roots)
    if (!types.count(r) && !bad.count(r)) out->missing.push_back({r, r});
  IdSet reach;
  Status s = markReachable(repo.store, roots, &reach, nullptr, true);
  if (!s.ok()) out->corrupt.push_back("reachability walk: " + s.msg);
  for (const auto& kv : types)
    if (!reach.count(kv.first)) out->dangling.push_back({kv.second, kv.first});
  return Status::Ok();
}

}
