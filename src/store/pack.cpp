#include "store/pack.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>

#include "core/sha256.hpp"
#include "core/varint.hpp"
#include "ds/interpsearch.hpp"
#include "store/delta.hpp"
#include "store/packidx.hpp"
#include "util/fs.hpp"

namespace chr {

Pack::~Pack() {
  if (fd_ >= 0) close(fd_);
}

Status Pack::open(const std::string& packPath, const std::string& idxPath) {
  packPath_ = packPath;
  idxPath_ = idxPath;
  std::string idx;
  CHR_TRY(fsu::readFile(idxPath, &idx));
  Status s = decodePackIndex(idx, &index_);
  if (!s.ok()) return CorruptErr(idxPath + ": " + s.msg);
  fd_ = ::open(packPath.c_str(), O_RDONLY);
  if (fd_ < 0) return CorruptErr("cannot open pack " + packPath);
  size_ = fsu::lstatPath(packPath).size;
  std::string head;
  CHR_TRY(readAt(0, 12, &head));
  if (head.compare(0, 4, "CPK1") != 0) return CorruptErr(packPath + ": bad pack header");
  return Status::Ok();
}

Status Pack::readAt(uint64_t off, size_t len, std::string* out) const {
  if (off > size_ || len > size_ - off) return CorruptErr(packPath_ + ": read past end of pack");
  out->resize(len);
  size_t done = 0;
  while (done < len) {
    ssize_t n = pread(fd_, &(*out)[done], len - done, static_cast<off_t>(off + done));
    if (n <= 0) return CorruptErr(packPath_ + ": short read");
    done += static_cast<size_t>(n);
  }
  return Status::Ok();
}

bool Pack::find(const Id& id, uint64_t* offset) const {
  long i = interpolationSearch(index_, id);
  if (i < 0) return false;
  *offset = index_[static_cast<size_t>(i)].offset;
  return true;
}

static Status parseHeader(const std::string& head, uint8_t* kind, uint64_t* size, size_t* used) {
  if (head.empty()) return CorruptErr("pack: empty entry");
  *kind = static_cast<uint8_t>(head[0]);
  size_t pos = 1;
  if (*kind < kPackBlob || *kind > kPackDelta || !getVarint(head, pos, size)) return CorruptErr("pack: bad entry header");
  *used = pos;
  return Status::Ok();
}

Status Pack::readEntry(uint64_t offset, uint8_t* kind, std::string* data, Id* base) const {
  std::string head;
  CHR_TRY(readAt(offset, static_cast<size_t>(std::min<uint64_t>(11, size_ - offset)), &head));
  uint64_t size = 0;
  size_t used = 0;
  CHR_TRY(parseHeader(head, kind, &size, &used));
  uint64_t at = offset + used;
  if (*kind == kPackDelta) {
    std::string raw;
    CHR_TRY(readAt(at, 32, &raw));
    *base = Id::fromRaw(raw.data());
    at += 32;
  }
  return readAt(at, static_cast<size_t>(size), data);
}

Status Pack::entryInfo(uint64_t offset, uint8_t* kind, uint64_t* fullSize) const {
  std::string head;
  CHR_TRY(readAt(offset, static_cast<size_t>(std::min<uint64_t>(11, size_ - offset)), &head));
  size_t used = 0;
  CHR_TRY(parseHeader(head, kind, fullSize, &used));
  if (*kind == kPackDelta) {
    std::string d;
    CHR_TRY(readAt(offset + used + 32, static_cast<size_t>(std::min<uint64_t>(10, *fullSize)), &d));
    *fullSize = deltaResultSize(d);
  }
  return Status::Ok();
}

Status Pack::verifyTrailer() const {
  if (size_ < 44) return CorruptErr(packPath_ + ": too small");
  std::string all;
  CHR_TRY(readAt(0, static_cast<size_t>(size_), &all));
  if (!(sha256(all.data(), all.size() - 32) == Id::fromRaw(all.data() + all.size() - 32)))
    return CorruptErr(packPath_ + ": trailer checksum mismatch");
  return Status::Ok();
}

static uint8_t kindOf(ObjType t) {
  return t == ObjType::Blob ? kPackBlob : t == ObjType::Tree ? kPackTree : kPackCommit;
}

static void putU32(std::string& s, uint32_t v) {
  for (int i = 3; i >= 0; --i) s += static_cast<char>((v >> (8 * i)) & 0xff);
}

Status writePack(const std::string& packDir, std::vector<PackInput>& objs, const PackParams& params,
                 PackWriteStats* stats) {
  *stats = PackWriteStats();
  std::stable_sort(objs.begin(), objs.end(), [](const PackInput& a, const PackInput& b) {
    if (a.type != b.type) return a.type < b.type;
    if (a.nameHint != b.nameHint) return a.nameHint < b.nameHint;
    return a.payload.size() > b.payload.size();
  });
  std::vector<int> depth(objs.size(), 0);
  std::string out = "CPK1";
  putU32(out, 1);
  putU32(out, static_cast<uint32_t>(objs.size()));
  std::vector<IdEntry> index;
  for (size_t i = 0; i < objs.size(); ++i) {
    const PackInput& o = objs[i];
    std::string bestDelta;
    size_t bestBase = 0;
    bool haveDelta = false;
    size_t tried = 0;
    for (size_t j = i; j-- > 0 && tried < static_cast<size_t>(params.window);) {
      if (objs[j].type != o.type) break;
      ++tried;
      if (depth[j] >= params.maxDepth || objs[j].payload.empty()) continue;
      std::string d = makeDelta(objs[j].payload, o.payload);
      if (!haveDelta || d.size() < bestDelta.size()) {
        bestDelta.swap(d);
        bestBase = j;
        haveDelta = true;
      }
    }
    uint64_t limit = o.payload.size() * static_cast<uint64_t>(100 - params.minSaving) / 100;
    index.push_back({o.id, out.size()});
    if (haveDelta && bestDelta.size() < limit) {
      depth[i] = depth[bestBase] + 1;
      out += static_cast<char>(kPackDelta);
      putVarint(out, bestDelta.size());
      out.append(reinterpret_cast<const char*>(objs[bestBase].id.b), 32);
      out += bestDelta;
      ++stats->deltas;
    } else {
      out += static_cast<char>(kindOf(o.type));
      putVarint(out, o.payload.size());
      out += o.payload;
    }
    stats->fullBytes += objectHeader(o.type, o.payload.size()).size() + o.payload.size();
  }
  Id trailer = sha256(out);
  out.append(reinterpret_cast<const char*>(trailer.b), 32);
  stats->objects = objs.size();
  stats->packBytes = out.size();
  stats->name = "pack-" + trailer.hex();
  std::string idx = encodePackIndex(index);
  CHR_TRY(fsu::mkdirs(packDir));
  CHR_TRY(fsu::writeFileAtomic(packDir + "/" + stats->name + ".pack", out, 0444));
  CHR_TRY(fsu::writeFileAtomic(packDir + "/" + stats->name + ".idx", idx, 0444));
  return Status::Ok();
}

}
