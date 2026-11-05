#include "store/objstore.hpp"

#include <unordered_set>

#include "core/strutil.hpp"
#include "store/delta.hpp"
#include "store/pack.hpp"
#include "util/fs.hpp"

namespace chr {

static const size_t kCacheMax = 512;
static const size_t kCacheObjectMax = 1 << 20;

ObjectStore::ObjectStore() = default;
ObjectStore::~ObjectStore() = default;

void ObjectStore::setDir(const std::string& objectsDir) {
  dir_ = objectsDir;
  loose_.setDir(objectsDir);
  reloadPacks();
}

void ObjectStore::reloadPacks() {
  packs_.clear();
  packsLoaded_ = false;
  cache_.clear();
}

Status ObjectStore::packs(std::vector<Pack*>* out) {
  out->clear();
  if (!packsLoaded_) {
    packsLoaded_ = true;
    std::string dir = dir_ + "/pack";
    std::vector<std::string> names;
    if (fsu::isDir(dir)) CHR_TRY(fsu::listDir(dir, &names));
    for (const std::string& n : names) {
      if (!endsWith(n, ".idx") || n.find(".tmp-") != std::string::npos) continue;
      std::string stem = dir + "/" + n.substr(0, n.size() - 4);
      if (!fsu::exists(stem + ".pack")) continue;
      auto p = std::make_unique<Pack>();
      CHR_TRY(p->open(stem + ".pack", stem + ".idx"));
      packs_.push_back(std::move(p));
    }
  }
  for (auto& p : packs_) out->push_back(p.get());
  return Status::Ok();
}

void ObjectStore::remember(const Id& id, ObjType t, const std::string& payload) {
  if (payload.size() > kCacheObjectMax) return;
  if (cache_.size() >= kCacheMax) cache_.clear();
  cache_[id] = Cached{t, payload};
}

Status ObjectStore::put(ObjType t, const std::string& payload, Id* out) {
  Id id = objectId(t, payload);
  if (out) *out = id;
  if (has(id)) return Status::Ok();
  return loose_.write(t, payload, nullptr);
}

static bool findPacked(const std::vector<Pack*>& packs, const Id& id, Pack** pack, uint64_t* off) {
  for (Pack* p : packs)
    if (p->find(id, off)) {
      *pack = p;
      return true;
    }
  return false;
}

static ObjType typeOfKind(uint8_t k) {
  return k == kPackBlob ? ObjType::Blob : k == kPackTree ? ObjType::Tree : ObjType::Commit;
}

Status ObjectStore::getPacked(const Id& id, ObjType* t, std::string* payload, bool* found) {
  *found = false;
  std::vector<Pack*> ps;
  CHR_TRY(packs(&ps));
  Pack* pack = nullptr;
  uint64_t off = 0;
  if (!findPacked(ps, id, &pack, &off)) return Status::Ok();
  *found = true;
  std::vector<std::string> deltas;
  Id cur = id;
  const size_t limit = static_cast<size_t>(maxDepth_) + 5;
  for (;;) {
    auto hit = cache_.find(cur);
    if (hit != cache_.end() && cur != id) {
      *t = hit->second.type;
      *payload = hit->second.payload;
      break;
    }
    uint8_t kind = 0;
    std::string data;
    Id base;
    if (!findPacked(ps, cur, &pack, &off)) {
      if (!loose_.has(cur)) return CorruptErr("missing delta base " + cur.hex() + " (for " + id.hex() + ")");
      CHR_TRY(loose_.read(cur, t, payload));
      break;
    }
    CHR_TRY(pack->readEntry(off, &kind, &data, &base));
    if (kind != kPackDelta) {
      *t = typeOfKind(kind);
      payload->swap(data);
      break;
    }
    deltas.push_back(std::move(data));
    if (deltas.size() > limit) return CorruptErr("delta chain too long for " + id.hex());
    cur = base;
  }
  for (size_t i = deltas.size(); i-- > 0;) {
    std::string next;
    Status s = applyDelta(*payload, deltas[i], &next);
    if (!s.ok()) return CorruptErr("corrupt packed object " + id.hex() + ": " + s.msg);
    payload->swap(next);
  }
  if (!(objectId(*t, *payload) == id)) return CorruptErr("corrupt packed object " + id.hex() + " (hash mismatch)");
  return Status::Ok();
}

Status ObjectStore::get(const Id& id, ObjType* t, std::string* payload) {
  auto it = cache_.find(id);
  if (it != cache_.end()) {
    *t = it->second.type;
    *payload = it->second.payload;
    return Status::Ok();
  }
  if (loose_.has(id)) {
    CHR_TRY(loose_.read(id, t, payload));
  } else {
    bool found = false;
    CHR_TRY(getPacked(id, t, payload, &found));
    if (!found) return CorruptErr("missing object " + id.hex());
  }
  remember(id, *t, *payload);
  return Status::Ok();
}

bool ObjectStore::has(const Id& id) {
  if (cache_.count(id) || loose_.has(id)) return true;
  std::vector<Pack*> ps;
  if (!packs(&ps).ok()) return false;
  Pack* p = nullptr;
  uint64_t off = 0;
  return findPacked(ps, id, &p, &off);
}

Status ObjectStore::forEachId(const std::function<Status(const Id&)>& fn) {
  std::vector<Id> ids;
  CHR_TRY(loose_.list(&ids));
  std::unordered_set<Id, IdHash> seen(ids.begin(), ids.end());
  for (const Id& id : ids) CHR_TRY(fn(id));
  std::vector<Pack*> ps;
  CHR_TRY(packs(&ps));
  for (Pack* p : ps)
    for (const IdEntry& e : p->index())
      if (seen.insert(e.id).second) CHR_TRY(fn(e.id));
  return Status::Ok();
}

static Status expectType(const Id& id, ObjType want, ObjType got) {
  if (want == got) return Status::Ok();
  return Err(std::string("object ") + id.hex() + " is a " + typeName(got) + ", expected " + typeName(want));
}

Status ObjectStore::readBlob(const Id& id, std::string* out) {
  ObjType t;
  CHR_TRY(get(id, &t, out));
  return expectType(id, ObjType::Blob, t);
}

Status ObjectStore::readTree(const Id& id, std::vector<TreeEntry>* out) {
  ObjType t;
  std::string payload;
  CHR_TRY(get(id, &t, &payload));
  CHR_TRY(expectType(id, ObjType::Tree, t));
  Status s = decodeTree(payload, out);
  if (!s.ok()) return CorruptErr("tree " + id.hex() + ": " + s.msg);
  return Status::Ok();
}

Status ObjectStore::readCommit(const Id& id, Commit* out) {
  ObjType t;
  std::string payload;
  CHR_TRY(get(id, &t, &payload));
  CHR_TRY(expectType(id, ObjType::Commit, t));
  Status s = decodeCommit(payload, out);
  if (!s.ok()) return CorruptErr("commit " + id.hex() + ": " + s.msg);
  return Status::Ok();
}

}
