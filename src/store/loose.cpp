#include "store/loose.hpp"

#include <sys/stat.h>

#include <cstdio>

#include "core/sha256.hpp"
#include "util/fs.hpp"

namespace chr {

std::string LooseStore::pathFor(const Id& id) const {
  std::string h = id.hex();
  return dir_ + "/" + h.substr(0, 2) + "/" + h.substr(2);
}

bool LooseStore::has(const Id& id) const { return fsu::lstatPath(pathFor(id)).isReg; }

Status LooseStore::read(const Id& id, ObjType* t, std::string* payload) const {
  std::string bytes;
  Status s = fsu::readFile(pathFor(id), &bytes);
  if (!s.ok()) return CorruptErr("missing object " + id.hex());
  if (!(sha256(bytes) == id)) return CorruptErr("corrupt object " + id.hex() + " (hash mismatch)");
  s = decodeObject(bytes, t, payload);
  if (!s.ok()) return CorruptErr("corrupt object " + id.hex() + ": " + s.msg);
  return Status::Ok();
}

Status LooseStore::write(ObjType t, const std::string& payload, Id* out) const {
  std::string bytes = objectHeader(t, payload.size()) + payload;
  Id id = sha256(bytes);
  if (out) *out = id;
  std::string path = pathFor(id);
  if (fsu::lstatPath(path).isReg) return Status::Ok();
  CHR_TRY(fsu::mkdirs(fsu::dirName(path)));
  CHR_TRY(fsu::mkdirs(dir_ + "/tmp"));
  std::string tmp = dir_ + "/tmp/obj-" + fsu::uniqueSuffix();
  CHR_TRY(fsu::writeFile(tmp, bytes, 0444));
  if (std::rename(tmp.c_str(), path.c_str()) != 0) {
    fsu::removeFile(tmp);
    return Err("cannot store object " + id.hex());
  }
  return Status::Ok();
}

Status LooseStore::list(std::vector<Id>* out) const {
  out->clear();
  std::vector<std::string> dirs;
  if (!fsu::isDir(dir_)) return Status::Ok();
  CHR_TRY(fsu::listDir(dir_, &dirs));
  for (const std::string& d : dirs) {
    if (d.size() != 2 || !isLowerHex(d)) continue;
    std::vector<std::string> files;
    CHR_TRY(fsu::listDir(dir_ + "/" + d, &files));
    for (const std::string& f : files) {
      Id id;
      if (f.size() == 62 && Id::fromHex(d + f, &id)) out->push_back(id);
    }
  }
  return Status::Ok();
}

Status LooseStore::remove(const Id& id) const {
  std::string p = pathFor(id);
  CHR_TRY(fsu::removeFile(p));
  fsu::removeEmptyParents(p, dir_);
  return Status::Ok();
}

int64_t LooseStore::mtimeSeconds(const Id& id) const {
  fsu::FileStat st = fsu::lstatPath(pathFor(id));
  return st.exists ? st.mtimeNs / 1000000000LL : -1;
}

}
